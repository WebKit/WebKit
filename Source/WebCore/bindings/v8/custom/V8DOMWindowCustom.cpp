/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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
#include "V8DOMWindow.h"

#include "BindingState.h"
#include "Chrome.h"
#include "ContentSecurityPolicy.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "Location.h"
#include "MediaPlayer.h"
#include "MessagePort.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "ScheduledAction.h"
#include "ScriptCallStack.h"
#include "ScriptSourceCode.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "SharedWorkerRepository.h"
#include "Storage.h"
#include "V8Binding.h"
#include "V8EventListener.h"
#include "V8GCForContextDispose.h"
#include "V8HiddenPropertyName.h"
#include "V8HTMLCollection.h"
#include "V8Node.h"
#include "V8Utilities.h"
#include "WindowFeatures.h"
#include <wtf/ArrayBuffer.h>

namespace WebCore {

v8::Handle<v8::Value> WindowSetTimeoutImpl(const v8::Arguments& args, bool singleShot)
{
    int argumentCount = args.Length();

    if (argumentCount < 1)
        return v8::Undefined();

    DOMWindow* imp = V8DOMWindow::toNative(args.Holder());
    ScriptExecutionContext* scriptContext = static_cast<ScriptExecutionContext*>(imp->document());

    if (!scriptContext)
        return setDOMException(INVALID_ACCESS_ERR, args.GetIsolate());

    v8::Handle<v8::Value> function = args[0];
    WTF::String functionString;
    if (!function->IsFunction()) {
        if (function->IsString())
            functionString = toWebCoreString(function);
        else {
            v8::Handle<v8::Value> v8String = function->ToString();

            // Bail out if string conversion failed.
            if (v8String.IsEmpty())
                return v8::Undefined();

            functionString = toWebCoreString(v8String);
        }

        // Don't allow setting timeouts to run empty functions!
        // (Bug 1009597)
        if (functionString.length() == 0)
            return v8::Undefined();
    }

    int32_t timeout = 0;
    if (argumentCount >= 2)
        timeout = args[1]->Int32Value();

    if (!BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), imp->frame()))
        return v8::Undefined();

    int id;
    if (function->IsFunction()) {
        int paramCount = argumentCount >= 2 ? argumentCount - 2 : 0;
        v8::Local<v8::Value>* params = 0;
        if (paramCount > 0) {
            params = new v8::Local<v8::Value>[paramCount];
            for (int i = 0; i < paramCount; i++)
                // parameters must be globalized
                params[i] = args[i+2];
        }

        // params is passed to action, and released in action's destructor
        ASSERT(imp->frame());
        OwnPtr<ScheduledAction> action = adoptPtr(new ScheduledAction(imp->frame()->script()->currentWorldContext(), v8::Handle<v8::Function>::Cast(function), paramCount, params));

        // FIXME: We should use OwnArrayPtr for params.
        delete[] params;

        id = DOMTimer::install(scriptContext, action.release(), timeout, singleShot);
    } else {
        if (imp->document() && !imp->document()->contentSecurityPolicy()->allowEval())
            return v8Integer(0, args.GetIsolate());
        ASSERT(imp->frame());
        id = DOMTimer::install(scriptContext, adoptPtr(new ScheduledAction(imp->frame()->script()->currentWorldContext(), functionString)), timeout, singleShot);
    }

    // Try to do the idle notification before the timeout expires to get better
    // use of any idle time. Aim for the middle of the interval for simplicity.
    if (timeout > 0) {
        double maximumFireInterval = static_cast<double>(timeout) / 1000 / 2;
        V8GCForContextDispose::instance().notifyIdleSooner(maximumFireInterval);
    }

    return v8Integer(id, args.GetIsolate());
}

v8::Handle<v8::Value> V8DOMWindow::eventAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate());
    if (holder.IsEmpty())
        return v8::Undefined();

    Frame* frame = V8DOMWindow::toNative(holder)->frame();
    if (!BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), frame))
        return v8::Undefined();

    ASSERT(frame);
    v8::Local<v8::Context> context = frame->script()->currentWorldContext();
    if (context.IsEmpty())
        return v8::Undefined();

    v8::Handle<v8::String> eventSymbol = V8HiddenPropertyName::event();
    v8::Handle<v8::Value> jsEvent = context->Global()->GetHiddenValue(eventSymbol);
    if (jsEvent.IsEmpty())
        return v8::Undefined();
    return jsEvent;
}

void V8DOMWindow::eventAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate());
    if (holder.IsEmpty())
        return;

    Frame* frame = V8DOMWindow::toNative(holder)->frame();
    if (!BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), frame))
        return;

    ASSERT(frame);
    v8::Local<v8::Context> context = frame->script()->currentWorldContext();
    if (context.IsEmpty())
        return;

    v8::Handle<v8::String> eventSymbol = V8HiddenPropertyName::event();
    context->Global()->SetHiddenValue(eventSymbol, value);
}

void V8DOMWindow::locationAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    DOMWindow* imp = V8DOMWindow::toNative(info.Holder());
    BindingState* state = BindingState::instance();

    DOMWindow* active = activeDOMWindow(state);
    if (!active)
      return;

    DOMWindow* first = firstDOMWindow(state);
    if (!first)
      return;

    if (Location* location = imp->location())
        location->setHref(toWebCoreString(value), active, first);
}

void V8DOMWindow::openerAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    DOMWindow* imp = V8DOMWindow::toNative(info.Holder());

    if (!BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), imp->frame()))
        return;

    // Opener can be shadowed if it is in the same domain.
    // Have a special handling of null value to behave
    // like Firefox. See bug http://b/1224887 & http://b/791706.
    if (value->IsNull()) {
        // imp->frame() cannot be null,
        // otherwise, SameOrigin check would have failed.
        ASSERT(imp->frame());
        imp->frame()->loader()->setOpener(0);
    }

    // Delete the accessor from this object.
    info.Holder()->Delete(name);

    // Put property on the front (this) object.
    info.This()->Set(name, value);
}

v8::Handle<v8::Value> V8DOMWindow::addEventListenerCallback(const v8::Arguments& args)
{
    String eventType = toWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();

    DOMWindow* imp = V8DOMWindow::toNative(args.Holder());

    if (!BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), imp->frame()))
        return v8::Undefined();

    Document* doc = imp->document();

    if (!doc)
        return v8::Undefined();

    // FIXME: Check if there is not enough arguments
    if (!imp->frame())
        return v8::Undefined();

    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(args[1], false, ListenerFindOrCreate);

    if (listener) {
        imp->addEventListener(eventType, listener, useCapture);
        createHiddenDependency(args.Holder(), args[1], eventListenerCacheIndex);
    }

    return v8::Undefined();
}


v8::Handle<v8::Value> V8DOMWindow::removeEventListenerCallback(const v8::Arguments& args)
{
    String eventType = toWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();

    DOMWindow* imp = V8DOMWindow::toNative(args.Holder());

    if (!BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), imp->frame()))
        return v8::Undefined();

    Document* doc = imp->document();

    if (!doc)
        return v8::Undefined();

    if (!imp->frame())
        return v8::Undefined();

    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(args[1], false, ListenerFindOnly);

    if (listener) {
        imp->removeEventListener(eventType, listener.get(), useCapture);
        removeHiddenDependency(args.Holder(), args[1], eventListenerCacheIndex);
    }

    return v8::Undefined();
}

static bool isLegacyTargetOriginDesignation(v8::Handle<v8::Value> value)
{
    if (value->IsString() || value->IsStringObject())
        return true;
    return false;
}


v8::Handle<v8::Value> V8DOMWindow::postMessageCallback(const v8::Arguments& args)
{
    // None of these need to be RefPtr because args and context are guaranteed
    // to hold on to them.
    DOMWindow* window = V8DOMWindow::toNative(args.Holder());
    DOMWindow* source = activeDOMWindow(BindingState::instance());

    // If called directly by WebCore we don't have a calling context.
    if (!source)
        return throwTypeError(0, args.GetIsolate());

    // This function has variable arguments and can be:
    // Per current spec:
    //   postMessage(message, targetOrigin)
    //   postMessage(message, targetOrigin, {sequence of transferrables})
    // Legacy non-standard implementations in webkit allowed:
    //   postMessage(message, {sequence of transferrables}, targetOrigin);
    MessagePortArray portArray;
    ArrayBufferArray arrayBufferArray;
    int targetOriginArgIndex = 1;
    if (args.Length() > 2) {
        int transferablesArgIndex = 2;
        if (isLegacyTargetOriginDesignation(args[2])) {
            targetOriginArgIndex = 2;
            transferablesArgIndex = 1;
        }
        if (!extractTransferables(args[transferablesArgIndex], portArray, arrayBufferArray, args.GetIsolate()))
            return v8::Undefined();
    }
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<WithUndefinedOrNullCheck>, targetOrigin, args[targetOriginArgIndex]);

    bool didThrow = false;
    RefPtr<SerializedScriptValue> message =
        SerializedScriptValue::create(args[0],
                                      &portArray,
                                      &arrayBufferArray,
                                      didThrow,
                                      args.GetIsolate());
    if (didThrow)
        return v8::Undefined();

    ExceptionCode ec = 0;
    window->postMessage(message.release(), &portArray, targetOrigin, source, ec);
    return setDOMException(ec, args.GetIsolate());
}

// FIXME(fqian): returning string is cheating, and we should
// fix this by calling toString function on the receiver.
// However, V8 implements toString in JavaScript, which requires
// switching context of receiver. I consider it is dangerous.
v8::Handle<v8::Value> V8DOMWindow::toStringCallback(const v8::Arguments& args)
{
    v8::Handle<v8::Object> domWrapper = args.This()->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate());
    if (domWrapper.IsEmpty())
        return args.This()->ObjectProtoToString();
    return domWrapper->ObjectProtoToString();
}

v8::Handle<v8::Value> V8DOMWindow::releaseEventsCallback(const v8::Arguments& args)
{
    return v8::Undefined();
}

v8::Handle<v8::Value> V8DOMWindow::captureEventsCallback(const v8::Arguments& args)
{
    return v8::Undefined();
}

class DialogHandler {
public:
    explicit DialogHandler(v8::Handle<v8::Value> dialogArguments)
        : m_dialogArguments(dialogArguments)
    {
    }

    void dialogCreated(DOMWindow*);
    v8::Handle<v8::Value> returnValue() const;

private:
    v8::Handle<v8::Value> m_dialogArguments;
    v8::Handle<v8::Context> m_dialogContext;
};

inline void DialogHandler::dialogCreated(DOMWindow* dialogFrame)
{
    m_dialogContext = dialogFrame->frame() ? dialogFrame->frame()->script()->currentWorldContext() : v8::Local<v8::Context>();
    if (m_dialogContext.IsEmpty())
        return;
    if (m_dialogArguments.IsEmpty())
        return;
    v8::Context::Scope scope(m_dialogContext);
    m_dialogContext->Global()->Set(v8::String::NewSymbol("dialogArguments"), m_dialogArguments);
}

inline v8::Handle<v8::Value> DialogHandler::returnValue() const
{
    if (m_dialogContext.IsEmpty())
        return v8::Undefined();
    v8::Context::Scope scope(m_dialogContext);
    v8::Handle<v8::Value> returnValue = m_dialogContext->Global()->Get(v8::String::NewSymbol("returnValue"));
    if (returnValue.IsEmpty())
        return v8::Undefined();
    return returnValue;
}

static void setUpDialog(DOMWindow* dialog, void* handler)
{
    static_cast<DialogHandler*>(handler)->dialogCreated(dialog);
}

v8::Handle<v8::Value> V8DOMWindow::showModalDialogCallback(const v8::Arguments& args)
{
    DOMWindow* impl = V8DOMWindow::toNative(args.Holder());
    BindingState* state = BindingState::instance();
    if (!BindingSecurity::shouldAllowAccessToFrame(state, impl->frame()))
        return v8::Undefined();

    // FIXME: Handle exceptions properly.
    String urlString = toWebCoreStringWithUndefinedOrNullCheck(args[0]);
    DialogHandler handler(args[1]);
    String dialogFeaturesString = toWebCoreStringWithUndefinedOrNullCheck(args[2]);

    impl->showModalDialog(urlString, dialogFeaturesString, activeDOMWindow(state), firstDOMWindow(state), setUpDialog, &handler);

    return handler.returnValue();
}

v8::Handle<v8::Value> V8DOMWindow::openCallback(const v8::Arguments& args)
{
    DOMWindow* impl = V8DOMWindow::toNative(args.Holder());
    BindingState* state = BindingState::instance();
    if (!BindingSecurity::shouldAllowAccessToFrame(state, impl->frame()))
        return v8::Undefined();

    // FIXME: Handle exceptions properly.
    String urlString = toWebCoreStringWithUndefinedOrNullCheck(args[0]);
    AtomicString frameName = (args[1]->IsUndefined() || args[1]->IsNull()) ? "_blank" : AtomicString(toWebCoreString(args[1]));
    String windowFeaturesString = toWebCoreStringWithUndefinedOrNullCheck(args[2]);

    RefPtr<DOMWindow> openedWindow = impl->open(urlString, frameName, windowFeaturesString, activeDOMWindow(state), firstDOMWindow(state));
    if (!openedWindow)
        return v8::Undefined();

    return toV8(openedWindow.release(), args.Holder(), args.GetIsolate());
}

v8::Handle<v8::Value> V8DOMWindow::indexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{

    DOMWindow* window = V8DOMWindow::toNative(info.Holder());
    if (!window)
        return v8Undefined();

    Frame* frame = window->frame();
    if (!frame)
        return v8Undefined();

    Frame* child = frame->tree()->scopedChild(index);
    if (child)
        return toV8(child->document()->domWindow(), info.Holder(), info.GetIsolate());

    return v8Undefined();
}

v8::Handle<v8::Value> V8DOMWindow::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{

    DOMWindow* window = V8DOMWindow::toNative(info.Holder());
    if (!window)
        return v8Undefined();

    Frame* frame = window->frame();
    // window is detached from a frame.
    if (!frame)
        return v8Undefined();

    // Search sub-frames.
    AtomicString propName = toWebCoreAtomicString(name);
    Frame* child = frame->tree()->scopedChild(propName);
    if (child)
        return toV8(child->document()->domWindow(), info.Holder(), info.GetIsolate());

    // Search IDL functions defined in the prototype
    if (!info.Holder()->GetRealNamedProperty(name).IsEmpty())
        return v8Undefined();

    // Search named items in the document.
    Document* doc = frame->document();

    if (doc && doc->isHTMLDocument()) {
        if (static_cast<HTMLDocument*>(doc)->hasNamedItem(propName.impl()) || doc->hasElementWithId(propName.impl())) {
            RefPtr<HTMLCollection> items = doc->windowNamedItems(propName);
            if (!items->isEmpty()) {
                if (items->hasExactlyOneItem())
                    return toV8(items->item(0), info.Holder(), info.GetIsolate());
                return toV8(items.release(), info.Holder(), info.GetIsolate());
            }
        }
    }

    return v8Undefined();
}


v8::Handle<v8::Value> V8DOMWindow::setTimeoutCallback(const v8::Arguments& args)
{
    return WindowSetTimeoutImpl(args, true);
}


v8::Handle<v8::Value> V8DOMWindow::setIntervalCallback(const v8::Arguments& args)
{
    return WindowSetTimeoutImpl(args, false);
}

bool V8DOMWindow::namedSecurityCheck(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    v8::Handle<v8::Object> window = host->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate());
    if (window.IsEmpty())
        return false;  // the frame is gone.

    DOMWindow* targetWindow = V8DOMWindow::toNative(window);

    ASSERT(targetWindow);

    Frame* target = targetWindow->frame();
    if (!target)
        return false;

    if (key->IsString()) {
        DEFINE_STATIC_LOCAL(AtomicString, nameOfProtoProperty, ("__proto__", AtomicString::ConstructFromLiteral));

        String name = toWebCoreString(key);
        Frame* childFrame = target->tree()->scopedChild(name);
        // Notice that we can't call HasRealNamedProperty for ACCESS_HAS
        // because that would generate infinite recursion.
        if (type == v8::ACCESS_HAS && childFrame)
            return true;
        // We need to explicitly compare against nameOfProtoProperty because
        // V8's JSObject::LocalLookup finds __proto__ before
        // interceptors and even when __proto__ isn't a "real named property".
        if (type == v8::ACCESS_GET && childFrame && !host->HasRealNamedProperty(key->ToString()) && name != nameOfProtoProperty)
            return true;
    }

    return BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), target, DoNotReportSecurityError);
}

bool V8DOMWindow::indexedSecurityCheck(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    v8::Handle<v8::Object> window = host->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate());
    if (window.IsEmpty())
        return false;

    DOMWindow* targetWindow = V8DOMWindow::toNative(window);

    ASSERT(targetWindow);

    Frame* target = targetWindow->frame();
    if (!target)
        return false;
    Frame* childFrame =  target->tree()->scopedChild(index);

    // Notice that we can't call HasRealNamedProperty for ACCESS_HAS
    // because that would generate infinite recursion.
    if (type == v8::ACCESS_HAS && childFrame)
        return true;
    if (type == v8::ACCESS_GET && childFrame && !host->HasRealIndexedProperty(index))
        return true;

    return BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), target, DoNotReportSecurityError);
}

v8::Handle<v8::Value> toV8(DOMWindow* window, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    // Notice that we explicitly ignore creationContext because the DOMWindow is its own creationContext.

    if (!window)
        return v8NullWithCheck(isolate);
    // Initializes environment of a frame, and return the global object
    // of the frame.
    Frame* frame = window->frame();
    if (!frame)
        return v8Undefined();

    // Special case: Because of evaluateInIsolatedWorld() one DOMWindow can have
    // multiple contexts and multiple global objects associated with it. When
    // code running in one of those contexts accesses the window object, we
    // want to return the global object associated with that context, not
    // necessarily the first global object associated with that DOMWindow.
    v8::Handle<v8::Context> currentContext = v8::Context::GetCurrent();
    v8::Handle<v8::Object> currentGlobal = currentContext->Global();
    v8::Handle<v8::Object> windowWrapper = currentGlobal->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate());
    if (!windowWrapper.IsEmpty()) {
        if (V8DOMWindow::toNative(windowWrapper) == window)
            return currentGlobal;
    }

    // Otherwise, return the global object associated with this frame.
    v8::Handle<v8::Context> context = frame->script()->currentWorldContext();
    if (context.IsEmpty())
        return v8Undefined();

    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

} // namespace WebCore
