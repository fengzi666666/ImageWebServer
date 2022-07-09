#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

// 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类
template<typename T>
class threadpool {
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void* worker(void* arg);////要运行的线程  静态函数
    void run();//运行线程

private:
    // 线程的数量
    int m_thread_number;  
    
    // 描述线程池的数组，大小为m_thread_number    
    pthread_t * m_threads;

    // 请求队列中最多允许的、等待处理的请求的数量  
    int m_max_requests; 
    
    // 请求队列
    std::list< T* > m_workqueue;  

    // 保护请求队列的互斥锁
    locker m_queuelocker;   

    // 是否有任务需要处理 信号量
    sem m_queuestat;

    // 是否结束线程          
    bool m_stop;                    
};

template< typename T >
threadpool< T >::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests), 
        m_stop(false), m_threads(NULL) {
    //判断线程数和队列最多允许的请求数量
    if((thread_number <= 0) || (max_requests <= 0) ) {
        throw std::exception();
    }

    //创建线程池数组
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) {
        throw std::exception();
    }

    // 创建thread_number 个线程，并将他们设置为脱离线程。
    for ( int i = 0; i < thread_number; ++i ) {
        printf( "create the %dth thread\n", i);
         //若线程创建失败
        if(pthread_create(m_threads + i, NULL, worker, this ) != 0) {
            delete [] m_threads;
            throw std::exception();
        }
        // pthread_t *restrict tidp,   //新创建的线程ID指向的内存单元。
        // const pthread_attr_t *restrict attr,  //线程属性，默认为NULL  设置线程分离
        // void *(*start_rtn)(void *), //新创建的线程从start_rtn函数的地址开始运行  静态函数
        // void *restrict arg //默认为NULL。若上述函数需要参数，将参数放入结构中并将地址作为arg传入。

        //线程创建成功  设置线程分离
        if( pthread_detach( m_threads[i] ) ) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template< typename T >
threadpool< T >::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}



//队列添加任务保证线程同步
template< typename T >
bool threadpool< T >::append( T* request )
{
    // 操作工作队列时一定要加锁，因为它被所有线程共享。
    //上锁
    m_queuelocker.lock();
    //请求队列数量大于最大量
    if ( m_workqueue.size() > m_max_requests ) {
        //解锁
        m_queuelocker.unlock();
        return false;
    }
     //将请求加入到队列
    m_workqueue.push_back(request);
    //解锁
    m_queuelocker.unlock();
    //信号量增加
    m_queuestat.post();
    return true;
}

template< typename T >
void* threadpool< T >::worker( void* arg )
{
    threadpool* pool = ( threadpool* )arg;
    pool->run();//运行线程
    return pool;
}

template< typename T >
void threadpool< T >::run() {

    while (!m_stop) {//线程一直运行 直到要结束线程
        m_queuestat.wait();//等待信号量  有值就去工作  没值就阻塞
        m_queuelocker.lock();//上锁
        if ( m_workqueue.empty() ) {//队列没有工作则解锁
            m_queuelocker.unlock();
            continue;
        }
        //有工作取出链表头部的工作
        T* request = m_workqueue.front();
        m_workqueue.pop_front();//删掉工作  
        m_queuelocker.unlock();//解锁
        if ( !request ) {//若没获取到工作继续循环
            continue;
        }
        request->process();//工作运行
    }

}

#endif
