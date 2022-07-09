#ifndef LOCKER_H
#define LOCKER_H
#include<pthread.h>
#include<exception>//异常
#include<semaphore.h>//信号量

//线程同步机制封装类


//互斥锁类
class locker{
public:
    locker(){
        if(pthread_mutex_init(&m_mutex,NULL)!=0){//默认初始化互斥锁 不为0说明出错
            throw std::exception();//抛出异常对象
        }
    }

    ~locker(){
        pthread_mutex_destroy(&m_mutex);//销毁互斥锁
    }

    bool lock(){//加互斥锁  
        return pthread_mutex_lock(&m_mutex)==0;
    }

    bool unlock(){//解互斥锁
        return pthread_mutex_unlock(&m_mutex)==0;
    }

    pthread_mutex_t* get(){
        return &m_mutex;//返回互斥锁
    }
private:
    pthread_mutex_t m_mutex;//互斥锁
};

//条件变量类
class cond{
public:
    cond(){
        if(pthread_cond_init(&m_cond,NULL)!=0){//初始化条件变量
            throw std::exception();
        }
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);//销毁条件变量
    }
    bool wait(pthread_mutex_t *mutex){
        //用于阻塞当前线程，等待别的线程使用pthread_cond_signal()或pthread_cond_broadcast来唤醒它
        //函数一进入wait状态就会自动释放互斥锁，在唤醒后又获得互斥锁
        return pthread_cond_wait(&m_cond,mutex)==0;
    }
    bool timedwait(pthread_mutex_t *mutex,struct timespec t){//超时等待
        return pthread_cond_timedwait(&m_cond,mutex,&t)==0;
    }
    bool sinal(){//发送一个信号给另外一个正在处于阻塞等待状态的线程,使其脱离阻塞状态,继续执行
        return pthread_cond_signal(&m_cond)==0;
    }
    bool broadcast(){//将所有线程都唤醒
        return pthread_cond_broadcast(&m_cond)==0;
    }
private:
    pthread_cond_t m_cond;

};


//信号量类
class sem{
public:
    sem(){
        if(sem_init(&m_sem,0,0)!=0){
            throw std::exception();
        }
    }
    sem(int num){
        if(sem_init(&m_sem,0,num)!=0){
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }
    bool wait(){//等待信号量 信号量减1
        return sem_wait(&m_sem)==0;
    }
    bool post(){//释放信号量 +1 当有线程阻塞在这个信号量上时，调用这个函数会使其中的一个线程不在阻塞
        return sem_post(&m_sem)==0;
    }
private:
    sem_t m_sem;
};
#endif