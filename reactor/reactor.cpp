#include"reactor.h"

Reactor Reactor::reactor;

Reactor& Reactor::get_instance()
{
	return reactor;
}
int Reactor::regist(EventHandler* h, Event e)
{
	return impl->regist(h, e);
}
void Reactor::remove(EventHandler*h)
{
	return impl->remove(h);
}
void Reactor::event_loop(timeout t)
{
	return impl->event_loop(t);
}
Reactor::Reactor()
{
	impl = new ReactorImplementation();
}
Reactor::~Reactor()
{
	delete impl;
	impl = nullptr;
}

ReactorImplementation::ReactorImplementation()
{
#ifdef USE_EPOLL
	demultiplexer = new EpollDemultiplexer();
#else
	demultiplexer = new SelectDemultiplexer();
#endif
}
ReactorImplementation::~ReactorImplementation()
{
	delete demultiplexer;
}
int ReactorImplementation::regist(EventHandler* handler, Event event)
{
	Handle fd = handler->get_handle();
	handlers[fd] = handler;
	return demultiplexer->regist(handler, event);
}
void ReactorImplementation::remove(EventHandler* handler)
{
	Handle fd = handler->get_handle();
	demultiplexer->remove(fd);
	handlers.erase(fd);
}

void ReactorImplementation::event_loop(timeout timeout)
{
	while (true)
	{
		demultiplexer->wait_event(handlers, timeout);
	}
}




EpollDemultiplexer::EpollDemultiplexer()
{
	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1)
	{
		printf("epollcreate error %d %s", errno, strerror(errno));

		return;
	}
	evs.resize(max_event);
}
EpollDemultiplexer::~EpollDemultiplexer()
{
	close(epoll_fd);
}
int EpollDemultiplexer::regist(EventHandler* handler, Event e)
{
	epoll_event ev{};
	ev.data.fd = handler->get_handle();
	ev.events = 0;
	if (e == ReadEvent)
	{
		ev.events = EPOLLIN;
	}
	if (e == WriteEvent)
	{
		ev.events = EPOLLOUT;
	}

	return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
}

int EpollDemultiplexer::remove(Handle h)
{
	return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, h, nullptr);
}
int EpollDemultiplexer::wait_event(std::map<Handle, EventHandler*> handlers, timeout timeout) {
	int n = epoll_wait(epoll_fd, evs.data(), max_event, timeout);
	for (int i = 0; i < n; ++i) {
		Handle fd = evs[i].data.fd;
		if (handlers.count(fd)) {
			if (evs[i].events & EPOLLIN)  handlers[fd]->handle_read();
			if (evs[i].events & EPOLLOUT) handlers[fd]->handle_write();
			if (evs[i].events & (EPOLLERR | EPOLLHUP)) handlers[fd]->handle_error();
		}
	}
	return n;
}


SelectDemultiplexer::SelectDemultiplexer()
{
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
}

int SelectDemultiplexer::regist(EventHandler* handler, Event e)
{
	Handle fd = handler->get_handle();
	if (e == ReadEvent)
		FD_SET(fd, &read_set);
	if (e == WriteEvent)
		FD_SET(fd, &write_set);
	return 0;
}

int SelectDemultiplexer::remove(Handle handle) 
{
	FD_CLR(handle, &read_set);
	FD_CLR(handle, &write_set);
	return 0;
}
int SelectDemultiplexer::wait_event(std::map<Handle, EventHandler*> handlers, timeout timeout)
{
	fd_set r = read_set, w = write_set, e = err_set;

	timeval tv{};
	if (timeout >= 0) {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	// 找到最大 fd，用于 select 第一个参数
	Handle maxfd = 0;
	for (auto& pair : handlers) maxfd = std::max(maxfd, pair.first);

	int n = select(maxfd + 1, &r, &w, &e, timeout < 0 ? nullptr : &tv);

	// 遍历所有 handler，分发事件
	for (auto& pair : handlers) {
		Handle fd = pair.first;
		EventHandler* h = pair.second;

		if (FD_ISSET(fd, &r)) h->handle_read();
		if (FD_ISSET(fd, &w)) h->handle_write();
		if (FD_ISSET(fd, &e)) h->handle_error();
	}
	return n;
}
SelectDemultiplexer::~SelectDemultiplexer()
{
	;
}


//listenhandle实现
ListenHandle::ListenHandle()
{
	listenfd = -1;
}
ListenHandle::~ListenHandle()
{
	close(listenfd);
}
Handle ListenHandle::get_handle()
{
	return listenfd;
}
bool ListenHandle::listenOn(port p)
{
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in server_add;
	server_add.sin_family = AF_INET;
	server_add.sin_port = htons(p);
	server_add.sin_addr.s_addr = INADDR_ANY;

	if (bind(listenfd, (sockaddr*)&server_add, sizeof(server_add)) == -1)
	{
		printf("bind error %d %s", errno, strerror(errno));
		
		return false;
	}
	if (listen(listenfd, 10) == -1)
	{
		printf("listen error %d %s", errno, strerror(errno));

		return false;
	}
	return true;
}
void ListenHandle::handle_read()
{
	int clientfd = accept(listenfd, NULL, NULL);
	if (clientfd == -1)
	{
		printf("accept error %d %s", errno, strerror(errno));

		return ;
	}
	EventHandler* handle = new SockHandle(clientfd);
	Reactor::get_instance().regist(handle, ReadEvent);
}
void ListenHandle::handle_write()
{
	;
}
void ListenHandle::handle_error()
{
	close(listenfd);
}



//SockHandle实现
SockHandle::SockHandle(Handle fd)
{
	sock_fd = fd;
	buf = new char[MAX_SIZE];
}
SockHandle::~SockHandle()
{
	close(sock_fd);
	delete[] buf;
}
Handle SockHandle::get_handle()
{
	return sock_fd;
}
void SockHandle::handle_read()
{
	int count = read(sock_fd, buf, MAX_SIZE);
	if (count <= 0)
	{
		printf("read error %d %s", errno, strerror(errno));
		handle_error();
		return;
	}
	write(sock_fd, buf, count);
	memset(buf, 0, MAX_SIZE);
}
void SockHandle::handle_write()
{
	;
}
void SockHandle::handle_error()
{
	Reactor::get_instance().remove(this);
	close(sock_fd);
	return;
}