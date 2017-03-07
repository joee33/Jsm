//local
#include "fsm.h"
//std
#include <algorithm>
#include <exception>
#include <sstream>
//boost
#include "boost/bind.hpp"

namespace XYH_StatusMachine {

    void Xyh_Event::handle(unsigned int signal, const void* msg) throw (std::logic_error) {
        //过期的event不再处理
        if (expired()) { return; }

        shared_ptr<Xyh_Status> ns = m_curStatus->route(signal);
        if (ns) {
            move(ns, signal, msg);
        }
        else {
            std::stringstream ss;
            ss << "not found link trigger by signal(" << signal
                << "). curren status(" << m_curStatus->getId()
                << ")";
            throw std::logic_error(ss.str());
        }
    }

    void Xyh_Event::place(shared_ptr<Xyh_Status> s, unsigned int signal, const void* msg) throw (std::logic_error) {
        if (expired()) {
            throw std::logic_error("event has expired");
        }

        if (m_curStatus) {
            throw std::logic_error("event has valid status");
        }

        shared_ptr<Xyh_Event> self = shared_from_this();

        setCurrentStatus(s);

        enterTime(s->timestamp());

        if (marked() && s->fade()) {
            m_stt = STT_RECYCLE;
        }
        else {
            s->addEvent(self);
        }

        s->routine(self, signal, msg);
    }

    bool Xyh_Event::operator<(const Xyh_Event & rhs) {
        return m_id < rhs.m_id;
    }

    shared_ptr<Xyh_Status> Xyh_Event::getCurrentStatus() {
        return m_curStatus;
    }

    void Xyh_Event::setCurrentStatus(shared_ptr<Xyh_Status> s) {
        m_curStatus = s;
    }

    void Xyh_Event::move(shared_ptr<Xyh_Status> s, unsigned int signal, const void* msg) {
        //过期的event不再处理
        if (expired()) { return; }

        shared_ptr<Xyh_Event> self = shared_from_this();

        if (m_curStatus) {
            m_curStatus->removeEvent(self);
        }

        setCurrentStatus(s);

        enterTime(s->timestamp());

        //若时间已被标记过期，且当前状态允许停止事件则讲event状态置为待回收
        if (marked() && s->fade()) {
            m_stt = STT_RECYCLE;
        }
        else {
            s->addEvent(self);
        }

        //执行
        s->routine(self, signal, msg);
    }


    Xyh_Status::Xyh_Status(unsigned int id, string name, bool fade) :
        m_Id(id),
        m_owner(0),
        m_name(name),
        m_fade(fade),
        m_ticktock(0) {
    }

    shared_ptr<Xyh_Status> Xyh_Status::route(unsigned int signal) {
        if (1 == m_setSelfLink.count(signal)) {
            return shared_from_this();
        }
        else {
            if (m_mapLink.end() != m_mapLink.find(signal)) {
                return m_mapLink[signal];
            }
            else {
                return shared_ptr<Xyh_Status>();
            }
        }
    }

    void Xyh_Status::addLink(unsigned int signal, shared_ptr<Xyh_Status>& status) {
        m_mapLink.insert(std::make_pair(signal, status));
    }

    void Xyh_Status::addLink(unsigned int signal) {
        m_setSelfLink.insert(signal);
    }

    void Xyh_Status::regular(unsigned int tabel, unsigned int period) {
        m_regularEvt.push_back(std::pair<unsigned int, unsigned int>(tabel, period));
    }

    void Xyh_Status::ticktock(const unsigned long long ticktock, const boost::system::error_code& e) {
        if (e) {
            std::cout << "Xyh_Status::_ticktock. timer be cancel(" << m_Id << ")" << e.message() << std::endl;
            return;
        }

        m_ticktock = ticktock;

        //处理到期事件
        typedef map<unsigned int, shared_ptr<_InStore> >::iterator ItType;
        std::pair<ItType, ItType> r = m_mapTEvent.equal_range(m_ticktock);
        for (ItType it = r.first; it != r.second; it++) {
            //若event当前所在status Id与m_id 不相等，说明event已转移，忽略超时
            if (!it->second->_event->expired() &&
                (it->second->_event->getCurrentStatus()->getId() == m_Id)) {
                timerRoutine(it->second->_tabel, it->second->_event);
            }
        }
        //清除到期事件
        m_mapTEvent.erase(m_ticktock);
    }

    Xyh_Status::compare::compare(shared_ptr<Xyh_Event>& s) : _s(s) {}

    bool Xyh_Status::compare::operator()(shared_ptr<Xyh_Event>& e) {
        return e->getId() == _s->getId();
    }

    void Xyh_Status::removeEvent(shared_ptr<Xyh_Event>& s) {
        m_listEvent.remove_if(compare(s));
    }

    void Xyh_Status::addEvent(shared_ptr<Xyh_Event>& s) {
        m_listEvent.push_back(s);
        typedef list<std::pair<unsigned int, unsigned int> >::value_type VType;
        BOOST_FOREACH(VType v, m_regularEvt) {
            typedef multimap<unsigned int, shared_ptr<_InStore> >::value_type MVType;
            m_mapTEvent.insert(MVType(m_ticktock + v.second, shared_ptr<_InStore>(new _InStore(v.first, s))));
        }
    }


    Xyh_Jsm::Xyh_Jsm(unsigned int _id, boost::asio::io_service & _io_Servivce) :
	    m_ioService(_io_Servivce),
        m_timer(_io_Servivce),
        m_ticktock(0) {

        m_timer.expires_from_now(boost::posix_time::seconds(1));
        m_timer.async_wait(boost::bind(&Xyh_Jsm::ticktock, this, _1));
    }

    void Xyh_Jsm::digestion(unsigned int eid, unsigned int sig, const void* msg) {
        shared_ptr<Xyh_Event> event = findEvent(eid);
        if (event) {
            //过期的event不再处理
            if (event->expired()) {
                return;
            }

            shared_ptr<XYH_StatusMachine::Xyh_Status> cS = event->getCurrentStatus();

            shared_ptr<XYH_StatusMachine::Xyh_Status> nS = cS->route(sig);

            if (nS) {
                if (cS->getId() == nS->getId()) {
                    cS->routine(event, sig, msg);
                }
                else {
                    event->move(nS, sig, msg);
                }
            }
            else {
                std::stringstream ss;
                ss << "not found next status. current status:"
                    << cS->getId()
                    << " Signal:"
                    << sig;

                throw std::logic_error(ss.str());
            }
        }
        else {
            std::stringstream ss;
            ss << "not found event(" << eid << ") signal(" << sig << ")";
            throw std::logic_error(ss.str());
        }
    }

    void Xyh_Jsm::process(unsigned int eid, unsigned int sig, const void * msg) {
        shared_ptr<Xyh_Event> event = findEvent(eid);
        if (event) {
            //过期的event不再处理
            if (event->expired()) {
                return;
            }

            shared_ptr<XYH_StatusMachine::Xyh_Status> cS = event->getCurrentStatus();

            shared_ptr<XYH_StatusMachine::Xyh_Status> nS = cS->route(sig);

            if (nS) {
                event->move(nS, sig, msg);
            }
            else {
                std::stringstream ss;
                ss << "not found next status. current status:"
                    << cS->getId()
                    << " Signal:"
                    << sig;

                throw std::logic_error(ss.str());
            }
        }
        else {
            std::stringstream ss;
            ss << "not found event(" << eid << ") signal(" << sig << ")";
            throw std::logic_error(ss.str());
        }
    }

    void Xyh_Jsm::process(unsigned int sig, const void * msg) {
        typedef map<unsigned int, shared_ptr<Xyh_Event> >::iterator ItType;
        
        for (ItType it = m_mapEvent.begin(); it != m_mapEvent.end(); it++) {
            //过期的event不再处理
            if (it->second->expired()) {
                continue;
            }

            shared_ptr<XYH_StatusMachine::Xyh_Status> cS = it->second->getCurrentStatus();

            shared_ptr<XYH_StatusMachine::Xyh_Status> nS = cS->route(sig);

            if (nS) {
                it->second->move(nS, sig, msg);
            }
            else {
                std::cout << "not found next status. current status:" 
                    << cS->getId()
                    << " Signal:" 
                    << sig
                    << std::endl;
            }
        }
    }

    void Xyh_Jsm::addStatus(shared_ptr<Xyh_Status> s) {
        m_mapStatus.insert(std::make_pair(s->getId(), s));
    }

    shared_ptr<Xyh_Status> Xyh_Jsm::findStatus(unsigned int id) {
        if (m_mapStatus.end() == m_mapStatus.find(id)) {
            return shared_ptr<Xyh_Status>();
        }
        else {
            return m_mapStatus[id];
        }
    }

    void Xyh_Jsm::addEvent(shared_ptr<Xyh_Event> e) {
        if (!m_mapEvent.count(e->getId())) {
            m_mapEvent.insert(std::make_pair(e->getId(), e));
        } else {
            m_mapEvent[e->getId()] = e;
        }
        
    }

    void Xyh_Jsm::relEvent(unsigned int id) {
        if (m_mapEvent.count(id)) {
            m_mapEvent[id]->expire();
        }

        m_mapEvent.erase(id);
    }

    void Xyh_Jsm::expireEvent(unsigned int id) {
        if (m_mapEvent.count(id)) {
            m_mapEvent[id]->expire();
        }
    }

    shared_ptr<Xyh_Event> Xyh_Jsm::findEvent(unsigned int id) {
        if (m_mapEvent.end() != m_mapEvent.find(id)) {
            return m_mapEvent[id];
        }
        else {
            return shared_ptr<Xyh_Event>();
        }
    }

    void Xyh_Jsm::ticktock(const boost::system::error_code & e) {
        typedef map<unsigned int, shared_ptr<Xyh_Status> >::iterator ItType;

        if (e) {
            return;
        }

        //遍历所有status，并查看是否有定时事件到期
        for (ItType it = m_mapStatus.begin(); it != m_mapStatus.end(); it++) {
            it->second->ticktock(m_ticktock, e);
        }

        //滴答数+1
        m_ticktock++;
        m_timer.expires_from_now(boost::posix_time::seconds(1));
        m_timer.async_wait(boost::bind(&Xyh_Jsm::ticktock, this, _1));
    }

    void Xyh_Jsm::stop() {
        m_timer.cancel();
    }

}; //namespace XYH_StatusMachine