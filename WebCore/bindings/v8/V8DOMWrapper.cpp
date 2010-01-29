/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8DOMWrapper.h"

#include "CSSMutableStyleDeclaration.h"
#include "DOMDataStore.h"
#include "DOMObjectsInclude.h"
#include "DocumentLoader.h"
#include "FrameLoaderClient.h"
#include "Notification.h"
#include "SVGElementInstance.h"
#include "SVGPathSeg.h"
#include "ScriptController.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8CustomBinding.h"
#include "V8CustomEventListener.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8EventListenerList.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8Index.h"
#include "V8IsolatedContext.h"
#include "V8MessageChannel.h"
#include "V8Location.h"
#include "V8NamedNodeMap.h"
#include "V8NodeList.h"
#include "V8Proxy.h"
#include "V8StyleSheet.h"
#include "WebGLArray.h"
#include "WebGLContextAttributes.h"
#include "WebGLUniformLocation.h"
#include "WorkerContextExecutionProxy.h"

#include <algorithm>
#include <utility>
#include <v8.h>
#include <v8-debug.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

#if ENABLE(SVG)

static V8ClassIndex::V8WrapperType downcastSVGPathSeg(void* pathSeg)
{
    SVGPathSeg* realPathSeg = reinterpret_cast<SVGPathSeg*>(pathSeg);

    switch (realPathSeg->pathSegType()) {
    case SVGPathSeg::PATHSEG_CLOSEPATH:                    return V8ClassIndex::SVGPATHSEGCLOSEPATH;
    case SVGPathSeg::PATHSEG_MOVETO_ABS:                   return V8ClassIndex::SVGPATHSEGMOVETOABS;
    case SVGPathSeg::PATHSEG_MOVETO_REL:                   return V8ClassIndex::SVGPATHSEGMOVETOREL;
    case SVGPathSeg::PATHSEG_LINETO_ABS:                   return V8ClassIndex::SVGPATHSEGLINETOABS;
    case SVGPathSeg::PATHSEG_LINETO_REL:                   return V8ClassIndex::SVGPATHSEGLINETOREL;
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:            return V8ClassIndex::SVGPATHSEGCURVETOCUBICABS;
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:            return V8ClassIndex::SVGPATHSEGCURVETOCUBICREL;
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:        return V8ClassIndex::SVGPATHSEGCURVETOQUADRATICABS;
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:        return V8ClassIndex::SVGPATHSEGCURVETOQUADRATICREL;
    case SVGPathSeg::PATHSEG_ARC_ABS:                      return V8ClassIndex::SVGPATHSEGARCABS;
    case SVGPathSeg::PATHSEG_ARC_REL:                      return V8ClassIndex::SVGPATHSEGARCREL;
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:        return V8ClassIndex::SVGPATHSEGLINETOHORIZONTALABS;
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:        return V8ClassIndex::SVGPATHSEGLINETOHORIZONTALREL;
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:          return V8ClassIndex::SVGPATHSEGLINETOVERTICALABS;
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:          return V8ClassIndex::SVGPATHSEGLINETOVERTICALREL;
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:     return V8ClassIndex::SVGPATHSEGCURVETOCUBICSMOOTHABS;
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:     return V8ClassIndex::SVGPATHSEGCURVETOCUBICSMOOTHREL;
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS: return V8ClassIndex::SVGPATHSEGCURVETOQUADRATICSMOOTHABS;
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL: return V8ClassIndex::SVGPATHSEGCURVETOQUADRATICSMOOTHREL;
    default:                                               return V8ClassIndex::INVALID_CLASS_INDEX;
    }
}

v8::Handle<v8::Value> V8DOMWrapper::convertSVGElementInstanceToV8Object(SVGElementInstance* instance)
{
    if (!instance)
        return v8::Null();

    v8::Handle<v8::Object> existingInstance = getDOMSVGElementInstanceMap().get(instance);
    if (!existingInstance.IsEmpty())
        return existingInstance;

    instance->ref();

    // Instantiate the V8 object and remember it
    v8::Handle<v8::Object> result = instantiateV8Object(V8ClassIndex::SVGELEMENTINSTANCE, V8ClassIndex::SVGELEMENTINSTANCE, instance);
    if (!result.IsEmpty()) {
        // Only update the DOM SVG element map if the result is non-empty.
        getDOMSVGElementInstanceMap().set(instance, v8::Persistent<v8::Object>::New(result));
    }
    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertSVGObjectWithContextToV8Object(V8ClassIndex::V8WrapperType type, void* object)
{
    if (!object)
        return v8::Null();

    v8::Persistent<v8::Object> result = getDOMSVGObjectWithContextMap().get(object);
    if (!result.IsEmpty())
        return result;

    // Special case: SVGPathSegs need to be downcast to their real type
    if (type == V8ClassIndex::SVGPATHSEG)
        type = downcastSVGPathSeg(object);

    v8::Local<v8::Object> v8Object = instantiateV8Object(type, type, object);
    if (!v8Object.IsEmpty()) {
        result = v8::Persistent<v8::Object>::New(v8Object);
        switch (type) {
#define MAKE_CASE(TYPE, NAME)     \
        case V8ClassIndex::TYPE: static_cast<NAME*>(object)->ref(); break;
        SVG_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
#define MAKE_CASE(TYPE, NAME)     \
        case V8ClassIndex::TYPE:    \
            static_cast<V8SVGPODTypeWrapper<NAME>*>(object)->ref(); break;
        SVG_POD_NATIVE_TYPES(MAKE_CASE)
#undef MAKE_CASE
        default:
            ASSERT_NOT_REACHED();
        }
        getDOMSVGObjectWithContextMap().set(object, result);
    }

    return result;
}

#endif // ENABLE(SVG)

#if ENABLE(3D_CANVAS)
void V8DOMWrapper::setIndexedPropertiesToExternalArray(v8::Handle<v8::Object> wrapper,
                                                       int index,
                                                       void* address,
                                                       int length)
{
    v8::ExternalArrayType array_type = v8::kExternalByteArray;
    V8ClassIndex::V8WrapperType classIndex = V8ClassIndex::FromInt(index);
    switch (classIndex) {
    case V8ClassIndex::WEBGLBYTEARRAY:
        array_type = v8::kExternalByteArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDBYTEARRAY:
        array_type = v8::kExternalUnsignedByteArray;
        break;
    case V8ClassIndex::WEBGLSHORTARRAY:
        array_type = v8::kExternalShortArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDSHORTARRAY:
        array_type = v8::kExternalUnsignedShortArray;
        break;
    case V8ClassIndex::WEBGLINTARRAY:
        array_type = v8::kExternalIntArray;
        break;
    case V8ClassIndex::WEBGLUNSIGNEDINTARRAY:
        array_type = v8::kExternalUnsignedIntArray;
        break;
    case V8ClassIndex::WEBGLFLOATARRAY:
        array_type = v8::kExternalFloatArray;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    wrapper->SetIndexedPropertiesToExternalArrayData(address,
                                                     array_type,
                                                     length);
}
#endif

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
    default:
        break;
    }
#endif
    getDOMObjectMap().set(object, wrapper);
}

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForActiveDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE: break;
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
    default: 
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
    }
#endif
    getActiveDOMObjectMap().set(object, wrapper);
}

// The caller must have increased node's ref count.
void V8DOMWrapper::setJSWrapperForDOMNode(Node* node, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
    getDOMNodeMap().set(node, wrapper);
}

v8::Persistent<v8::FunctionTemplate> V8DOMWrapper::getTemplate(V8ClassIndex::V8WrapperType type)
{
    v8::Persistent<v8::FunctionTemplate>* cacheCell = V8ClassIndex::GetCache(type);
    if (!cacheCell->IsEmpty())
        return *cacheCell;

    // Not in the cache.
    FunctionTemplateFactory factory = V8ClassIndex::GetFactory(type);
    v8::Persistent<v8::FunctionTemplate> descriptor = factory();

    *cacheCell = descriptor;
    return descriptor;
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> objectPrototype)
{
    // A DOM constructor is a function instance created from a DOM constructor
    // template. There is one instance per context. A DOM constructor is
    // different from a normal function in two ways:
    //   1) it cannot be called as constructor (aka, used to create a DOM object)
    //   2) its __proto__ points to Object.prototype rather than
    //      Function.prototype.
    // The reason for 2) is that, in Safari, a DOM constructor is a normal JS
    // object, but not a function. Hotmail relies on the fact that, in Safari,
    // HTMLElement.__proto__ == Object.prototype.
    v8::Handle<v8::FunctionTemplate> functionTemplate = getTemplate(type);
    // Getting the function might fail if we're running out of
    // stack or memory.
    v8::TryCatch tryCatch;
    v8::Local<v8::Function> value = functionTemplate->GetFunction();
    if (value.IsEmpty())
        return v8::Local<v8::Function>();
    // Hotmail fix, see comments above.
    if (!objectPrototype.IsEmpty())
        value->Set(v8::String::New("__proto__"), objectPrototype);
    return value;
}

v8::Local<v8::Function> V8DOMWrapper::getConstructorForContext(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Context> context)
{
    // Enter the scope for this context to get the correct constructor.
    v8::Context::Scope scope(context);

    return getConstructor(type, V8DOMWindowShell::getHiddenObjectPrototype(context));
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, DOMWindow* window)
{
    Frame* frame = window->frame();
    if (!frame)
        return v8::Local<v8::Function>();

    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return v8::Local<v8::Function>();

    return getConstructorForContext(type, context);
}

v8::Local<v8::Function> V8DOMWrapper::getConstructor(V8ClassIndex::V8WrapperType type, WorkerContext*)
{
    WorkerContextExecutionProxy* proxy = WorkerContextExecutionProxy::retrieve();
    if (!proxy)
        return v8::Local<v8::Function>();

    v8::Handle<v8::Context> context = proxy->context();
    if (context.IsEmpty())
        return v8::Local<v8::Function>();

    return getConstructorForContext(type, context);
}

v8::Handle<v8::Value> V8DOMWrapper::convertToV8Object(V8ClassIndex::V8WrapperType type, void* impl)
{
    ASSERT(type != V8ClassIndex::EVENTLISTENER);
    ASSERT(type != V8ClassIndex::EVENTTARGET);
    ASSERT(type != V8ClassIndex::EVENT);

    // These objects can be constructed under WorkerContextExecutionProxy.  They need special
    // handling, since if we proceed below V8Proxy::retrieve() will get called and will crash.
    // TODO(ukai): websocket?
    if ((type == V8ClassIndex::DOMCOREEXCEPTION
         || type == V8ClassIndex::RANGEEXCEPTION
         || type == V8ClassIndex::EVENTEXCEPTION
         || type == V8ClassIndex::XMLHTTPREQUESTEXCEPTION
         || type == V8ClassIndex::MESSAGEPORT)
        && WorkerContextExecutionProxy::retrieve()) {
        return WorkerContextExecutionProxy::convertToV8Object(type, impl);
    }

    bool isActiveDomObject = false;
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        DOM_NODE_TYPES(MAKE_CASE)
#if ENABLE(SVG)
        SVG_NODE_TYPES(MAKE_CASE)
#endif
        return convertNodeToV8Object(static_cast<Node*>(impl));
    case V8ClassIndex::CSSVALUE:
        return convertCSSValueToV8Object(static_cast<CSSValue*>(impl));
    case V8ClassIndex::CSSRULE:
        return convertCSSRuleToV8Object(static_cast<CSSRule*>(impl));
    case V8ClassIndex::STYLESHEET:
        return convertStyleSheetToV8Object(static_cast<StyleSheet*>(impl));
    case V8ClassIndex::DOMWINDOW:
        return convertWindowToV8Object(static_cast<DOMWindow*>(impl));
    case V8ClassIndex::NAMEDNODEMAP:
        return convertNamedNodeMapToV8Object(static_cast<NamedNodeMap*>(impl));
#if ENABLE(SVG)
        SVG_NONNODE_TYPES(MAKE_CASE)
        if (type == V8ClassIndex::SVGELEMENTINSTANCE)
            return convertSVGElementInstanceToV8Object(static_cast<SVGElementInstance*>(impl));
        return convertSVGObjectWithContextToV8Object(type, impl);
#endif

        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        isActiveDomObject = true;
        break;
    default:
        break;
  }

#undef MAKE_CASE

    if (!impl)
        return v8::Null();

    // Non DOM node
    v8::Persistent<v8::Object> result = isActiveDomObject ? getActiveDOMObjectMap().get(impl) : getDOMObjectMap().get(impl);
    if (result.IsEmpty()) {
#if ENABLE(3D_CANVAS)
        if (type == V8ClassIndex::WEBGLARRAY && impl) {
            // Determine which subclass we are wrapping.
            WebGLArray* array = reinterpret_cast<WebGLArray*>(impl);
            if (array->isByteArray())
                type = V8ClassIndex::WEBGLBYTEARRAY;
            else if (array->isFloatArray())
                type = V8ClassIndex::WEBGLFLOATARRAY;
            else if (array->isIntArray())
                type = V8ClassIndex::WEBGLINTARRAY;
            else if (array->isShortArray())
                type = V8ClassIndex::WEBGLSHORTARRAY;
            else if (array->isUnsignedByteArray())
                type = V8ClassIndex::WEBGLUNSIGNEDBYTEARRAY;
            else if (array->isUnsignedIntArray())
                type = V8ClassIndex::WEBGLUNSIGNEDINTARRAY;
            else if (array->isUnsignedShortArray())
                type = V8ClassIndex::WEBGLUNSIGNEDSHORTARRAY;
        }
#endif

        v8::Local<v8::Object> v8Object = instantiateV8Object(type, type, impl);
        if (!v8Object.IsEmpty()) {
            // Go through big switch statement, it has some duplications
            // that were handled by code above (such as CSSVALUE, CSSRULE, etc).
            switch (type) {
#define MAKE_CASE(TYPE, NAME) \
            case V8ClassIndex::TYPE: static_cast<NAME*>(impl)->ref(); break;
                DOM_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
            default:
                ASSERT_NOT_REACHED();
            }
            result = v8::Persistent<v8::Object>::New(v8Object);
            if (isActiveDomObject)
                setJSWrapperForActiveDOMObject(impl, result);
            else
                setJSWrapperForDOMObject(impl, result);

            if (type == V8ClassIndex::CANVASPIXELARRAY) {
                CanvasPixelArray* pixels = reinterpret_cast<CanvasPixelArray*>(impl);
                result->SetIndexedPropertiesToPixelData(pixels->data()->data(), pixels->length());
            }

#if ENABLE(3D_CANVAS)
            // Set up WebGLArray subclasses' accesses similarly.
            switch (type) {
            case V8ClassIndex::WEBGLBYTEARRAY:
            case V8ClassIndex::WEBGLUNSIGNEDBYTEARRAY:
            case V8ClassIndex::WEBGLSHORTARRAY:
            case V8ClassIndex::WEBGLUNSIGNEDSHORTARRAY:
            case V8ClassIndex::WEBGLINTARRAY:
            case V8ClassIndex::WEBGLUNSIGNEDINTARRAY:
            case V8ClassIndex::WEBGLFLOATARRAY: {
                WebGLArray* array = reinterpret_cast<WebGLArray*>(impl);
                setIndexedPropertiesToExternalArray(result,
                                                    V8ClassIndex::ToInt(type),
                                                    array->baseAddress(),
                                                    array->length());
                break;
            }
            default:
                break;
            }
#endif

            // Special case for non-node objects associated with a
            // DOMWindow. Both Safari and FF let the JS wrappers for these
            // objects survive GC. To mimic their behavior, V8 creates
            // hidden references from the DOMWindow to these wrapper
            // objects. These references get cleared when the DOMWindow is
            // reused by a new page.
            switch (type) {
            case V8ClassIndex::CONSOLE:
                setHiddenWindowReference(static_cast<Console*>(impl)->frame(), V8DOMWindow::consoleIndex, result);
                break;
            case V8ClassIndex::HISTORY:
                setHiddenWindowReference(static_cast<History*>(impl)->frame(), V8DOMWindow::historyIndex, result);
                break;
            case V8ClassIndex::NAVIGATOR:
                setHiddenWindowReference(static_cast<Navigator*>(impl)->frame(), V8DOMWindow::navigatorIndex, result);
                break;
            case V8ClassIndex::SCREEN:
                setHiddenWindowReference(static_cast<Screen*>(impl)->frame(), V8DOMWindow::screenIndex, result);
                break;
            case V8ClassIndex::LOCATION:
                setHiddenWindowReference(static_cast<Location*>(impl)->frame(), V8DOMWindow::locationIndex, result);
                break;
            case V8ClassIndex::DOMSELECTION:
                setHiddenWindowReference(static_cast<DOMSelection*>(impl)->frame(), V8DOMWindow::domSelectionIndex, result);
                break;
            case V8ClassIndex::BARINFO: {
                BarInfo* barInfo = static_cast<BarInfo*>(impl);
                Frame* frame = barInfo->frame();
                switch (barInfo->type()) {
                case BarInfo::Locationbar:
                    setHiddenWindowReference(frame, V8DOMWindow::locationbarIndex, result);
                    break;
                case BarInfo::Menubar:
                    setHiddenWindowReference(frame, V8DOMWindow::menubarIndex, result);
                    break;
                case BarInfo::Personalbar:
                    setHiddenWindowReference(frame, V8DOMWindow::personalbarIndex, result);
                    break;
                case BarInfo::Scrollbars:
                    setHiddenWindowReference(frame, V8DOMWindow::scrollbarsIndex, result);
                    break;
                case BarInfo::Statusbar:
                    setHiddenWindowReference(frame, V8DOMWindow::statusbarIndex, result);
                    break;
                case BarInfo::Toolbar:
                    setHiddenWindowReference(frame, V8DOMWindow::toolbarIndex, result);
                    break;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    return result;
}

void V8DOMWrapper::setHiddenWindowReference(Frame* frame, const int internalIndex, v8::Handle<v8::Object> jsObject)
{
    // Get DOMWindow
    if (!frame)
        return; // Object might be detached from window
    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return;

    ASSERT(internalIndex < V8DOMWindow::internalFieldCount);

    v8::Handle<v8::Object> global = context->Global();
    // Look for real DOM wrapper.
    global = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    ASSERT(global->GetInternalField(internalIndex)->IsUndefined());
    global->SetInternalField(internalIndex, jsObject);
}

V8ClassIndex::V8WrapperType V8DOMWrapper::domWrapperType(v8::Handle<v8::Object> object)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    return V8ClassIndex::FromInt(type->Int32Value());
}

PassRefPtr<NodeFilter> V8DOMWrapper::wrapNativeNodeFilter(v8::Handle<v8::Value> filter)
{
    // A NodeFilter is used when walking through a DOM tree or iterating tree
    // nodes.
    // FIXME: we may want to cache NodeFilterCondition and NodeFilter
    // object, but it is minor.
    // NodeFilter is passed to NodeIterator that has a ref counted pointer
    // to NodeFilter. NodeFilter has a ref counted pointer to NodeFilterCondition.
    // In NodeFilterCondition, filter object is persisted in its constructor,
    // and disposed in its destructor.
    if (!filter->IsFunction())
        return 0;

    NodeFilterCondition* condition = new V8NodeFilterCondition(filter);
    return NodeFilter::create(condition);
}

v8::Local<v8::Object> V8DOMWrapper::instantiateV8Object(V8Proxy* proxy, V8ClassIndex::V8WrapperType descriptorType, V8ClassIndex::V8WrapperType cptrType, void* impl)
{
    // Make a special case for document.all
    if (descriptorType == V8ClassIndex::HTMLCOLLECTION && static_cast<HTMLCollection*>(impl)->type() == DocAll)
        descriptorType = V8ClassIndex::HTMLALLCOLLECTION;

    if (V8IsolatedContext::getEntered()) {
        // This effectively disables the wrapper cache for isolated worlds.
        proxy = 0;
        // FIXME: Do we need a wrapper cache for the isolated world?  We should
        //        see if the performance gains are worth while.
        // We'll get one once we give the isolated context a proper window shell.
    } else if (!proxy)
        proxy = V8Proxy::retrieve();

    v8::Local<v8::Object> instance;
    if (proxy)
        // FIXME: Fix this to work properly with isolated worlds (see above).
        instance = proxy->windowShell()->createWrapperFromCache(descriptorType);
    else {
        v8::Local<v8::Function> function = getTemplate(descriptorType)->GetFunction();
        instance = SafeAllocation::newInstance(function);
    }
    if (!instance.IsEmpty()) {
        // Avoid setting the DOM wrapper for failed allocations.
        setDOMWrapper(instance, V8ClassIndex::ToInt(cptrType), impl);
    }
    return instance;
}

#ifndef NDEBUG
bool V8DOMWrapper::maybeDOMWrapper(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    return true;
}
#endif

bool V8DOMWrapper::isDOMEventWrapper(v8::Handle<v8::Value> value)
{
    // All kinds of events use EVENT as dom type in JS wrappers.
    // See EventToV8Object
    return isWrapperOfType(value, V8ClassIndex::EVENT);
}

bool V8DOMWrapper::isWrapperOfType(v8::Handle<v8::Value> value, V8ClassIndex::V8WrapperType classType)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    v8::Handle<v8::Value> type = object->GetInternalField(v8DOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    return V8ClassIndex::FromInt(type->Int32Value()) == classType;
}

#if ENABLE(VIDEO)
#define FOR_EACH_VIDEO_TAG(macro)                  \
    macro(audio, AUDIO)                            \
    macro(source, SOURCE)                          \
    macro(video, VIDEO)
#else
#define FOR_EACH_VIDEO_TAG(macro)
#endif

#if ENABLE(DATAGRID)
#define FOR_EACH_DATAGRID_TAG(macro)               \
    macro(datagrid, DATAGRID)                        \
    macro(dcell, DATAGRIDCELL)                       \
    macro(dcol, DATAGRIDCOL)                         \
    macro(drow, DATAGRIDROW)
#else
#define FOR_EACH_DATAGRID_TAG(macro)
#endif

#define FOR_EACH_TAG(macro)                        \
    FOR_EACH_DATAGRID_TAG(macro)                   \
    macro(a, ANCHOR)                               \
    macro(applet, APPLET)                          \
    macro(area, AREA)                              \
    macro(base, BASE)                              \
    macro(basefont, BASEFONT)                      \
    macro(blockquote, BLOCKQUOTE)                  \
    macro(body, BODY)                              \
    macro(br, BR)                                  \
    macro(button, BUTTON)                          \
    macro(caption, TABLECAPTION)                   \
    macro(col, TABLECOL)                           \
    macro(colgroup, TABLECOL)                      \
    macro(del, MOD)                                \
    macro(canvas, CANVAS)                          \
    macro(dir, DIRECTORY)                          \
    macro(div, DIV)                                \
    macro(dl, DLIST)                               \
    macro(embed, EMBED)                            \
    macro(fieldset, FIELDSET)                      \
    macro(font, FONT)                              \
    macro(form, FORM)                              \
    macro(frame, FRAME)                            \
    macro(frameset, FRAMESET)                      \
    macro(h1, HEADING)                             \
    macro(h2, HEADING)                             \
    macro(h3, HEADING)                             \
    macro(h4, HEADING)                             \
    macro(h5, HEADING)                             \
    macro(h6, HEADING)                             \
    macro(head, HEAD)                              \
    macro(hr, HR)                                  \
    macro(html, HTML)                              \
    macro(img, IMAGE)                              \
    macro(iframe, IFRAME)                          \
    macro(image, IMAGE)                            \
    macro(input, INPUT)                            \
    macro(ins, MOD)                                \
    macro(isindex, ISINDEX)                        \
    macro(keygen, SELECT)                          \
    macro(label, LABEL)                            \
    macro(legend, LEGEND)                          \
    macro(li, LI)                                  \
    macro(link, LINK)                              \
    macro(listing, PRE)                            \
    macro(map, MAP)                                \
    macro(marquee, MARQUEE)                        \
    macro(menu, MENU)                              \
    macro(meta, META)                              \
    macro(object, OBJECT)                          \
    macro(ol, OLIST)                               \
    macro(optgroup, OPTGROUP)                      \
    macro(option, OPTION)                          \
    macro(p, PARAGRAPH)                            \
    macro(param, PARAM)                            \
    macro(pre, PRE)                                \
    macro(q, QUOTE)                                \
    macro(script, SCRIPT)                          \
    macro(select, SELECT)                          \
    macro(style, STYLE)                            \
    macro(table, TABLE)                            \
    macro(thead, TABLESECTION)                     \
    macro(tbody, TABLESECTION)                     \
    macro(tfoot, TABLESECTION)                     \
    macro(td, TABLECELL)                           \
    macro(th, TABLECELL)                           \
    macro(tr, TABLEROW)                            \
    macro(textarea, TEXTAREA)                      \
    macro(title, TITLE)                            \
    macro(ul, ULIST)                               \
    macro(xmp, PRE)

V8ClassIndex::V8WrapperType V8DOMWrapper::htmlElementType(HTMLElement* element)
{
    typedef HashMap<String, V8ClassIndex::V8WrapperType> WrapperTypeMap;
    DEFINE_STATIC_LOCAL(WrapperTypeMap, wrapperTypeMap, ());
    if (wrapperTypeMap.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
        wrapperTypeMap.set(#tag, V8ClassIndex::HTML##name##ELEMENT);
        FOR_EACH_TAG(ADD_TO_HASH_MAP)
#if ENABLE(VIDEO)
        if (MediaPlayer::isAvailable()) {
            FOR_EACH_VIDEO_TAG(ADD_TO_HASH_MAP)
        }
#endif
#undef ADD_TO_HASH_MAP
    }

    V8ClassIndex::V8WrapperType type = wrapperTypeMap.get(element->localName().impl());
    if (!type)
        return V8ClassIndex::HTMLELEMENT;
    return type;
}
#undef FOR_EACH_TAG

#if ENABLE(SVG)

#if ENABLE(SVG_ANIMATION)
#define FOR_EACH_ANIMATION_TAG(macro) \
    macro(animateColor, ANIMATECOLOR) \
    macro(animate, ANIMATE) \
    macro(animateTransform, ANIMATETRANSFORM) \
    macro(set, SET)
#else
#define FOR_EACH_ANIMATION_TAG(macro)
#endif

#if ENABLE(SVG) && ENABLE(FILTERS)
#define FOR_EACH_FILTERS_TAG(macro) \
    macro(feBlend, FEBLEND) \
    macro(feColorMatrix, FECOLORMATRIX) \
    macro(feComponentTransfer, FECOMPONENTTRANSFER) \
    macro(feComposite, FECOMPOSITE) \
    macro(feDiffuseLighting, FEDIFFUSELIGHTING) \
    macro(feDisplacementMap, FEDISPLACEMENTMAP) \
    macro(feDistantLight, FEDISTANTLIGHT) \
    macro(feFlood, FEFLOOD) \
    macro(feFuncA, FEFUNCA) \
    macro(feFuncB, FEFUNCB) \
    macro(feFuncG, FEFUNCG) \
    macro(feFuncR, FEFUNCR) \
    macro(feGaussianBlur, FEGAUSSIANBLUR) \
    macro(feImage, FEIMAGE) \
    macro(feMerge, FEMERGE) \
    macro(feMergeNode, FEMERGENODE) \
    macro(feMorphology, FEMORPHOLOGY) \
    macro(feOffset, FEOFFSET) \
    macro(fePointLight, FEPOINTLIGHT) \
    macro(feSpecularLighting, FESPECULARLIGHTING) \
    macro(feSpotLight, FESPOTLIGHT) \
    macro(feTile, FETILE) \
    macro(feTurbulence, FETURBULENCE) \
    macro(filter, FILTER)
#else
#define FOR_EACH_FILTERS_TAG(macro)
#endif

#if ENABLE(SVG_FONTS)
#define FOR_EACH_FONTS_TAG(macro) \
    macro(font-face, FONTFACE) \
    macro(font-face-format, FONTFACEFORMAT) \
    macro(font-face-name, FONTFACENAME) \
    macro(font-face-src, FONTFACESRC) \
    macro(font-face-uri, FONTFACEURI)
#else
#define FOR_EACH_FONTS_TAG(marco)
#endif

#if ENABLE(SVG_FOREIGN_OBJECT)
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    macro(foreignObject, FOREIGNOBJECT)
#else
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro)
#endif

#if ENABLE(SVG_USE)
#define FOR_EACH_USE_TAG(macro) \
    macro(use, USE)
#else
#define FOR_EACH_USE_TAG(macro)
#endif

#define FOR_EACH_TAG(macro) \
    FOR_EACH_ANIMATION_TAG(macro) \
    FOR_EACH_FILTERS_TAG(macro) \
    FOR_EACH_FONTS_TAG(macro) \
    FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    FOR_EACH_USE_TAG(macro) \
    macro(a, A) \
    macro(altGlyph, ALTGLYPH) \
    macro(circle, CIRCLE) \
    macro(clipPath, CLIPPATH) \
    macro(cursor, CURSOR) \
    macro(defs, DEFS) \
    macro(desc, DESC) \
    macro(ellipse, ELLIPSE) \
    macro(g, G) \
    macro(glyph, GLYPH) \
    macro(image, IMAGE) \
    macro(linearGradient, LINEARGRADIENT) \
    macro(line, LINE) \
    macro(marker, MARKER) \
    macro(mask, MASK) \
    macro(metadata, METADATA) \
    macro(path, PATH) \
    macro(pattern, PATTERN) \
    macro(polyline, POLYLINE) \
    macro(polygon, POLYGON) \
    macro(radialGradient, RADIALGRADIENT) \
    macro(rect, RECT) \
    macro(script, SCRIPT) \
    macro(stop, STOP) \
    macro(style, STYLE) \
    macro(svg, SVG) \
    macro(switch, SWITCH) \
    macro(symbol, SYMBOL) \
    macro(text, TEXT) \
    macro(textPath, TEXTPATH) \
    macro(title, TITLE) \
    macro(tref, TREF) \
    macro(tspan, TSPAN) \
    macro(view, VIEW) \
    // end of macro

V8ClassIndex::V8WrapperType V8DOMWrapper::svgElementType(SVGElement* element)
{
    typedef HashMap<String, V8ClassIndex::V8WrapperType> WrapperTypeMap;
    DEFINE_STATIC_LOCAL(WrapperTypeMap, wrapperTypeMap, ());
    if (wrapperTypeMap.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
        wrapperTypeMap.set(#tag, V8ClassIndex::SVG##name##ELEMENT);
        FOR_EACH_TAG(ADD_TO_HASH_MAP)
#undef ADD_TO_HASH_MAP
    }

    V8ClassIndex::V8WrapperType type = wrapperTypeMap.get(element->localName().impl());
    if (!type)
        return V8ClassIndex::SVGELEMENT;
    return type;
}
#undef FOR_EACH_TAG

#endif // ENABLE(SVG)

v8::Handle<v8::Value> V8DOMWrapper::convertEventToV8Object(Event* event)
{
    if (!event)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(event);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::EVENT;

    if (event->isUIEvent()) {
        if (event->isKeyboardEvent())
            type = V8ClassIndex::KEYBOARDEVENT;
        else if (event->isTextEvent())
            type = V8ClassIndex::TEXTEVENT;
        else if (event->isMouseEvent())
            type = V8ClassIndex::MOUSEEVENT;
        else if (event->isWheelEvent())
            type = V8ClassIndex::WHEELEVENT;
#if ENABLE(SVG)
        else if (event->isSVGZoomEvent())
            type = V8ClassIndex::SVGZOOMEVENT;
#endif
        else if (event->isCompositionEvent())
            type = V8ClassIndex::COMPOSITIONEVENT;
        else
            type = V8ClassIndex::UIEVENT;
    } else if (event->isMutationEvent())
        type = V8ClassIndex::MUTATIONEVENT;
    else if (event->isOverflowEvent())
        type = V8ClassIndex::OVERFLOWEVENT;
    else if (event->isMessageEvent())
        type = V8ClassIndex::MESSAGEEVENT;
    else if (event->isPageTransitionEvent())
        type = V8ClassIndex::PAGETRANSITIONEVENT;
    else if (event->isPopStateEvent())
        type = V8ClassIndex::POPSTATEEVENT;
    else if (event->isProgressEvent()) {
        if (event->isXMLHttpRequestProgressEvent())
            type = V8ClassIndex::XMLHTTPREQUESTPROGRESSEVENT;
        else
            type = V8ClassIndex::PROGRESSEVENT;
    } else if (event->isWebKitAnimationEvent())
        type = V8ClassIndex::WEBKITANIMATIONEVENT;
    else if (event->isWebKitTransitionEvent())
        type = V8ClassIndex::WEBKITTRANSITIONEVENT;
#if ENABLE(WORKERS)
    else if (event->isErrorEvent())
        type = V8ClassIndex::ERROREVENT;
#endif
#if ENABLE(DOM_STORAGE)
    else if (event->isStorageEvent())
        type = V8ClassIndex::STORAGEEVENT;
#endif
    else if (event->isBeforeLoadEvent())
        type = V8ClassIndex::BEFORELOADEVENT;


    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::EVENT, event);
    if (result.IsEmpty()) {
        // Instantiation failed. Avoid updating the DOM object map and
        // return null which is already handled by callers of this function
        // in case the event is NULL.
        return v8::Null();
    }

    event->ref(); // fast ref
    setJSWrapperForDOMObject(event, v8::Persistent<v8::Object>::New(result));

    return result;
}

static const V8ClassIndex::V8WrapperType mapping[] = {
    V8ClassIndex::INVALID_CLASS_INDEX,    // NONE
    V8ClassIndex::INVALID_CLASS_INDEX,    // ELEMENT_NODE needs special treatment
    V8ClassIndex::ATTR,                   // ATTRIBUTE_NODE
    V8ClassIndex::TEXT,                   // TEXT_NODE
    V8ClassIndex::CDATASECTION,           // CDATA_SECTION_NODE
    V8ClassIndex::ENTITYREFERENCE,        // ENTITY_REFERENCE_NODE
    V8ClassIndex::ENTITY,                 // ENTITY_NODE
    V8ClassIndex::PROCESSINGINSTRUCTION,  // PROCESSING_INSTRUCTION_NODE
    V8ClassIndex::COMMENT,                // COMMENT_NODE
    V8ClassIndex::INVALID_CLASS_INDEX,    // DOCUMENT_NODE needs special treatment
    V8ClassIndex::DOCUMENTTYPE,           // DOCUMENT_TYPE_NODE
    V8ClassIndex::DOCUMENTFRAGMENT,       // DOCUMENT_FRAGMENT_NODE
    V8ClassIndex::NOTATION,               // NOTATION_NODE
    V8ClassIndex::NODE,                   // XPATH_NAMESPACE_NODE
};

v8::Handle<v8::Value> V8DOMWrapper::convertDocumentToV8Object(Document* document)
{
    // Find a proxy for this node.
    //
    // Note that if proxy is found, we might initialize the context which can
    // instantiate a document wrapper.  Therefore, we get the proxy before
    // checking if the node already has a wrapper.
    V8Proxy* proxy = V8Proxy::retrieve(document->frame());
    if (proxy)
        proxy->windowShell()->initContextIfNeeded();

    DOMNodeMapping& domNodeMap = getDOMNodeMap();
    v8::Handle<v8::Object> wrapper = domNodeMap.get(document);
    if (wrapper.IsEmpty())
        return convertNewNodeToV8Object(document, proxy, domNodeMap);

    return wrapper;
}

static v8::Handle<v8::Value> getWrapper(Node* node)
{
    ASSERT(WTF::isMainThread());
    V8IsolatedContext* context = V8IsolatedContext::getEntered();
    if (LIKELY(!context)) {
        v8::Persistent<v8::Object>* wrapper = node->wrapper();
        if (!wrapper)
            return v8::Handle<v8::Value>();
        return *wrapper;
    }

    DOMNodeMapping& domNodeMap = context->world()->domDataStore()->domNodeMap();
    return domNodeMap.get(node);
}

v8::Handle<v8::Value> V8DOMWrapper::convertNodeToV8Object(Node* node)
{
    if (!node)
        return v8::Null();
    
    v8::Handle<v8::Value> wrapper = getWrapper(node);
    if (!wrapper.IsEmpty())
        return wrapper;

    Document* document = node->document();
    if (node == document)
        return convertDocumentToV8Object(document);

    return convertNewNodeToV8Object(node, 0, getDOMNodeMap());
}
    
// Caller checks node is not null.
v8::Handle<v8::Value> V8DOMWrapper::convertNewNodeToV8Object(Node* node, V8Proxy* proxy, DOMNodeMapping& domNodeMap)
{
    if (!proxy && node->document())
        proxy = V8Proxy::retrieve(node->document()->frame());

    bool isDocument = false; // document type node has special handling
    V8ClassIndex::V8WrapperType type;

    Node::NodeType nodeType = node->nodeType();
    if (nodeType == Node::ELEMENT_NODE) {
        if (node->isHTMLElement())
            type = htmlElementType(static_cast<HTMLElement*>(node));
#if ENABLE(SVG)
        else if (node->isSVGElement())
            type = svgElementType(static_cast<SVGElement*>(node));
#endif
        else
            type = V8ClassIndex::ELEMENT;
    } else if (nodeType == Node::DOCUMENT_NODE) {
        isDocument = true;
        Document* document = static_cast<Document*>(node);
        if (document->isHTMLDocument())
            type = V8ClassIndex::HTMLDOCUMENT;
#if ENABLE(SVG)
        else if (document->isSVGDocument())
            type = V8ClassIndex::SVGDOCUMENT;
#endif
        else
            type = V8ClassIndex::DOCUMENT;
    } else {
        ASSERT(nodeType < static_cast<int>(sizeof(mapping)/sizeof(mapping[0])));
        type = mapping[nodeType];
        ASSERT(type != V8ClassIndex::INVALID_CLASS_INDEX);
    }

    v8::Handle<v8::Context> context;
    if (proxy)
        context = proxy->context();

    // Enter the node's context and create the wrapper in that context.
    if (!context.IsEmpty())
        context->Enter();

    v8::Local<v8::Object> result = instantiateV8Object(proxy, type, V8ClassIndex::NODE, node);

    // Exit the node's context if it was entered.
    if (!context.IsEmpty())
        context->Exit();

    if (result.IsEmpty()) {
        // If instantiation failed it's important not to add the result
        // to the DOM node map. Instead we return an empty handle, which
        // should already be handled by callers of this function in case
        // the node is NULL.
        return result;
    }

    node->ref();
    domNodeMap.set(node, v8::Persistent<v8::Object>::New(result));

    if (isDocument) {
        if (proxy)
            proxy->windowShell()->updateDocumentWrapper(result);

        if (type == V8ClassIndex::HTMLDOCUMENT) {
            // Create marker object and insert it in two internal fields.
            // This is used to implement temporary shadowing of
            // document.all.
            ASSERT(result->InternalFieldCount() == V8HTMLDocument::internalFieldCount);
            v8::Local<v8::Object> marker = v8::Object::New();
            result->SetInternalField(V8HTMLDocument::markerIndex, marker);
            result->SetInternalField(V8HTMLDocument::shadowIndex, marker);
        }
    }

    return result;
}

// A JS object of type EventTarget is limited to a small number of possible classes.
// Check EventTarget.h for new type conversion methods
v8::Handle<v8::Value> V8DOMWrapper::convertEventTargetToV8Object(EventTarget* target)
{
    if (!target)
        return v8::Null();

#if ENABLE(SVG)
    SVGElementInstance* instance = target->toSVGElementInstance();
    if (instance)
        return convertToV8Object(V8ClassIndex::SVGELEMENTINSTANCE, instance);
#endif

#if ENABLE(WORKERS)
    Worker* worker = target->toWorker();
    if (worker)
        return convertToV8Object(V8ClassIndex::WORKER, worker);
#endif // WORKERS

#if ENABLE(SHARED_WORKERS)
    SharedWorker* sharedWorker = target->toSharedWorker();
    if (sharedWorker)
        return convertToV8Object(V8ClassIndex::SHAREDWORKER, sharedWorker);
#endif // SHARED_WORKERS

#if ENABLE(NOTIFICATIONS)
    Notification* notification = target->toNotification();
    if (notification)
        return convertToV8Object(V8ClassIndex::NOTIFICATION, notification);
#endif

#if ENABLE(WEB_SOCKETS)
    WebSocket* webSocket = target->toWebSocket();
    if (webSocket)
        return convertToV8Object(V8ClassIndex::WEBSOCKET, webSocket);
#endif

    Node* node = target->toNode();
    if (node)
        return convertNodeToV8Object(node);

    if (DOMWindow* domWindow = target->toDOMWindow())
        return convertToV8Object(V8ClassIndex::DOMWINDOW, domWindow);

    // XMLHttpRequest is created within its JS counterpart.
    XMLHttpRequest* xmlHttpRequest = target->toXMLHttpRequest();
    if (xmlHttpRequest) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(xmlHttpRequest);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    // MessagePort is created within its JS counterpart
    MessagePort* port = target->toMessagePort();
    if (port) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(port);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    XMLHttpRequestUpload* upload = target->toXMLHttpRequestUpload();
    if (upload) {
        v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(upload);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    DOMApplicationCache* domAppCache = target->toDOMApplicationCache();
    if (domAppCache)
        return convertToV8Object(V8ClassIndex::DOMAPPLICATIONCACHE, domAppCache);
#endif

#if ENABLE(EVENTSOURCE)
    EventSource* eventSource = target->toEventSource();
    if (eventSource)
        return convertToV8Object(V8ClassIndex::EVENTSOURCE, eventSource);
#endif

    ASSERT(0);
    return notHandledByInterceptor();
}

v8::Handle<v8::Value> V8DOMWrapper::convertEventListenerToV8Object(ScriptExecutionContext* context, EventListener* listener)
{
    if (!listener)
        return v8::Null();

    // FIXME: can a user take a lazy event listener and set to other places?
    V8AbstractEventListener* v8listener = static_cast<V8AbstractEventListener*>(listener);
    return v8listener->getListenerObject(context);
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(Node* node, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

#if ENABLE(SVG)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(SVGElementInstance* element, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return getEventListener(element->correspondingElement(), value, isAttribute, lookup);
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(AbstractWorker* worker, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (worker->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
    }

    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

#if ENABLE(NOTIFICATIONS)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(Notification* notification, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (notification->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
    }

    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(WorkerContext* workerContext, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    WorkerContextExecutionProxy* workerContextProxy = workerContext->script()->proxy();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);

    return 0;
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(XMLHttpRequestUpload* upload, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return getEventListener(upload->associatedXMLHttpRequest(), value, isAttribute, lookup);
}

#if ENABLE(EVENTSOURCE)
PassRefPtr<EventListener> V8DOMWrapper::getEventListener(EventSource* eventSource, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (V8Proxy::retrieve(eventSource->scriptExecutionContext()))
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);

#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
#endif

    return 0;
}
#endif

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(EventTarget* eventTarget, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    if (V8Proxy::retrieve(eventTarget->scriptExecutionContext()))
        return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);

#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
    if (workerContextProxy)
        return workerContextProxy->findOrCreateEventListener(value, isAttribute, lookup == ListenerFindOnly);
#endif

    return 0;
}

PassRefPtr<EventListener> V8DOMWrapper::getEventListener(V8Proxy* proxy, v8::Local<v8::Value> value, bool isAttribute, ListenerLookupType lookup)
{
    return (lookup == ListenerFindOnly) ? V8EventListenerList::findWrapper(value, isAttribute) : V8EventListenerList::findOrCreateWrapper<V8EventListener>(value, isAttribute);
}

v8::Handle<v8::Value> V8DOMWrapper::convertDOMImplementationToV8Object(DOMImplementation* impl)
{
    v8::Handle<v8::Object> result = instantiateV8Object(V8ClassIndex::DOMIMPLEMENTATION, V8ClassIndex::DOMIMPLEMENTATION, impl);
    if (result.IsEmpty()) {
        // If the instantiation failed, we ignore it and return null instead
        // of returning an empty handle.
        return v8::Null();
    }
    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertStyleSheetToV8Object(StyleSheet* sheet)
{
    if (!sheet)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(sheet);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::STYLESHEET;
    if (sheet->isCSSStyleSheet())
        type = V8ClassIndex::CSSSTYLESHEET;

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::STYLESHEET, sheet);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        sheet->ref();
        setJSWrapperForDOMObject(sheet, v8::Persistent<v8::Object>::New(result));
    }

    // Add a hidden reference from stylesheet object to its owner node.
    Node* ownerNode = sheet->ownerNode();
    if (ownerNode) {
        v8::Handle<v8::Object> owner = v8::Handle<v8::Object>::Cast(convertNodeToV8Object(ownerNode));
        result->SetInternalField(V8StyleSheet::ownerNodeIndex, owner);
    }

    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertCSSValueToV8Object(CSSValue* value)
{
    if (!value)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(value);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type;

    if (value->isWebKitCSSTransformValue())
        type = V8ClassIndex::WEBKITCSSTRANSFORMVALUE;
    else if (value->isValueList())
        type = V8ClassIndex::CSSVALUELIST;
    else if (value->isPrimitiveValue())
        type = V8ClassIndex::CSSPRIMITIVEVALUE;
#if ENABLE(SVG)
    else if (value->isSVGPaint())
        type = V8ClassIndex::SVGPAINT;
    else if (value->isSVGColor())
        type = V8ClassIndex::SVGCOLOR;
#endif
    else
        type = V8ClassIndex::CSSVALUE;

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::CSSVALUE, value);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        value->ref();
        setJSWrapperForDOMObject(value, v8::Persistent<v8::Object>::New(result));
    }

    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertCSSRuleToV8Object(CSSRule* rule)
{
    if (!rule) 
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(rule);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type;

    switch (rule->type()) {
    case CSSRule::STYLE_RULE:
        type = V8ClassIndex::CSSSTYLERULE;
        break;
    case CSSRule::CHARSET_RULE:
        type = V8ClassIndex::CSSCHARSETRULE;
        break;
    case CSSRule::IMPORT_RULE:
        type = V8ClassIndex::CSSIMPORTRULE;
        break;
    case CSSRule::MEDIA_RULE:
        type = V8ClassIndex::CSSMEDIARULE;
        break;
    case CSSRule::FONT_FACE_RULE:
        type = V8ClassIndex::CSSFONTFACERULE;
        break;
    case CSSRule::PAGE_RULE:
        type = V8ClassIndex::CSSPAGERULE;
        break;
    case CSSRule::VARIABLES_RULE:
        type = V8ClassIndex::CSSVARIABLESRULE;
        break;
    case CSSRule::WEBKIT_KEYFRAME_RULE:
        type = V8ClassIndex::WEBKITCSSKEYFRAMERULE;
        break;
    case CSSRule::WEBKIT_KEYFRAMES_RULE:
        type = V8ClassIndex::WEBKITCSSKEYFRAMESRULE;
        break;
    default:  // CSSRule::UNKNOWN_RULE
        type = V8ClassIndex::CSSRULE;
        break;
    }

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::CSSRULE, rule);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        rule->ref();
        setJSWrapperForDOMObject(rule, v8::Persistent<v8::Object>::New(result));
    }
    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertWindowToV8Object(DOMWindow* window)
{
    if (!window)
        return v8::Null();
    // Initializes environment of a frame, and return the global object
    // of the frame.
    Frame* frame = window->frame();
    if (!frame)
        return v8::Handle<v8::Object>();

    // Special case: Because of evaluateInIsolatedWorld() one DOMWindow can have
    // multiple contexts and multiple global objects associated with it. When
    // code running in one of those contexts accesses the window object, we
    // want to return the global object associated with that context, not
    // necessarily the first global object associated with that DOMWindow.
    v8::Handle<v8::Context> currentContext = v8::Context::GetCurrent();
    v8::Handle<v8::Object> currentGlobal = currentContext->Global();
    v8::Handle<v8::Object> windowWrapper = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, currentGlobal);
    if (!windowWrapper.IsEmpty()) {
        if (V8DOMWindow::toNative(windowWrapper) == window)
            return currentGlobal;
    }

    // Otherwise, return the global object associated with this frame.
    v8::Handle<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return v8::Handle<v8::Object>();

    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

v8::Handle<v8::Value> V8DOMWrapper::convertNamedNodeMapToV8Object(NamedNodeMap* map)
{
    if (!map)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(map);
    if (!wrapper.IsEmpty())
        return wrapper;

    v8::Handle<v8::Object> result = instantiateV8Object(V8ClassIndex::NAMEDNODEMAP, V8ClassIndex::NAMEDNODEMAP, map);
    if (result.IsEmpty())
        return result;

    // Only update the DOM object map if the result is non-empty.
    map->ref();
    setJSWrapperForDOMObject(map, v8::Persistent<v8::Object>::New(result));

    // Add a hidden reference from named node map to its owner node.
    if (Element* element = map->element()) {
        v8::Handle<v8::Object> owner = v8::Handle<v8::Object>::Cast(convertNodeToV8Object(element));
        result->SetInternalField(V8NamedNodeMap::ownerNodeIndex, owner);
    }

    return result;
}

}  // namespace WebCore
