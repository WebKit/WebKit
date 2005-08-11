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

#include <qvariant.h>

#include "Event.h"
#include "EventImpl.h"
#include "EventListenerImpl.h"

using namespace KDOM;

EventListenerImpl::EventListenerImpl() : Shared(true)
{
	m_doc = 0;
	m_ecmaEventListener = false;

	m_listener = NULL;
	m_compareListener = NULL;
}

EventListenerImpl::~EventListenerImpl()
{
	if(m_doc && m_doc->ecmaEngine() && m_ecmaEventListener)
		m_doc->ecmaEngine()->removeEventListener(static_cast<KJS::ObjectImp *>(m_compareListener));
}

void EventListenerImpl::handleEvent(EventImpl *evt)
{
	if(!evt)
		return; 

	Q_ASSERT(m_doc != 0);

	KJS::Interpreter::lock();
	Ecma *ecmaEngine = m_doc->ecmaEngine();
	if(ecmaEngine && m_listener->implementsCall())
	{
		ScriptInterpreter *interpreter = ecmaEngine->interpreter();
		KJS::ExecState *exec = ecmaEngine->globalExec();

		// Append 'evt' object
		Event evtObj(evt);
		
		KJS::List args;
		args.append(KDOM::getDOMEvent(exec, evtObj));

		// Set current event
		interpreter->setCurrentEvent(evt);

		// Call it!
		KJS::ObjectImp *thisObj = static_cast<KJS::ObjectImp *>(safe_cache<EventTarget>(exec, evtObj.currentTarget()));
		KJS::ValueImp *retVal = m_listener->callAsFunction(exec, thisObj, args);

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
}

DOMString EventListenerImpl::internalType() const
{
	if(!m_ecmaEventListener)
		return DOMString();

	return m_internalType;
}

KJS::ValueImp *EventListenerImpl::ecmaListener() const
{
	if(!m_ecmaEventListener)
		return NULL;

	return m_compareListener;
}

void EventListenerImpl::initListener(DocumentImpl *doc, bool ecmaEventListener, KJS::ObjectImp *listener, KJS::ValueImp *compareListener, const DOMString &internalType)
{
	if(!listener || !listener->implementsCall())
		kdError() << "EcmaScript listener object " << listener << " is not valid or doesn't implement a call!" << endl;

	m_doc = doc;
	m_listener = listener;
	m_internalType = internalType;
	m_compareListener = compareListener;
	m_ecmaEventListener = ecmaEventListener;
}

// vim:ts=4:noet
