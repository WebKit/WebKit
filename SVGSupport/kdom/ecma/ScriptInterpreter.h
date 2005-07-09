/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
                  2004 Rob Buis <buis@kde.org>

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

#include <kdom/ecma/DOMBridge.h>

namespace KJS
{
	class Value;
	class Object;
	class Interpreter;
};

namespace KDOM
{
	class EventImpl;
	class DocumentImpl;

	class ScriptInterpreter : public KJS::Interpreter
	{
	public:
		ScriptInterpreter(const KJS::Object &global, DocumentImpl *doc);
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
	template<class DOMObj>
	inline KJS::Value cacheDOMObject(KJS::ExecState *exec, typename DOMObj::Private *domObj)
	{
		KJS::ObjectImp *ret = 0;
		if(!domObj)
			return KJS::Null();

		ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
		if((ret = interpreter->getDOMObject(domObj)))
			return KJS::Value(ret);
		else
		{
			ret = DOMObj(domObj).bridge(exec);
			interpreter->putDOMObject(domObj, ret);
			return KJS::Value(ret);
		}
	}

	// Lookup or create singleton Impl object, and return a unique bridge object for it.
	// (Very much like KJS::cacheGlobalObject, which is for a singleton ObjectImp)
	// This one is _only_ used for Constructor objects.
	template <class ClassCtor>
	inline KJS::Object cacheGlobalBridge(KJS::ExecState *exec, const KJS::Identifier &propertyName)
	{
		KJS::ValueImp *obj = static_cast<KJS::ObjectImp*>(exec->interpreter()->globalObject().imp())->getDirect(propertyName);
		if(obj)
			return KJS::Object::dynamicCast(KJS::Value(obj));
		else
		{
			ClassCtor *ctor = new ClassCtor(exec); // create the ClassCtor instance
			KJS::Object newObject(new DOMBridgeCtor<ClassCtor>(exec, ctor)); // create the bridge around it
			exec->interpreter()->globalObject().put(exec, propertyName, newObject, KJS::Internal);
			return newObject;
		}
	}
};

#endif

// vim:ts=4:noet
