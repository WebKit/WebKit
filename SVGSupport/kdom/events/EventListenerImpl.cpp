/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include <qvariant.h>

#include "EventImpl.h"
#include "EventListenerImpl.h"

using namespace KDOM;

EventListenerImpl::EventListenerImpl() : Shared()
{
    m_doc = 0;
    m_internalType = 0;

    m_ecmaEventListener = false;

    m_listener = NULL;
    m_compareListener = NULL;
}

EventListenerImpl::~EventListenerImpl()
{
    if(m_internalType)
        m_internalType->deref();

/*
    if(m_doc && m_doc->ecmaEngine() && m_ecmaEventListener)
        m_doc->ecmaEngine()->removeEventListener(static_cast<KJS::ObjectImp *>(m_compareListener.imp()));
*/
}

void EventListenerImpl::handleEvent(EventImpl *evt)
{
/*
    if(!evt)
        return; 

    Q_ASSERT(m_doc != 0);

    KJS::Interpreter::lock();
    Ecma *ecmaEngine = m_doc->ecmaEngine();
    if(ecmaEngine && m_listener.implementsCall())
    {
        ScriptInterpreter *interpreter = ecmaEngine->interpreter();
        KJS::ExecState *exec = ecmaEngine->globalExec();

        // Append 'evt' object
        Event evtObj(evt);
        
        KJS::List args;
        args.append(getDOMEvent(exec, evtObj));

        // Set current event
        interpreter->setCurrentEvent(evt);

        // Call it!
        KJS::Object thisObj = KJS::Object::dynamicCast(safe_cache<EventTarget>(exec, evtObj.currentTarget()));
        KJS::Value retVal = m_listener.call(exec, thisObj, args);

        // Reset current event after processing
        interpreter->setCurrentEvent(0);

        if(exec->hadException())
            exec->clearException();
        else
        {
            // In case the js event listener returns a value, check it...
            QVariant ret = toVariant(exec, retVal);
            if(ret.type() == QVariant::Bool && ret.toBool() == false)
                evt->preventDefault();
        }
    }
    KJS::Interpreter::unlock();
*/
}

DOMStringImpl *EventListenerImpl::internalType() const
{
    if(!m_ecmaEventListener)
        return 0;

    return m_internalType;
}
KJS::ValueImp *EventListenerImpl::ecmaListener() const
{
    if(!m_ecmaEventListener)
        return NULL;

    return m_compareListener;
}

void EventListenerImpl::initListener(DocumentImpl *doc, bool ecmaEventListener, KJS::ObjectImp *listener, KJS::ValueImp *compareListener, DOMStringImpl *internalType)
{
    if(!listener || !listener->implementsCall())
        kdError() << "EcmaScript listener object " << listener << " is not valid or doesn't implement a call!" << endl;

    m_doc = doc;
    m_listener = listener;
    m_compareListener = compareListener;
    m_ecmaEventListener = ecmaEventListener;

    KDOM_SAFE_SET(m_internalType, internalType);
}

// vim:ts=4:noet
