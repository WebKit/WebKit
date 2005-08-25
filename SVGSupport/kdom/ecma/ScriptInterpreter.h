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

#ifndef KDOM_ScriptInterpreter_H
#define KDOM_ScriptInterpreter_H

#include <kjs/interpreter.h>

namespace KJS
{
	class ValueImp;
	class ObjectImp;
};

namespace KDOM
{
	class EventImpl;
	class DocumentImpl;

	class ScriptInterpreter : public KJS::Interpreter
	{
	public:
		ScriptInterpreter(KJS::ObjectImp *global, DocumentImpl *doc);
		virtual ~ScriptInterpreter();

		DocumentImpl *document() const;

		KJS::ObjectImp *getDOMObject(void *handle) const;
		void putDOMObject(void *handle, KJS::ObjectImp *obj);
		void removeDOMObject(void *handle);
	
		// Used by Shared()'s deref() function
		static void forgetDOMObject(void *handle);

		EventImpl *currentEvent() const;
		void setCurrentEvent(EventImpl *evt);

		// Internal
		virtual void mark();

	private:
		class Private;
		Private *d;
	};

	// Lookup or create JS object around an existing "DOM Object"
	template<class DOMObjWrapper, class DOMObjImpl>
	inline KJS::ValueImp *cacheDOMObject(KJS::ExecState *exec, const DOMObjWrapper *constWrapper)
	{
		KJS::ObjectImp *jsObject = 0;
		if(!constWrapper)
			return KJS::Null();

		DOMObjWrapper *wrapper = const_cast<DOMObjWrapper *>(constWrapper);

		ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
		if((jsObject = interpreter->getDOMObject(wrapper)))
			return jsObject;

		jsObject = constWrapper->bridge(exec);
		interpreter->putDOMObject(wrapper, jsObject);
		return jsObject;
	}
};

#endif

// vim:ts=4:noet
