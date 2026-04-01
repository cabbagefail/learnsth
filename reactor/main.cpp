#include "reactor.h"

int main()
{
    // 1. 创建监听句柄
    ListenHandle listen_handler;

    // 2. 监听 8888 端口
    if (!listen_handler.listenOn(8888))
    {
        std::cout << "server start fail" << std::endl;
        return -1;
    }
    std::cout << "server start now,port is 8888  " << std::endl;

    // 3. 把监听 fd 注册到 Reactor，监听读事件（有连接就触发）
    Reactor::get_instance().regist(&listen_handler, ReadEvent);

    // 4. 启动反应堆事件循环（-1 表示永久阻塞）
    Reactor::get_instance().event_loop(-1);

    return 0;
}