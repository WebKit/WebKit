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

#include <kdebug.h>
#include <q3ptrdict.h>

#include "kdom.h"
#include "Ecma.h"
#include "CDFInterface.h"
#include "GlobalObject.h"
#include "kdom/ecma/EcmaInterface.h"
#include "ScriptInterpreter.h"
#include "EventListenerImpl.h"

#include <kjs/protect.h>

// for getDOMNode
#include "TextImpl.h"
#include "AttrImpl.h"
#include "EntityImpl.h"
#include "ElementImpl.h"
#include "CommentImpl.h"
#include "NotationImpl.h"
#include "CDATASectionImpl.h"
#include "DocumentTypeImpl.h"
#include "EntityReferenceImpl.h"
#include "DocumentFragmentImpl.h"
#include "ProcessingInstructionImpl.h"

#include <kdom/bindings/js/core/TextWrapper.h>
#include <kdom/bindings/js/core/AttrWrapper.h>
#include <kdom/bindings/js/core/EntityWrapper.h>
#include <kdom/bindings/js/core/ElementWrapper.h>
#include <kdom/bindings/js/core/CommentWrapper.h>
#include <kdom/bindings/js/core/NotationWrapper.h>
#include <kdom/bindings/js/core/DocumentWrapper.h>
#include <kdom/bindings/js/core/DocumentTypeWrapper.h>
#include <kdom/bindings/js/core/CDATASectionWrapper.h>
#include <kdom/bindings/js/core/EntityReferenceWrapper.h>
#include <kdom/bindings/js/core/DocumentFragmentWrapper.h>
#include <kdom/bindings/js/core/ProcessingInstructionWrapper.h>

// for getDOMEvent
#include "EventImpl.h"
#include "UIEventImpl.h"
#include "MouseEventImpl.h"
#include "KeyboardEventImpl.h"
#include "MutationEventImpl.h"

#include <kdom/bindings/js/events/UIEventWrapper.h>
#include <kdom/bindings/js/events/MouseEventWrapper.h>
// #include <kdom/bindings/js/events/KeyboardEventWrapper.h>
#include <kdom/bindings/js/events/MutationEventWrapper.h>

// for getDOMCSSRule
#include "CSSRuleImpl.h"
#include "CSSPageRuleImpl.h"
#include "CSSMediaRuleImpl.h"
#include "CSSStyleRuleImpl.h"
#include "CSSImportRuleImpl.h"
#include "CSSUnknownRuleImpl.h"
#include "CSSCharsetRuleImpl.h"
#include "CSSFontFaceRuleImpl.h"

#include <kdom/bindings/js/css/CSSPageRuleWrapper.h>
#include <kdom/bindings/js/css/CSSMediaRuleWrapper.h>
#include <kdom/bindings/js/css/CSSStyleRuleWrapper.h>
#include <kdom/bindings/js/css/CSSImportRuleWrapper.h>
#include <kdom/bindings/js/css/CSSUnknownRuleWrapper.h>
#include <kdom/bindings/js/css/CSSCharsetRuleWrapper.h>
#include <kdom/bindings/js/css/CSSFontFaceRuleWrapper.h>

// for getDOMCSSValue
#include "CSSValueImpl.h"
#include "CSSValueListImpl.h"
#include "CSSPrimitiveValueImpl.h"

#include <kdom/bindings/js/css/CSSValueWrapper.h>
#include <kdom/bindings/js/css/CSSValueListWrapper.h>
#include <kdom/bindings/js/css/CSSPrimitiveValueWrapper.h>

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

    Q3PtrDict<EventListenerImpl> eventListeners;
};

Ecma::Ecma(DocumentImpl *doc) : d(new Private(doc))
{
    d->eventListeners.setAutoDelete(false);
}

Ecma::~Ecma()
{
    // Garbage collection magic, taken from khtml/ecma
    if(d->globalObject && d->interpreter)
        d->globalObject->clearProperties();

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
    DocumentWrapper *wrapper = new DocumentWrapper(document);
 
    KJS::ObjectImp *kjsObj = wrapper->bridge(d->interpreter->globalExec());
#ifndef APPLE_CHANGES
    kjsObj->ref();
#endif

    delete wrapper;

    d->interpreter->putDOMObject(document, kjsObj);
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
    i->initListener(d->document, true, listenerObject, listener, 0);

    addEventListener(i, static_cast<KJS::ObjectImp *>(listenerObject));
    return i;
}

EventListenerImpl *Ecma::createEventListener(DOMStringImpl *type, DOMStringImpl *jsCode)
{    
    if(!type || !jsCode)
        return 0;

    KJS::Interpreter::lock();

    // We probably deal with sth. like onload="alert('hi');' ...
    DOMStringImpl *internalType = new DOMStringImpl("[KDOM] - ");
    internalType->append(jsCode);

    Q3PtrDictIterator<EventListenerImpl> it(d->eventListeners);
    for( ; it.current(); ++it)
    {
        EventListenerImpl *current = it.current();
        if((current->internalType() ? current->internalType()->string() : QString::null) == internalType->string())
            return current;
    }
    
    static KJS::ProtectedPtr<KJS::ValueImp> eventString = KJS::String("event");

    KJS::ObjectImp *constr = d->interpreter->builtinFunction();
    KJS::ExecState *exec = d->interpreter->globalExec();
    
    KJS::List args;
    args.append(eventString);
    args.append(KJS::String(jsCode->string()));

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
        kdError() << "Unable to create event listener object for event type \"" << type->string() << "\"" << endl;

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

KJS::ObjectImp *Ecma::inheritedGetDOMNode(KJS::ExecState *, NodeImpl *)
{
    // Of course we are a stub within KDOM...
    return 0;
}

KJS::ObjectImp *Ecma::inheritedGetDOMEvent(KJS::ExecState *, EventImpl *)
{
    // Of course we are a stub within KDOM...
    return 0;
}

KJS::ObjectImp *Ecma::inheritedGetDOMCSSRule(KJS::ExecState *, CSSRuleImpl *)
{
    // Of course we are a stub within KDOM...
    return 0;
}

KJS::ObjectImp *Ecma::inheritedGetDOMCSSValue(KJS::ExecState *, CSSValueImpl *)
{
    // Of course we are a stub within KDOM...
    return 0;
}

// Helpers in namespace 'KDOM'
KJS::ValueImp *KDOM::getDOMNode(KJS::ExecState *exec, NodeImpl *n)
{
    KJS::ObjectImp *ret = 0;
    if(!n)
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
    KJS::ObjectImp *request = interpreter->getDOMObject(n);
    if(request)
        return request;

    // Try hard to ask any DOM/SVG/HTML/whatever implementation
    // which may reside on top of KDOM, how to convert the current
    // Node into an EcmaScript suitable object, use standard way as fallback
    request = engine->inheritedGetDOMNode(exec, n);
    if(request)
    {
        interpreter->putDOMObject(n, request);
        return request;
    }

    switch(n->nodeType())
    {
        case ELEMENT_NODE:
        {
            ret = (new ElementWrapper(static_cast<ElementImpl *>(n)))->bridge(exec);
            break;
        }
        case ATTRIBUTE_NODE:
        {
            ret = (new AttrWrapper(static_cast<AttrImpl *>(n)))->bridge(exec);
            break;
        }
        case TEXT_NODE:
        {
            ret = (new TextWrapper(static_cast<TextImpl *>(n)))->bridge(exec);
            break;
        }
        case CDATA_SECTION_NODE:
        {
            ret = (new CDATASectionWrapper(static_cast<CDATASectionImpl *>(n)))->bridge(exec);
            break;
        }
        case ENTITY_REFERENCE_NODE:
        {
            ret = (new EntityReferenceWrapper(static_cast<EntityReferenceImpl *>(n)))->bridge(exec);
            break;
        }
        case ENTITY_NODE:
        {
            ret = (new EntityWrapper(static_cast<EntityImpl *>(n)))->bridge(exec);
            break;
        }
        case PROCESSING_INSTRUCTION_NODE:
        {
            ret = (new ProcessingInstructionWrapper(static_cast<ProcessingInstructionImpl *>(n)))->bridge(exec);
            break;
        }
        case COMMENT_NODE:
        {
            ret = (new CommentWrapper(static_cast<CommentImpl *>(n)))->bridge(exec);
            break;
        }
        case DOCUMENT_NODE:
        {
            ret = (new DocumentWrapper(static_cast<DocumentImpl *>(n)))->bridge(exec);
            break;
        }
        case DOCUMENT_TYPE_NODE:
        {
            ret = (new DocumentTypeWrapper(static_cast<DocumentTypeImpl *>(n)))->bridge(exec);
            break;
        }
        case DOCUMENT_FRAGMENT_NODE:
        {
            ret = (new DocumentFragmentWrapper(static_cast<DocumentFragmentImpl *>(n)))->bridge(exec);
            break;
        }
        case NOTATION_NODE:
        {
            ret = (new NotationWrapper(static_cast<NotationImpl *>(n)))->bridge(exec);
            break;
        }
        default:
            ret = (new NodeWrapper(static_cast<NodeImpl *>(n)))->bridge(exec);
    }

    interpreter->putDOMObject(n, ret);
    return ret;
}

KJS::ValueImp *KDOM::getDOMEvent(KJS::ExecState *exec, EventImpl *e)
{
    KJS::ObjectImp *ret = 0;
    if(!e)
        return KJS::Null();

    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());

    // Reuse existing bridge, if possible
    KJS::ObjectImp *request = interpreter->getDOMObject(e);
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
            interpreter->putDOMObject(e, ret);
            return ret;
        }
    }

    KJS::Interpreter::lock();

    EventImplType identifier = e->identifier();

    if(identifier == TypeUIEvent)
        ret = (new UIEventWrapper(static_cast<UIEventImpl *>(e)))->bridge(exec);
    else if(identifier == TypeMouseEvent)
        ret = (new MouseEventWrapper(static_cast<MouseEventImpl *>(e)))->bridge(exec);
/*
    else if(identifier == TypeKeyboardEvent) // FIXME!
        ret = (new KeyboardEventWrapper(static_cast<KeyboardEventImpl *>(e)))->bridge(exec);
*/
    else if(identifier == TypeMutationEvent)
        ret = (new MutationEventWrapper(static_cast<MutationEventImpl *>(e)))->bridge(exec);
    else if(identifier == TypeGenericEvent)
        ret = (new EventWrapper(e))->bridge(exec);

    KJS::Interpreter::unlock();

    interpreter->putDOMObject(e, ret);
    return ret;
}

KJS::ValueImp *KDOM::getDOMCSSRule(KJS::ExecState *exec, CSSRuleImpl *c)
{
    KJS::ObjectImp *ret = 0;
    if(!c)
        return KJS::Null();

    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());

    // Reuse existing bridge, if possible
    KJS::ObjectImp *request = interpreter->getDOMObject(c);
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
            interpreter->putDOMObject(c, ret);
            return ret;
        }
    }

    if(c->isCharsetRule())
        ret = (new CSSCharsetRuleWrapper(static_cast<CSSCharsetRuleImpl *>(c)))->bridge(exec);
    else if(c->isFontFaceRule())
        ret = (new CSSFontFaceRuleWrapper(static_cast<CSSFontFaceRuleImpl *>(c)))->bridge(exec);
    else if(c->isImportRule())
        ret = (new CSSImportRuleWrapper(static_cast<CSSImportRuleImpl *>(c)))->bridge(exec);
    else if(c->isMediaRule())
        ret = (new CSSMediaRuleWrapper(static_cast<CSSMediaRuleImpl *>(c)))->bridge(exec);
    else if(c->isPageRule())
        ret = (new CSSPageRuleWrapper(static_cast<CSSPageRuleImpl *>(c)))->bridge(exec);
    else if(c->isStyleRule())
        ret = (new CSSStyleRuleWrapper(static_cast<CSSStyleRuleImpl *>(c)))->bridge(exec);
    else if(c->isUnknownRule())
        ret = (new CSSUnknownRuleWrapper(static_cast<CSSUnknownRuleImpl *>(c)))->bridge(exec);
    else
        ret = (new CSSRuleWrapper(c))->bridge(exec);
    
    interpreter->putDOMObject(c, ret);
    return ret;
}

KJS::ValueImp *KDOM::getDOMCSSValue(KJS::ExecState *exec, CSSValueImpl *c)
{
    KJS::ObjectImp *ret = 0;
    if(!c)
        return KJS::Null();

    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());

    // Reuse existing bridge, if possible
    KJS::ObjectImp *request = interpreter->getDOMObject(c);
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
            interpreter->putDOMObject(c, ret);
            return ret;
        }
    }

    if(c->isPrimitiveValue())
        ret = (new CSSPrimitiveValueWrapper(static_cast<CSSPrimitiveValueImpl *>(c)))->bridge(exec);
    else if(c->isValueList())
        ret = (new CSSValueListWrapper(static_cast<CSSValueListImpl *>(c)))->bridge(exec);
    else
        ret = (new CSSValueWrapper(c))->bridge(exec);
    
    interpreter->putDOMObject(c, ret);
    return ret;
}

DOMStringImpl *KDOM::toDOMString(KJS::ExecState *exec, KJS::ValueImp *val)
{
    // We have to distinguish between null and empty strings!
    // Very important to get the dom test suite running correctly :)
    QString string = val->toString(exec).qstring();
    if(string == "null")
        return 0;
    else if(string.isEmpty())
        return new DOMStringImpl(string);

    return new DOMStringImpl(string);
}

KJS::ValueImp *KDOM::getDOMString(DOMStringImpl *str)
{
    if(!str || str->isEmpty())
        return KJS::Null();

    return KJS::String(str->string());
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
