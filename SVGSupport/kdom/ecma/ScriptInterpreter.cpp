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

#include <assert.h>

#include <qptrdict.h>
#include <qptrlist.h>

#include "Document.h"
#include "EventImpl.h"
#include "ScriptInterpreter.h"

using namespace KDOM;

typedef QPtrList<ScriptInterpreter> InterpreterList;
static InterpreterList *s_interpreterList;

class ScriptInterpreter::Private
{
public:
	Private(DocumentImpl *doc) : document(doc), currentEvent(0) { }
	virtual ~Private() { }

	DocumentImpl *document;
	EventImpl *currentEvent;

	QPtrDict<KJS::ObjectImp> domObjects;
};

ScriptInterpreter::ScriptInterpreter(KJS::ObjectImp *global, DocumentImpl *doc) : KJS::Interpreter(global), d(new Private(doc))
{
	if(!s_interpreterList)
		s_interpreterList = new InterpreterList();

	s_interpreterList->append(this);
}

ScriptInterpreter::~ScriptInterpreter()
{
	assert(s_interpreterList && s_interpreterList->containsRef(this));
	s_interpreterList->remove(this);
	
	if(s_interpreterList->isEmpty())
	{
		delete s_interpreterList;
		s_interpreterList = 0;
	}

	delete d;
}

DocumentImpl *ScriptInterpreter::document() const
{
	if(!d)
		return 0;

	return d->document;
}

EventImpl *ScriptInterpreter::currentEvent() const
{
	if(!d)
		return 0;

	return d->currentEvent;
}

void ScriptInterpreter::setCurrentEvent(EventImpl *evt)
{
	if(d)
		d->currentEvent = evt;
}

KJS::ObjectImp *ScriptInterpreter::getDOMObject(void *handle) const
{
	if(!d)
		return 0;
		
	return d->domObjects[handle];
}

void ScriptInterpreter::putDOMObject(void *handle, KJS::ObjectImp *obj)
{
	if(!handle)
	{
		kdFatal() << k_funcinfo << " Cannot cache nil object! Backtrace: " << kdBacktrace() << endl;
		return;
	}
	
	if(d)
		d->domObjects.insert(handle, obj);
}

void ScriptInterpreter::removeDOMObject(void *handle)
{
	if(d)
		d->domObjects.take(handle);
}

void ScriptInterpreter::forgetDOMObject(void *handle)
{
	// Shared()'s deref() method uses this function to make us forget
	// about nearly deleted objects; it deletes the handle
	// in all interpreter instances, if multiple exist
	if(!s_interpreterList)
		return;

	QPtrListIterator<ScriptInterpreter> it(*s_interpreterList);
	while(it.current())
	{
		(*it)->removeDOMObject(handle);
		++it;
	}
}

void ScriptInterpreter::mark()
{
	KJS::Interpreter::mark();

	if(!d)
		return;

	kdDebug() << "!!!!!!!!!! ScriptInterpreter::mark " << this << " marking " << d->domObjects.count() << " DOM objects" << endl;
	QPtrDictIterator<KJS::ObjectImp> it(d->domObjects);
	for(; it.current(); ++it)
		it.current()->mark();
}

// vim:ts=4:noet
