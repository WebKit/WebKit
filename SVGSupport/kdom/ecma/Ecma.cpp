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

#include <kdebug.h>
#include <qvariant.h>
#include <qptrdict.h>

#include <kjs/interpreter.h>
#include <kjs/scope_chain.h>
#include <kjs/protect.h>

#include "Attr.h"
#include "Text.h"
#include "Ecma.h"
#include "Event.h"
#include "Entity.h"
#include <kdom/Helper.h>
#include "CSSRule.h"
#include "Element.h"
#include "Comment.h"
#include "UIEvent.h"
#include "CSSValue.h"
#include "Notation.h"
#include "Document.h"
#include "NodeImpl.h"
#include "EventImpl.h"
#include "MouseEvent.h"
#include "KeyboardEvent.h"
#include "CSSRuleImpl.h"
#include "UIEventImpl.h"
#include "CSSPageRule.h"
#include "CDFInterface.h"
#include "CSSStyleRule.h"
#include "CSSMediaRule.h"
#include "CSSValueImpl.h"
#include "AbstractView.h"
#include "DocumentType.h"
#include "CDATASection.h"
#include "GlobalObject.h"
#include "DocumentImpl.h"
#include "CSSValueList.h"
#include "CSSImportRule.h"
#include "EventListener.h"
#include "EcmaInterface.h"
#include "MutationEvent.h"
#include "CSSUnknownRule.h"
#include "CSSCharsetRule.h"
#include "MouseEventImpl.h"
#include "KeyboardEventImpl.h"
#include "EntityReference.h"
#include "EventTargetImpl.h"
#include "CSSFontFaceRule.h"
#include "DocumentFragment.h"
#include "ScriptInterpreter.h"
#include "MutationEventImpl.h"
#include "EventListenerImpl.h"
#include "CSSPrimitiveValue.h"
#include "ProcessingInstruction.h"

#include "Ecma.lut.h"
using namespace KDOM;

class Ecma::Private
{
public:
	Private(DocumentImpl *doc) : document(doc), globalObject(0), ecmaInterface(0), interpreter(0) { init = false; }
	virtual ~Private() { delete globalObject; }

	bool init;

	DocumentImpl *document;
	GlobalObject *globalObject;
	EcmaInterface *ecmaInterface;
	ScriptInterpreter *interpreter;

	QPtrDict<EventListenerImpl> eventListeners;
};

Ecma::Ecma(DocumentImpl *doc) : d(new Private(doc))
{
	d->eventListeners.setAutoDelete(false);
}

Ecma::~Ecma()
{
	// Garbage collection magic, taken from khtml/ecma
	if(d->globalObject && d->interpreter)
		d->globalObject->deleteAllProperties(d->interpreter->globalExec());

#ifndef APPLE_CHANGES
	while(KJS::Interpreter::collect()) ;
	delete d->interpreter;
	while(KJS::Interpreter::collect()) ;
#else
    delete d->interpreter;
#endif

	delete d;
}

void Ecma::setup(CDFInterface *interface)
{
	if(d->init || !interface)
		return;

	d->init = true;

	KJS::Interpreter::lock();
	// Create handler for js calls
	d->globalObject = interface->globalObject(d->document);
	d->ecmaInterface = interface->ecmaInterface();

	// Create code interpreter
	KJS::ObjectImp *kjsGlobalObject(d->globalObject);
	d->interpreter = new ScriptInterpreter(kjsGlobalObject, d->document);

	// Set object prototype for global object
	d->globalObject->setPrototype(d->interpreter->builtinObjectPrototype());

	setupDocument(d->document);
	KJS::Interpreter::unlock();
}

void Ecma::setupDocument(DocumentImpl *document)
{
	// Create base bridge for document
	Document docObj(document);
	
	KJS::ObjectImp *kjsObj = docObj.bridge(d->interpreter->globalExec());
#ifndef APPLE_CHANGES
	kjsObj->ref();
#endif
	
	d->interpreter->putDOMObject(document, kjsObj);
	document->deref(); // 'docObj' is held in memory until the ecma engine is destructed...
}

KJS::Completion Ecma::evaluate(const KJS::UString &code, KJS::ValueImp *thisV)
{
#ifdef KJS_VERBOSE
	kdDebug(6070) << "Ecma::evaluate " << code.qstring() << endl;
#endif

	return d->interpreter->evaluate(code, thisV);
}

KJS::ObjectImp *Ecma::globalObject() const
{
	return d->interpreter->globalObject();
}

KJS::ExecState *Ecma::globalExec() const
{
	return d->interpreter->globalExec();
}

EcmaInterface *Ecma::interface() const
{
	return d->ecmaInterface;
}

ScriptInterpreter *Ecma::interpreter() const
{
	return d->interpreter;
}

KJS::ObjectImp *Ecma::ecmaListenerToObject(KJS::ExecState *exec, KJS::ValueImp *listener)
{
	if(!listener->isObject())
		return NULL;
        KJS::ObjectImp *listenerObject = static_cast<KJS::ObjectImp *>(listener);
	
	// 'listener' may be a simple ecma function...
	if(listenerObject->implementsCall())
		return listenerObject;

	// 'listener' probably is an EventListener,
	// object containing a 'handleEvent' function...
	KJS::ValueImp *handleEventValue = listenerObject->get(exec, KJS::Identifier("handleEvent"));
	if(!handleEventValue->isObject())
		return NULL;
        KJS::ObjectImp *handleEventObject = static_cast<KJS::ObjectImp*>(handleEventValue);

	if(handleEventObject->implementsCall())
		return handleEventObject;

	kdError() << k_funcinfo << " Specified listener is neither a function nor an object!" << endl;
	return NULL;
}

EventListenerImpl *Ecma::findEventListener(KJS::ExecState *exec, KJS::ValueImp *listener)
{
	if(!d)
		return 0;
	
	KJS::ObjectImp *listenerObject = ecmaListenerToObject(exec, listener);
	if(!listenerObject)
		return 0;
	
	return d->eventListeners[static_cast<KJS::ObjectImp *>(listenerObject)];
}

EventListenerImpl *Ecma::createEventListener(KJS::ExecState *exec, KJS::ValueImp *listener)
{
	EventListenerImpl *existing = findEventListener(exec, listener);
	if(existing)
		return existing;
	
	KJS::ObjectImp *listenerObject = ecmaListenerToObject(exec, listener);
	if(!listenerObject)
		return 0;

	EventListenerImpl *i = new EventListenerImpl();
	i->initListener(d->document, true, listenerObject, listener, DOMString());

	addEventListener(i, static_cast<KJS::ObjectImp *>(listenerObject));
	return i;
}

EventListenerImpl *Ecma::createEventListener(const DOMString &type, const DOMString &jsCode)
{	
	KJS::Interpreter::lock();
	// We probably deal with sth. like onload="alert('hi');' ...
	DOMString internalType = KDOM::DOMString("[KDOM] - ") + jsCode;
	
	QPtrDictIterator<EventListenerImpl> it(d->eventListeners);
	for( ; it.current(); ++it)
	{
		EventListenerImpl *current = it.current();
		if(current->internalType() == internalType)
			return current;
	}
	
	static KJS::ProtectedValue eventString = KJS::String("event");

	KJS::ObjectImp *constr = d->interpreter->builtinFunction();
	KJS::ExecState *exec = d->interpreter->globalExec();
	
	KJS::List args;
	args.append(eventString);
	args.append(KJS::String(jsCode.string()));

	KJS::ObjectImp *obj = constr->construct(exec, args);
	if(exec->hadException())
	{
		exec->clearException();

		// failed to parse, so let's just make this listener a no-op
		obj = NULL;
	}

	// Safety first..
	EventListenerImpl *i = 0;
	if(obj)
	{
		i = new EventListenerImpl();
		i->initListener(d->document, true, obj, obj, internalType);
		addEventListener(i, obj);
	}
	else
		kdError() << "Unable to create event listener object for event type \"" << type << "\"" << endl;
	KJS::Interpreter::unlock();
	
	return i;
}

void Ecma::addEventListener(EventListenerImpl *listener, KJS::ObjectImp *imp)
{
	if(imp && listener)
		d->eventListeners.insert(imp, listener);
}

void Ecma::removeEventListener(KJS::ObjectImp *imp)
{
	if(imp)
		d->eventListeners.remove(imp);
}

void Ecma::finishedWithEvent(EventImpl *evt)
{
	ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(globalExec()->interpreter());
	interpreter->removeDOMObject(evt);
}

KJS::ObjectImp *Ecma::inheritedGetDOMNode(KJS::ExecState *, Node)
{
	// Of course we are a stub within KDOM...
	return 0;
}

KJS::ObjectImp *Ecma::inheritedGetDOMEvent(KJS::ExecState *, Event)
{
	// Of course we are a stub within KDOM...
	return 0;
}

KJS::ObjectImp *Ecma::inheritedGetDOMCSSRule(KJS::ExecState *, CSSRule)
{
	// Of course we are a stub within KDOM...
	return 0;
}

KJS::ObjectImp *Ecma::inheritedGetDOMCSSValue(KJS::ExecState *, CSSValue)
{
	// Of course we are a stub within KDOM...
	return 0;
}

// Helpers in namespace 'KDOM'
KJS::ValueImp *KDOM::getDOMNode(KJS::ExecState *exec, Node n)
{
	KJS::ObjectImp *ret = 0;
	if(n == Node::null)
		return KJS::Null();

	ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
	if(!interpreter)
		return KJS::Null();

	DocumentImpl *document = interpreter->document();
	if(!document)
		return KJS::Null();

	Ecma *engine = document->ecmaEngine();
	if(!engine)
		return KJS::Null();

	// Reuse existing bridge, if possible
	KJS::ObjectImp *request = interpreter->getDOMObject(n.handle());

	// Try hard to ask any DOM/SVG/HTML/whatever implementation
	// which may reside on top of KDOM, how to convert the current
	// Node into an EcmaScript suitable object, use standard way as fallback
	KJS::ObjectImp *topRequest = engine->inheritedGetDOMNode(exec, n);
	if(request)
	{
		if(topRequest && topRequest != request)
			return topRequest;
		
		return request;
	}
	else if(topRequest)
		return topRequest;
	
	switch(n.nodeType())
	{
		case ELEMENT_NODE:
			ret = Element(n).bridge(exec);
			break;
		case ATTRIBUTE_NODE:
			ret = Attr(n).bridge(exec);
			break;
		case TEXT_NODE:
			ret = Text(n).bridge(exec);
			break;
		case CDATA_SECTION_NODE:
			ret = CDATASection(n).bridge(exec);
			break;
		case ENTITY_REFERENCE_NODE:
			ret = EntityReference(n).bridge(exec);
			break;
		case ENTITY_NODE:
			ret = Entity(n).bridge(exec);
			break;
		case PROCESSING_INSTRUCTION_NODE:
			ret = ProcessingInstruction(n).bridge(exec);
			break;
		case COMMENT_NODE:
			ret = Comment(n).bridge(exec);
			break;
		case DOCUMENT_NODE:
			ret = Document(n).bridge(exec);
			break;
		case DOCUMENT_TYPE_NODE:
			ret = DocumentType(n).bridge(exec);
			break;
		case DOCUMENT_FRAGMENT_NODE:
			ret = DocumentFragment(n).bridge(exec);
			break;
		case NOTATION_NODE:
			ret = Notation(n).bridge(exec);
			break;
		default:
			ret = n.bridge(exec);
	}

	interpreter->putDOMObject(n.handle(), ret);
	return ret;
}

KJS::ValueImp *KDOM::getDOMEvent(KJS::ExecState *exec, Event e)
{
	KJS::ObjectImp *ret = 0;
	if(e == Event::null)
		return KJS::Null();

	ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());

	// Reuse existing bridge, if possible
	KJS::ObjectImp *request = interpreter->getDOMObject(e.handle());
	if(request)
		return request;

	// Try hard to ask any DOM/SVG/HTML/whatever implementation
	// which may reside on top of KDOM, how to convert the current
	// Node into an EcmaScript suitable object, use standard way as fallback
	Ecma *engine = interpreter->document()->ecmaEngine();
	if(engine)
	{
		KJS::Interpreter::lock();
		ret = engine->inheritedGetDOMEvent(exec, e);
		KJS::Interpreter::unlock();

		if(ret)
		{
			interpreter->putDOMObject(e.handle(), ret);
			return ret;
		}
	}

	KJS::Interpreter::lock();
	EventImplType identifier = (e.handle() ? (static_cast<EventImpl *>(e.handle())->identifier()) : TypeGenericEvent);

    if(identifier == TypeUIEvent)
		ret = UIEvent(static_cast<UIEventImpl *>(e.handle())).bridge(exec);
    else if(identifier == TypeMouseEvent)
		ret = MouseEvent(static_cast<MouseEventImpl *>(e.handle())).bridge(exec);
    else if(identifier == TypeKeyboardEvent)
		ret = KeyboardEvent(static_cast<KeyboardEventImpl *>(e.handle())).bridge(exec);
    else if(identifier == TypeMutationEvent)
		ret = MutationEvent(static_cast<MutationEventImpl *>(e.handle())).bridge(exec);
	else if(identifier == TypeGenericEvent)
		ret = e.bridge(exec);
	KJS::Interpreter::unlock();

	interpreter->putDOMObject(e.handle(), ret);
	return ret;
}

KJS::ValueImp *KDOM::getDOMCSSRule(KJS::ExecState *exec, CSSRule c)
{
	KJS::ObjectImp *ret = 0;
	if(c == CSSRule::null)
		return KJS::Null();

	ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());

	// Reuse existing bridge, if possible
	KJS::ObjectImp *request = interpreter->getDOMObject(c.handle());
	if(request)
		return request;

	// Try hard to ask any DOM/SVG/HTML/whatever implementation
	// which may reside on top of KDOM, how to convert the current
	// Node into an EcmaScript suitable object, use standard way as fallback
	Ecma *engine = interpreter->document()->ecmaEngine();
	if(engine)
	{
		ret = engine->inheritedGetDOMCSSRule(exec, c);

		if(ret)
		{
			interpreter->putDOMObject(c.handle(), ret);
			return ret;
		}
	}

	CSSRuleImpl *impl = static_cast<CSSRuleImpl *>(c.handle());

	if(impl->isCharsetRule())
		ret = CSSCharsetRule(c).bridge(exec);
	else if(impl->isFontFaceRule())
		ret = CSSFontFaceRule(c).bridge(exec);
	else if(impl->isImportRule())
		ret = CSSImportRule(c).bridge(exec);
	else if(impl->isMediaRule())
		ret = CSSMediaRule(c).bridge(exec);
	else if(impl->isPageRule())
		ret = CSSPageRule(c).bridge(exec);
	else if(impl->isStyleRule())
		ret = CSSStyleRule(c).bridge(exec);
	else if(impl->isUnknownRule())
		ret = CSSUnknownRule(c).bridge(exec);
	else
		ret = c.bridge(exec);
	
	interpreter->putDOMObject(c.handle(), ret);
	return ret;
}

KJS::ValueImp *KDOM::getDOMCSSValue(KJS::ExecState *exec, CSSValue c)
{
	KJS::ObjectImp *ret = 0;
	if(c == CSSValue::null)
		return KJS::Null();

	ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());

	// Reuse existing bridge, if possible
	KJS::ObjectImp *request = interpreter->getDOMObject(c.handle());
	if(request)
		return request;

	// Try hard to ask any DOM/SVG/HTML/whatever implementation
	// which may reside on top of KDOM, how to convert the current
	// Node into an EcmaScript suitable object, use standard way as fallback
	Ecma *engine = interpreter->document()->ecmaEngine();
	if(engine)
	{
		ret = engine->inheritedGetDOMCSSValue(exec, c);

		if(ret)
		{
			interpreter->putDOMObject(c.handle(), ret);
			return ret;
		}
	}

	CSSValueImpl *impl = static_cast<CSSValueImpl *>(c.handle());

	if(impl->isPrimitiveValue())
		ret = CSSPrimitiveValue(c).bridge(exec);
	else if(impl->isValueList())
		ret = CSSValueList(c).bridge(exec);
	else
		ret = c.bridge(exec);
	
	interpreter->putDOMObject(c.handle(), ret);
	return ret;
}

DOMString KDOM::toDOMString(KJS::ExecState *exec, KJS::ValueImp *val)
{
	// We have to distinguish between null and empty strings!
	// Very important to get the dom test suite running correctly :)
	QString string = val->toString(exec).qstring();
	if(string == "null")
		return DOMString();
	else if(string.isEmpty())
		return DOMString("");

	return DOMString(string);
}

KJS::ValueImp *KDOM::getDOMString(const DOMString &str)
{
	if(str.isNull() && str.isEmpty())
		return KJS::Null();

	return KJS::String(str.string());
}

QVariant KDOM::toVariant(KJS::ExecState *exec, KJS::ValueImp *val)
{
	QVariant res;

	switch(val->type())
	{
		case KJS::BooleanType:
			res = QVariant(val->toBoolean(exec), 0);
			break;
		case KJS::NumberType:
			res = QVariant(val->toNumber(exec));
			break;
		case KJS::StringType:
			res = QVariant(val->toString(exec).qstring());
			break;
		default:
			// everything else will be 'invalid'
			break;
	}

	return res;
}

// vim:ts=4:noet
