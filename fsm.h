#pragma once
//std
#ifdef WIN32
#include <memory>
#else
#include <tr1/memory>
#endif

#include <set>
#include <map>
#include <list>
#include <string>
#include <utility>
#include <iostream>
//boost
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/asio/deadline_timer.hpp>

namespace XYH_StatusMachine {
    using std::set;
    using std::map;
    using std::list;	
    using std::pair;
    using std::string;
    using std::multimap;
    using std::tr1::shared_ptr;
    using std::tr1::enable_shared_from_this;
    using boost::asio::deadline_timer;

    class Xyh_Jsm;
    class Xyh_Status;

    /**
     说明：状态机事件；
          一个状态机上面可以同时存在多个事件，每个事件有不同的状态；
          当事件接受到信号时，将根据预设规则转入下一个状态，并根据
          信号类型执行相应的操作。
    */
    class Xyh_Event : public enable_shared_from_this<Xyh_Event> {
    public:
        /**
         描述：状态机事件构造函数
         参数：
           id：  事件id；事件必须拥有在状态机内唯一的id
           nick：事件别名
         返回值：无
        */
        Xyh_Event(unsigned int id, string nick) :
            m_id(id),
            m_nick(nick),
            m_stt(STT_SURVIVE) { }

        virtual ~Xyh_Event() { }

        /**
         描述：处理信号
         参数：
           signal：  驱动本次操作的信号
           msg：     附加消息；将被作为参数传递给信号处理函数
         返回值：无
        */
        void handle(unsigned int signal, const void* msg) throw (std::logic_error);

        /**
         描述：事件加入状态机的指定状态；
              该函数仅允许在事件首次加入状态机时使用；若事件已处于有效状态，再调用该方法将抛出异常
         参数：
           s：       要加入的状态
           signal：  驱动本次操作的信号
           msg：     附加消息；将被做为参数传递给信号处理函数
        */
        void place(shared_ptr<Xyh_Status> s, unsigned int signal, const void* msg) throw (std::logic_error);

        /**
         描述：标记事件即将过期；当进入允许回收事件的状态时，事件将被标记为过期；
              在执行完当前状态的信号处理操作后，状态机将不再处理该事件的任何信号
         参数：无
         返回值：无
        */
        void expire() { m_stt = STT_BEMARKED; }

        /**
         描述：标记事件为正常；若事件在此之前被标记为即将过期或已过期，
              事件将恢复正常状态，重新接收信号驱动
         参数：无
         返回值：无
        */
        void survive() { m_stt = STT_SURVIVE; }

        /**
         描述：判断事件是否已过期
         参数：无
         返回值：事件已过期返回true，否则返回false；
        */
        bool expired() { return m_stt == STT_RECYCLE; }

        /**
         描述：判断事件是否已被标记为即将过期
         参数：无
         返回值：若被标记返回true，否则返回false
        */
        bool marked() { return m_stt == STT_BEMARKED; }

        /**
         描述：重载小于符号，根据id比较两个事件的大小
         参数：另一个事件
         返回值：若id小于另一个事件返回true，否则返回false
        */
        bool operator<(const Xyh_Event & rhs);

    public: //辅助函数
         
        /**
         描述：获取当前时间的id
         参数：无
         返回值：返回当前事件的id
        */
        unsigned int getId() { return m_id; }

        /**
         描述：获取当前事件的别名
         参数：无
         返回值：返回当前时间的别名
        */
        string getNick() { return m_nick; }

        /**
         描述：获取事件当前状态
         参数：无
         返回值：返回当前状态
        */
        shared_ptr<Xyh_Status> getCurrentStatus();

        /**
         描述：设置事件当前状态
         参数：
           s：要设置的状态
         返回值：无
        */
        void setCurrentStatus(shared_ptr<Xyh_Status> s);

        /**
         描述：获取进入当前状态的时间，单位为s
         参数：无
         返回值：返回事件进入当前状态的时间
        */
        unsigned long long enterTime() { return m_enterTime; }

        /**
         描述：事件运行状态枚举
        */
        enum {
            STT_SURVIVE     = 0,
            STT_BEMARKED    = 1,    //被标记即将过期；
            STT_RECYCLE     = 2     //已过期；不应再继续使用，状态机不再处理该事件的信号
        };

    private:
        friend class Xyh_Jsm;

        /**
         描述：将时间从当前状态转移到指定状态；转移路线必须符合状态机设定
         参数：
           s：       本次转移的目的状态
           signal：  驱动本次操作的信号
           msg：     附加消息；将被做为参数传递给信号处理函数
         返回值：无
        */
        void move(shared_ptr<Xyh_Status> s, unsigned int signal, const void* msg);

        /**
         描述：设置进入当前状态的时间
         参数：
           t：当前时间戳
         返回值：
        */
        void enterTime(unsigned long long t) { m_enterTime = t; }

    private:
        //事件Id
        unsigned int m_id;

        //时间别名
        string m_nick;

        //event的当前状态，STT_BEMARKED或STT_RECYCLE
        unsigned char m_stt;

        //进入当前状态的时间
        volatile long long m_enterTime;

        //当前status
        shared_ptr<Xyh_Status> m_curStatus;
    };


    /**
     说明：状态机状态；
          状态机由若干个状态构成，事件可以在状态和状态之间根据预设好的路线转移
    */
    class Xyh_Status : public enable_shared_from_this<Xyh_Status> {
    public:
        /**
         描述：构造函数
         参数：
           id：  状态id；状态必须拥有状态机内唯一的id
           name：状态别名
           fade：状态是否允许回收事件；当被标记为即将过期的时间进入可回收事件的状态后，将被标记为已过期
         返回值：无
        */
        Xyh_Status(unsigned int id, string name, bool fade = false);

        virtual ~Xyh_Status() { };

        /**
         描述：信号处理函数；
              派生类应该重写该方法；当事件进入状态后会立即执行该处理函数
         参数：
           e：       进入状态的事件
           label：   驱动事件进入该状态的信号
           msg：     附加信息
         返回值：无
        */
        virtual void routine(shared_ptr<Xyh_Event>& e, unsigned int lable, const void *msg) { }

        /**
         描述：超时处理函数；派生类应该重写该方法；
              若状态由regular方法设置了定时操作，则每当有一个新的事件进入状态时，即开始计时；
              超时后将调用该方法
         参数：
           label：触发超时的信号，在调用regular方法时指定
           e：    进入状态的事件
         返回值：无
        */
        virtual void timerRoutine(unsigned int label, shared_ptr<Xyh_Event>& e) { }

        /**
         描述：根据参数以及预定的转移路线，查找下一个状态
         参数：
           signal：信号
         返回值：若有符合条件的转移路线，则返回正确的状态；否则返回空指针
        */
        shared_ptr<Xyh_Status> route(unsigned int signal);

        /**
         描述：设定以当前状态为起始点的转移路线；
              同一个状态上，一个信号只能设置一条转移路线；若使用同一个信号重复设置，
              则只有最后一次设置生效
         参数：
           signal：触发信号
           status：目的状态
         返回值：无
        */
        void addLink(unsigned int signal, shared_ptr<Xyh_Status>& status);

        /**
         描述：设定当前状态上的自环转移路线；同一个状态上，一个信号只能设置一次转移路线，
              若同时设置了自环和非自环转移，则只有自环转移生效
         参数：
           signal：触发信号
         返回值：无
        */
        void addLink(unsigned int signal);

        /**
         描述：添加一个定时事件
              当时间进入状态时，即开始计时，在period秒后触发超时；状态机将调用状态的
              timerRoutine方法，并将label的值作为参数传递进去
         参数：
           label：   超时调用timerRoutine时传递的信号参数
           period：  超时时长
         返回值：无
        */
        void regular(unsigned int label, unsigned int period);

        /**
         描述：从当前状态移除指定事件
         参数：
           e：要移除的事件
         返回值：无
        */
        void removeEvent(shared_ptr<Xyh_Event>& e);

        /**
         描述：向当前状态添加一个事件
         参数：
           e：要添加的时间
         返回值：无
        */
        void addEvent(shared_ptr<Xyh_Event>& e);

    public:
        /**
         描述：事件比较类
        */
        class compare {
        public:
            compare(shared_ptr<Xyh_Event>& s);

            bool operator()(shared_ptr<Xyh_Event>& e);

        private:
            shared_ptr<Xyh_Event>& _s;

        };

        /**
         描述：获取状态id
         参数：无
         返回值：当前状态的id
        */
        unsigned int getId() { return m_Id; }

        /**
         描述：设置状态所属状态机的id
         参数：
           id：状态机id
         返回值：无
        */
        void setOwnerId(unsigned int id) { m_owner = id; }

        /**
         描述：获取状态所属状态机的id
         参数：无
         返回值：状态机id
        */
        unsigned int gettOwnerId() { return m_owner; }

        /**
         描述：获取自状态机创建起经过的时间，单位为秒
         参数：无
         返回值：自状态机创建起经过的时间，单位为秒
        */
        unsigned long long timestamp() { return m_ticktock; }

        /**
         描述：判断当前状态是否允许回收事件
         参数：无
         返回值：允许返回true，否则返回false
        */
        bool fade() { return m_fade; }

    private:

        struct _InStore {
            _InStore(unsigned int tabel, shared_ptr<Xyh_Event>& e) : _tabel(tabel), _event(e) { }

            unsigned int _tabel;
            shared_ptr<Xyh_Event> _event;
        };

        friend class Xyh_Jsm;

        /**
         描述：时钟嘀嗒处理方法
         参数：
           ticktock：状态机创建开始经过的时间,单位为s
           e：       状态机定时器错误码
         返回值：无
        */
        void ticktock(const unsigned long long ticktock, const boost::system::error_code& e);

    protected:
        //当前状态下的事件列表
        list<shared_ptr<Xyh_Event> > m_listEvent;

    private:
        //状态ID，在一个状态机中唯一
        unsigned int m_Id;

        //当前状态归属的状态机id
        unsigned int m_owner;

        //状态别名
        string m_name;

        //是否允许event在该状态被回收
        bool m_fade;

        //暂存当前滴答数
        volatile unsigned long long m_ticktock;

        //定时规则 <table, period>
        list<std::pair<unsigned int, unsigned int> > m_regularEvt;

        //定时事件
        multimap<unsigned int, shared_ptr<_InStore> > m_mapTEvent;

        //转移规则 <signal, status>
        map<unsigned int, shared_ptr<Xyh_Status> > m_mapLink;

        //自环信号集合
        set<unsigned int> m_setSelfLink;
    };


    typedef boost::function<void(const unsigned int)> FinishNotify;

    /**
     描述：状态机
          状态机由若干个状态以及若干状态与状态之间的转移关于则组成；
          事件在状态与状态之间独立转移，互不影响
    */
    class Xyh_Jsm {
    public:
        /**
         描述：构造函数
         参数：
           id:          全局唯一的状态机id，用于区分不同的状态机
           io_service:  boost库的io_service对象
           fr:          结束通知
        */
        Xyh_Jsm(unsigned int id, boost::asio::io_service &io_servivce/*, FinishNotify fr = 0*/);

        /**
         描述：析构函数
         参数：无
         返回值：无
        */
        virtual ~Xyh_Jsm() {};

        /**
         描述：驱动指定事件处理信号；若信号是自环信号，该方法将不触发定时事件
         参数：
           event:   事件id
           signal:  信号
           msg:     附加信息；作为参数传递给状态的信号处理方法
         返回值：无
        */
        void digestion(unsigned int event, unsigned int signal, const void* msg);

        /**
         描述：驱动指定事件处理信号；若信号是自环信号，该方法将在触发定时事件
         参数：
           event:   事件id
           signal:  信号
           msg:     附加信息；作为参数传递给状态的信号处理方法
         返回值：无
        */
        void process(unsigned int event, unsigned int signal, const void* msg);

        /**
         描述：处理信号；状态机内除已过期事件之外的所有事件都将收到该信号
         参数：
           signal：  信号
           msg：     附加信息
         返回值：无
        */
        void process(unsigned int signal, const void* msg);

        /**
         描述：向状态机添加状态
         参数：
           s：状态
         返回值：无
        */
        void addStatus(shared_ptr<Xyh_Status> s);
        
        /**
         描述：查找状态
         参数：
           id：状态id
         返回值：若存在返回状态，否则返回空指针
        */
        shared_ptr<Xyh_Status> findStatus(unsigned int id);

        /**
         描述：添加事件
         参数：
           e：事件
         返回值：无
        */
        void addEvent(shared_ptr<Xyh_Event> e);
        
        /**
         描述：删除事件
         参数：
           id：事件id
         返回值：无
        */
        void relEvent(unsigned int id);

        /**
         描述：标记事件即将过期
         参数：
           id：事件id
         返回值：无
        */
        void expireEvent(unsigned int id);

        /**
         描述：查找事件
         参数：
           id：事件id
         返回值：若存在返回事件，否则返回空指针
        */
        shared_ptr<Xyh_Event> findEvent(unsigned int id);
        
        /**
         描述：结束通知
         参数：
           id：
         返回值：
        */
	    //void notifyFinish(const unsigned int);

        /**
         描述：停止状态机
         参数：无
         返回值：无
        */
        void stop();

    private:
        /**
         描述：时钟嘀嗒处理方法
         参数：
           e：定时器错误码
         返回值：无
        */
        void ticktock(const boost::system::error_code& e);
       
    private:
	    boost::asio::io_service& m_ioService;

        //状态机内所有状态列表 <statusId, Status>
        map<unsigned int, shared_ptr<Xyh_Status> > m_mapStatus;

        //状态机内所有事件列表 <eventId, Event>
        map<unsigned int, shared_ptr<Xyh_Event> >  m_mapEvent;

        //定时器
        deadline_timer m_timer;

        //时钟滴答数
        volatile unsigned long long m_ticktock;
    };

} //namespace XYH_StatusMachine
