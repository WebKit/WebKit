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

#ifndef KDOM_Ecma_H
#define KDOM_Ecma_H

#include <kdom/Node.h>

class QVariant;

namespace KJS
{
	class Value;
	class Object;
	class UString;
	class ObjectImp;
	class ExecState;
	class Completion;
};

namespace KDOM
{
	class Event;
	class CSSRule;
	class CSSValue;
	class DOMString;
	class EventTarget;
	class AbstractView;
	class GlobalObject;
	class EcmaInterface;
	class EventListener;
	class ScriptInterpreter;

	class NodeImpl;
	class DocumentImpl;
	class CDFInterface;
	class EventListenerImpl;
	class Ecma
	{
	public:
		Ecma(DocumentImpl *doc);
		virtual ~Ecma();

		void setup(CDFInterface *interface);

		KJS::Completion evaluate(const KJS::UString &code, KJS::ValueImp *thisV);

		KJS::ObjectImp *globalObject() const;
		KJS::ExecState *globalExec() const;
		
		EcmaInterface *interface() const;
		ScriptInterpreter *interpreter() const;

		// Internal, used to handle event listeners
		KJS::ObjectImp *ecmaListenerToObject(KJS::ExecState *exec, KJS::ValueImp *listener);

		EventListenerImpl *createEventListener(const DOMString &type, const DOMString &jsCode);
		EventListenerImpl *createEventListener(KJS::ExecState *exec, KJS::ValueImp *listener);
		EventListenerImpl *findEventListener(KJS::ExecState *exec, KJS::ValueImp *listener);

		void addEventListener(EventListenerImpl *listener, KJS::ObjectImp *imp);
		void removeEventListener(KJS::ObjectImp *imp);

		void finishedWithEvent(EventImpl *evt);

		// Very important function that needs to be reimplemented by any user of
		// KDOM which builds another W3C Language on our top, who needs EcmaScript
		// bindings; Example for SVG: var document; <- That's a 'KSVG::SVGDocument'
		// debug(document.createElement('svg')); should show 'KSVG::SVGSVGElement'
		// but KDOM doesn't know anything about it and will return 'KDOM::Element'
		// Here is the standard way to avoid that!
		virtual KJS::ObjectImp *inheritedGetDOMNode(KJS::ExecState *exec, Node n);
		virtual KJS::ObjectImp *inheritedGetDOMEvent(KJS::ExecState *exec, Event e);
		virtual KJS::ObjectImp *inheritedGetDOMCSSRule(KJS::ExecState *exec, CSSRule c);
		virtual KJS::ObjectImp *inheritedGetDOMCSSValue(KJS::ExecState *exec, CSSValue c);

	protected:
		virtual void setupDocument(DocumentImpl *doc);

	private:
		class Private;
		Private *d;
	};

	// Helpers
	KJS::ValueImp *getDOMNode(KJS::ExecState *exec, Node n);
	KJS::ValueImp *getDOMEvent(KJS::ExecState *exec, Event e);
	KJS::ValueImp *getDOMCSSRule(KJS::ExecState *exec, CSSRule c);
	KJS::ValueImp *getDOMCSSValue(KJS::ExecState *exec, CSSValue c);

	KJS::ValueImp *getDOMString(const DOMString &str);

	DOMString toDOMString(KJS::ExecState *exec, KJS::ValueImp *val);
	QVariant toVariant(KJS::ExecState *exec, KJS::ValueImp *val);

	// Convert between ecma values and real kdom objects
	// Example: Node myNode = ecma_cast<Node>(exec, args[0], &toNode);
	//          Attr myAttr = ecma_cast<Attr>(exec, args[1], &toAttr);
	template<class T>
	T ecma_cast(KJS::ExecState *exec, KJS::ValueImp *val, T (convFuncPtr)(KJS::ExecState *, const KJS::ObjectImp *))
	{
		if(!val->isObject())
			return T::null;

		return convFuncPtr(exec, static_cast<KJS::ObjectImp *>(val));
	}

	// Convert between real kdom objects and ecma values
	// Example: return safe_cache<Attr>(exec, myAttr);
	template<class T>
	KJS::ValueImp *safe_cache(KJS::ExecState *exec, T obj)
	{
		if(obj != T::null)
			return obj.cache(exec);

		return KJS::Null();
	}
};

#endif

// vim:ts=4:noet
