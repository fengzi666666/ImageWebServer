#include<stdio.h>
#include<stdlib.h>
#include<string.h>
// #include<sys/socket.h>
// #include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include"locker.h"
#include"threadpool.h"
#include<signal.h>
#include"http_conn.h"

#define MAX_FD 65535//最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 //监听最大的文件数量


//添加信号捕捉
void addsig(int sig,void(handler)(int)){//sig要捕捉的信号 handler要处理的函数
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));//清空
    sa.sa_handler=handler;//信号处理函数为handler
    sigfillset(&sa.sa_mask);//用来将参数set信号集初始化，然后把所有的信号加入到此信号集里即将所有的信号标志位置为1，屏蔽所有的信号
    sigaction(sig,&sa,NULL);//检查或修改与指定信号相关联的处理动作  建立信号与信号处理函数之间的联系

}

//添加文件描述符到epoll对象中
extern void addfd(int epollfd,int fd,bool one_shot);
//从epoll中删除文件描述符
extern void removefd(int epollfd,int fd);
//修改文件描述符
extern void modfd(int epollfd,int fd,int ev );

int main(int argc,char* argv[]){
    if(argc<=1){//至少要传递一个参数 否则只有直接运行的命令
        printf("按照如下格式运行：%s port_number\n",basename(argv[0]));//去除路径和文件后缀部分的文件名或者目录名
        exit(-1);
    }

    //获取端口号
    int port=atoi(argv[1]);//字符串（const char*）转整数
     
    //对SIGPIE信号进行处理
    addsig(SIGPIPE,SIG_IGN);//收到SIGPIPE信号 忽略 当一个进程向某个已收到RST的套接字执行写操作时，内核向该进程发送SIGPIPE信号

    //创建线程池 初始化线程池
    threadpool<http_conn>* pool=NULL;//http连接的任务
    try{
        pool=new threadpool<http_conn>;
    }catch(...){//创建线程池失败 退出
        exit(-1);
    }

    //创建一个数组  用于保存所有客户端的信息
    http_conn * users=new http_conn[MAX_FD];

    //
    //创建套接字
    int listenfd=socket(PF_INET,SOCK_STREAM,0);


    //设置端口复用  端口复用最常用的用途是:
        // 防止服务器重启时之前绑定的端口还未释放
        // 程序突然退出而系统没有释放端口
    int reuse=1;//设置为1才是端口复用
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    //绑定
    struct sockaddr_in address;
    address.sin_family=AF_INET;//协议族 IPv4互联网协议族
    address.sin_addr.s_addr=INADDR_ANY;//IP地址
    address.sin_port=htons(port);//端口
    
    bind(listenfd,(struct sockaddr*)&address,sizeof(address));


    //监听
    listen(listenfd,5);

    //创建epoll对象 时间数组 添加
    //epoll_create函数生成一个epoll专用的文件描述符。
    //它其实是在内核申请一空间，用来存放你想关注的socket fd上是否发生以及发生了什么事件。
    //size就是你在这个epoll fd上能关注的最大socket fd数。
    epoll_event evens[MAX_EVENT_NUMBER];//监听数组
    
    int epollfd=epoll_create(5);//创建epoll对象
    
    //将监听的文件描述符添加到epoll对象中
    addfd(epollfd,listenfd,false);
    http_conn::m_epollfd=epollfd;

    while(true){
        int num=epoll_wait(epollfd,evens,MAX_EVENT_NUMBER,-1);//等待在epoll文件描述符的I/O事件
        if(num<0&&(errno!=EINTR)){//epoll_wait调用失败
            printf("epoll failure\n");
            break;
        }

        //循环遍历事件数组
        for(int i=0;i<num;i++){
            int sockfd=evens[i].data.fd;
            if(sockfd==listenfd){
                //有客户端连接进来

                struct sockaddr_in client_address;//定义客户端地址
                socklen_t client_addrlen=sizeof(client_address);//长度
                //连接客户端
                int connfd=accept(sockfd,(struct sockaddr*)&client_address,&client_addrlen);

                if(http_conn::m_user_count>=MAX_FD){//用户数量大于最大的文件描述符
                    //目前连接数满了
                    //给客户端一个信息，服务器内部正忙
                    close(connfd);
                    continue;
                }

                //将新的客户初始化 放到数组当中
                users[connfd].init(connfd,client_address);
            }else if(evens[i].events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR)){//EPOLLHUP读写关闭 EPOLLRDHUP读关闭 出错
                //对方异常断开或者错误事件
                //关闭连接
                users[sockfd].close_conn();
            }else if(evens[i].events&EPOLLIN){//读事件
                if(users[sockfd].read()){//读取成功
                    //一次性把所有数据都读完
                    pool->append(users+sockfd);
                }else{//读取失败
                    users[sockfd].close_conn();
                }
            }else if(evens[i].events&EPOLLOUT){//写事件
                if(!users[sockfd].write()){//一次性写完所有数据
                    users[sockfd].close_conn();//写数据失败
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete[]users;
    delete pool;

    return 0;
}