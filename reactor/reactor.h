#pragma once
#include<iostream>
#include<map>
#include<vector>
#include<sys/epoll.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string.h>
#define USE_SELECT 
using Handle = int;
using timeout = int;
using port = int;
class ReactorImplementation;
class EventDemultiplexer;
enum Event
{
	ReadEvent = 0,
	WriteEvent = 1,
	ErrorEvent = 2

};
class EventHandler
{
public:
	virtual Handle get_handle() = 0;
	virtual void handle_read() = 0;
	virtual void handle_write() = 0;
	virtual void handle_error() = 0;
};
class Reactor
{
public:
	static Reactor& get_instance();
	
	int regist(EventHandler* eventhandler, Event e);
	
	void remove(EventHandler* eventhandler);
	
	void event_loop(timeout t);
	

private:

	ReactorImplementation* impl;
	static Reactor reactor;
	Reactor();
	~Reactor();

};



class ReactorImplementation
{
public:
	ReactorImplementation();
	~ReactorImplementation();
	int regist(EventHandler*, Event);
	void remove(EventHandler*);
	void event_loop(timeout);
private:
	EventDemultiplexer* demultiplexer;
	std::map<Handle, EventHandler*> handlers;
};


class EventDemultiplexer
{
public:
	virtual int wait_event(std::map<Handle, EventHandler*> handlers, timeout) = 0;
	virtual int regist(EventHandler*, Event) = 0;
	virtual int remove(Handle) = 0;
	virtual ~EventDemultiplexer() = default;
};

class EpollDemultiplexer :public EventDemultiplexer {
public:
	int wait_event(std::map<Handle, EventHandler*>handlers, timeout) override;
	int regist(EventHandler*, Event) override;
	int remove(Handle) override;
	EpollDemultiplexer();
	~EpollDemultiplexer();
private:
	int epoll_fd;
	std::vector<epoll_event> evs;
	const int max_event = 1024;	
};
class SelectDemultiplexer :public EventDemultiplexer {
public:
	int wait_event(std::map<Handle, EventHandler*>handlers, timeout)override;
	int regist(EventHandler*, Event) override;
	int remove(Handle)override;
	SelectDemultiplexer();
	~SelectDemultiplexer();

private:
	fd_set read_set;
	fd_set write_set;
	fd_set err_set;

};

class ListenHandle :public EventHandler {
public:
	Handle get_handle() override;
	void handle_read()override;
	void handle_write()override;
	void handle_error()override;
	bool listenOn(port);
	ListenHandle();
	~ListenHandle();
private:
	Handle listenfd;
};

class SockHandle :public EventHandler {
public:
	Handle get_handle() override;
	void handle_read() override;
	void handle_write() override;
	void handle_error() override;
	SockHandle(Handle fd);
	~SockHandle();
private:
	Handle sock_fd;
	char* buf;
	int MAX_SIZE = 1024;
};
