#pragma once
#include "fsm.h"


class TransitionBuilder {
    public:
        constexpr  TransitionBuilder& from(DeviceState from){
            m_from = from;
            return *this;
        }
        
        constexpr TransitionBuilder& on(DeviceEvent event){
            m_event = event;
            return *this;
        }
        constexpr TransitionBuilder& to(DeviceState to){
            m_to = to;
            return *this;
        }
        constexpr TransitionBuilder& action(void (*action)()){
            m_action = action;
            return *this;
        }
        
        constexpr Transition build() const{ return {m_from, m_event, m_to, m_action};}
    
    private:
        DeviceState m_from;
        DeviceEvent m_event;
        DeviceState m_to;
        void (*m_action)();
    };