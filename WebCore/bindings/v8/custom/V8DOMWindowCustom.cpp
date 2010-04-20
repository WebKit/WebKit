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
#include "V8DOMWindow.h"

#include "Chrome.h"
#include "Database.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "MediaPlayer.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "SharedWorkerRepository.h"
#include "Storage.h"
#include "V8Binding.h"
#include "V8BindingDOMWindow.h"
#include "V8BindingState.h"
#include "V8CustomEventListener.h"
#include "V8Database.h"
#include "V8DatabaseCallback.h"
#include "V8GCForContextDispose.h"
#include "V8HTMLAudioElementConstructor.h"
#include "V8HTMLCollection.h"
#include "V8HTMLImageElementConstructor.h"
#include "V8HTMLOptionElementConstructor.h"
#include "V8MessagePortCustom.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#if ENABLE(WEB_SOCKETS)
#include "WebSocket.h"
#endif
#include "WindowFeatures.h"

// Horizontal and vertical offset, from the parent content area, around newly
// opened popups that don't specify a location.
static const int popupTilePixels = 10;

namespace WebCore {

v8::Handle<v8::Value> WindowSetTimeoutImpl(const v8::Arguments& args, bool singleShot)
{
    int argumentCount = args.Length();

    if (argumentCount < 1)
        return v8::Undefined();

    DOMWindow* imp = V8DOMWindow::toNative(args.Holder());
    ScriptExecutionContext* scriptContext = static_cast<ScriptExecutionContext*>(imp->document());

    if (!scriptContext) {
        V8Proxy::setDOMException(INVALID_ACCESS_ERR);
        return v8::Undefined();
    }

    v8::Handle<v8::Value> function = args[0];
    WebCore::String functionString;
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

    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), true))
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
        ScheduledAction* action = new ScheduledAction(V8Proxy::context(imp->frame()), v8::Handle<v8::Function>::Cast(function), paramCount, params);

        delete[] params;

        id = DOMTimer::install(scriptContext, action, timeout, singleShot);
    } else {
        id = DOMTimer::install(scriptContext, new ScheduledAction(V8Proxy::context(imp->frame()), functionString), timeout, singleShot);
    }

    // Try to do the idle notification before the timeout expires to get better
    // use of any idle time. Aim for the middle of the interval for simplicity.
    if (timeout > 0) {
        double maximumFireInterval = static_cast<double>(timeout) / 1000 / 2;
        V8GCForContextDispose::instance().notifyIdleSooner(maximumFireInterval);
    }

    return v8::Integer::New(id);
}

v8::Handle<v8::Value> V8DOMWindow::eventAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), info.This());
    if (holder.IsEmpty())
        return v8::Undefined();

    Frame* frame = V8DOMWindow::toNative(holder)->frame();
    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), frame, true))
        return v8::Undefined();

    v8::Local<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return v8::Undefined();

    v8::Local<v8::String> eventSymbol = v8::String::NewSymbol("event");
    v8::Handle<v8::Value> jsEvent = context->Global()->GetHiddenValue(eventSymbol);
    if (jsEvent.IsEmpty())
        return v8::Undefined();
    return jsEvent;
}

void V8DOMWindow::eventAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), info.This());
    if (holder.IsEmpty())
        return;

    Frame* frame = V8DOMWindow::toNative(holder)->frame();
    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), frame, true))
        return;

    v8::Local<v8::Context> context = V8Proxy::context(frame);
    if (context.IsEmpty())
        return;

    v8::Local<v8::String> eventSymbol = v8::String::NewSymbol("event");
    context->Global()->SetHiddenValue(eventSymbol, value);
}

v8::Handle<v8::Value> V8DOMWindow::cryptoAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    // FIXME: Implement me.
    return v8::Undefined();
}

void V8DOMWindow::locationAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    DOMWindow* imp = V8DOMWindow::toNative(info.Holder());
    V8DOMWindowShell::setLocation(imp, toWebCoreString(value));
}


void V8DOMWindow::openerAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    DOMWindow* imp = V8DOMWindow::toNative(info.Holder());

    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), true))
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

#if ENABLE(VIDEO)

v8::Handle<v8::Value> V8DOMWindow::AudioAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    DOMWindow* window = V8DOMWindow::toNative(info.Holder());
    return V8DOMWrapper::getConstructor(&V8HTMLAudioElementConstructor::info, window);
}

#endif

v8::Handle<v8::Value> V8DOMWindow::ImageAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    DOMWindow* window = V8DOMWindow::toNative(info.Holder());
    return V8DOMWrapper::getConstructor(&V8HTMLImageElementConstructor::info, window);
}

v8::Handle<v8::Value> V8DOMWindow::OptionAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    DOMWindow* window = V8DOMWindow::toNative(info.Holder());
    return V8DOMWrapper::getConstructor(&V8HTMLOptionElementConstructor::info, window);
}

v8::Handle<v8::Value> V8DOMWindow::addEventListenerCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.addEventListener()");

    String eventType = toWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();

    DOMWindow* imp = V8DOMWindow::toNative(args.Holder());

    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), true))
        return v8::Undefined();

    Document* doc = imp->document();

    if (!doc)
        return v8::Undefined();

    // FIXME: Check if there is not enough arguments
    V8Proxy* proxy = V8Proxy::retrieve(imp->frame());
    if (!proxy)
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
    INC_STATS("DOM.DOMWindow.removeEventListener()");

    String eventType = toWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();

    DOMWindow* imp = V8DOMWindow::toNative(args.Holder());

    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), true))
        return v8::Undefined();

    Document* doc = imp->document();

    if (!doc)
        return v8::Undefined();

    V8Proxy* proxy = V8Proxy::retrieve(imp->frame());
    if (!proxy)
        return v8::Undefined();

    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(args[1], false, ListenerFindOnly);

    if (listener) {
        imp->removeEventListener(eventType, listener.get(), useCapture);
        removeHiddenDependency(args.Holder(), args[1], eventListenerCacheIndex);
    }

    return v8::Undefined();
}

v8::Handle<v8::Value> V8DOMWindow::postMessageCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.postMessage()");
    DOMWindow* window = V8DOMWindow::toNative(args.Holder());

    DOMWindow* source = V8Proxy::retrieveFrameForCallingContext()->domWindow();
    ASSERT(source->frame());

    bool didThrow = false;
    RefPtr<SerializedScriptValue> message = SerializedScriptValue::create(args[0], didThrow);
    if (didThrow)
        return v8::Undefined();

    MessagePortArray portArray;
    String targetOrigin;

    // This function has variable arguments and can either be:
    //   postMessage(message, port, targetOrigin);
    // or
    //   postMessage(message, targetOrigin);
    v8::TryCatch tryCatch;
    if (args.Length() > 2) {
        if (!getMessagePortArray(args[1], portArray))
            return v8::Undefined();
        targetOrigin = toWebCoreStringWithNullOrUndefinedCheck(args[2]);
    } else {
        targetOrigin = toWebCoreStringWithNullOrUndefinedCheck(args[1]);
    }

    if (tryCatch.HasCaught())
        return v8::Undefined();

    ExceptionCode ec = 0;
    window->postMessage(message.release(), &portArray, targetOrigin, source, ec);
    return throwError(ec);
}

// FIXME(fqian): returning string is cheating, and we should
// fix this by calling toString function on the receiver.
// However, V8 implements toString in JavaScript, which requires
// switching context of receiver. I consider it is dangerous.
v8::Handle<v8::Value> V8DOMWindow::toStringCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.toString()");
    v8::Handle<v8::Object> domWrapper = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), args.This());
    if (domWrapper.IsEmpty())
        return args.This()->ObjectProtoToString();
    return domWrapper->ObjectProtoToString();
}

v8::Handle<v8::Value> V8DOMWindow::releaseEventsCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.nop()");
    return v8::Undefined();
}

v8::Handle<v8::Value> V8DOMWindow::captureEventsCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.nop()");
    return v8::Undefined();
}

static bool canShowModalDialogNow(const Frame* frame)
{
    // A frame can out live its page. See bug 1219613.
    if (!frame || !frame->page())
        return false;
    return frame->page()->chrome()->canRunModalNow();
}

static bool allowPopUp()
{
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();

    ASSERT(frame);
    if (frame->script()->processingUserGesture())
        return true;
    Settings* settings = frame->settings();
    return settings && settings->javaScriptCanOpenWindowsAutomatically();
}

static HashMap<String, String> parseModalDialogFeatures(const String& featuresArg)
{
    HashMap<String, String> map;

    Vector<String> features;
    featuresArg.split(';', features);
    Vector<String>::const_iterator end = features.end();
    for (Vector<String>::const_iterator it = features.begin(); it != end; ++it) {
        String featureString = *it;
        int pos = featureString.find('=');
        int colonPos = featureString.find(':');
        if (pos >= 0 && colonPos >= 0)
            continue;  // ignore any strings that have both = and :
        if (pos < 0)
            pos = colonPos;
        if (pos < 0) {
            // null string for value means key without value
            map.set(featureString.stripWhiteSpace().lower(), String());
        } else {
            String key = featureString.left(pos).stripWhiteSpace().lower();
            String val = featureString.substring(pos + 1).stripWhiteSpace().lower();
            int spacePos = val.find(' ');
            if (spacePos != -1)
                val = val.left(spacePos);
            map.set(key, val);
        }
    }

    return map;
}

v8::Handle<v8::Value> V8DOMWindow::showModalDialogCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.showModalDialog()");

    String url = toWebCoreStringWithNullOrUndefinedCheck(args[0]);
    v8::Local<v8::Value> dialogArgs = args[1];
    String featureArgs = toWebCoreStringWithNullOrUndefinedCheck(args[2]);

    DOMWindow* window = V8DOMWindow::toNative(args.Holder());
    Frame* frame = window->frame();

    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), frame, true))
        return v8::Undefined();

    Frame* callingFrame = V8Proxy::retrieveFrameForCallingContext();
    if (!callingFrame)
        return v8::Undefined();

    Frame* enteredFrame = V8Proxy::retrieveFrameForEnteredContext();
    if (!enteredFrame)
        return v8::Undefined();

    if (!canShowModalDialogNow(frame) || !allowPopUp())
        return v8::Undefined();

    const HashMap<String, String> features = parseModalDialogFeatures(featureArgs);

    const bool trusted = false;

    FloatRect screenRect = screenAvailableRect(frame->view());

    WindowFeatures windowFeatures;
    // default here came from frame size of dialog in MacIE.
    windowFeatures.width = WindowFeatures::floatFeature(features, "dialogwidth", 100, screenRect.width(), 620);
    windowFeatures.widthSet = true;
    // default here came from frame size of dialog in MacIE.
    windowFeatures.height = WindowFeatures::floatFeature(features, "dialogheight", 100, screenRect.height(), 450);
    windowFeatures.heightSet = true;

    windowFeatures.x = WindowFeatures::floatFeature(features, "dialogleft", screenRect.x(), screenRect.right() - windowFeatures.width, -1);
    windowFeatures.xSet = windowFeatures.x > 0;
    windowFeatures.y = WindowFeatures::floatFeature(features, "dialogtop", screenRect.y(), screenRect.bottom() - windowFeatures.height, -1);
    windowFeatures.ySet = windowFeatures.y > 0;

    if (WindowFeatures::boolFeature(features, "center", true)) {
        if (!windowFeatures.xSet) {
            windowFeatures.x = screenRect.x() + (screenRect.width() - windowFeatures.width) / 2;
            windowFeatures.xSet = true;
        }
        if (!windowFeatures.ySet) {
            windowFeatures.y = screenRect.y() + (screenRect.height() - windowFeatures.height) / 2;
            windowFeatures.ySet = true;
        }
    }

    windowFeatures.dialog = true;
    windowFeatures.resizable = WindowFeatures::boolFeature(features, "resizable");
    windowFeatures.scrollbarsVisible = WindowFeatures::boolFeature(features, "scroll", true);
    windowFeatures.statusBarVisible = WindowFeatures::boolFeature(features, "status", !trusted);
    windowFeatures.menuBarVisible = false;
    windowFeatures.toolBarVisible = false;
    windowFeatures.locationBarVisible = false;
    windowFeatures.fullscreen = false;

    Frame* dialogFrame = V8BindingDOMWindow::createWindow(V8BindingState::Only(), callingFrame, enteredFrame, frame, url, "", windowFeatures, dialogArgs);
    if (!dialogFrame)
        return v8::Undefined();

    // Hold on to the context of the dialog window long enough to retrieve the
    // value of the return value property.
    v8::Local<v8::Context> context = V8Proxy::context(dialogFrame);

    // Run the dialog.
    dialogFrame->page()->chrome()->runModal();

    // Extract the return value property from the dialog window.
    v8::Local<v8::Value> returnValue;
    if (!context.IsEmpty()) {
        v8::Context::Scope scope(context);
        returnValue = context->Global()->Get(v8::String::New("returnValue"));
    }

    if (!returnValue.IsEmpty())
        return returnValue;

    return v8::Undefined();
}


v8::Handle<v8::Value> V8DOMWindow::openCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.open()");

    String urlString = toWebCoreStringWithNullOrUndefinedCheck(args[0]);
    AtomicString frameName = (args[1]->IsUndefined() || args[1]->IsNull()) ? "_blank" : AtomicString(toWebCoreString(args[1]));

    DOMWindow* parent = V8DOMWindow::toNative(args.Holder());
    Frame* frame = parent->frame();

    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), frame, true))
        return v8::Undefined();

    Frame* enteredFrame = V8Proxy::retrieveFrameForEnteredContext();
    if (!enteredFrame)
        return v8::Undefined();

    Frame* callingFrame = V8Proxy::retrieveFrameForCallingContext();
    // We may not have a calling context if we are invoked by a plugin via NPAPI.
    if (!callingFrame)
        callingFrame = enteredFrame;

    Page* page = frame->page();
    if (!page)
        return v8::Undefined();

    // Because FrameTree::find() returns true for empty strings, we must check
    // for empty framenames. Otherwise, illegitimate window.open() calls with
    // no name will pass right through the popup blocker.
    if (!allowPopUp() &&
        (frameName.isEmpty() || !frame->tree()->find(frameName))) {
        return v8::Undefined();
    }

    // Get the target frame for the special cases of _top and _parent.  In those
    // cases, we can schedule a location change right now and return early.
    bool topOrParent = false;
    if (frameName == "_top") {
        frame = frame->tree()->top();
        topOrParent = true;
    } else if (frameName == "_parent") {
        if (Frame* parent = frame->tree()->parent())
            frame = parent;
        topOrParent = true;
    }
    if (topOrParent) {
        if (!shouldAllowNavigation(frame))
            return v8::Undefined();

        String completedUrl;
        if (!urlString.isEmpty())
            completedUrl = completeURL(urlString);

        if (!completedUrl.isEmpty() &&
            (!protocolIsJavaScript(completedUrl) || ScriptController::isSafeScript(frame))) {
            bool userGesture = processingUserGesture();

            // For whatever reason, Firefox uses the entered frame to determine
            // the outgoingReferrer.  We replicate that behavior here.
            String referrer = enteredFrame->loader()->outgoingReferrer();

            frame->redirectScheduler()->scheduleLocationChange(completedUrl, referrer, false, userGesture);
        }
        return toV8(frame->domWindow());
    }

    // In the case of a named frame or a new window, we'll use the
    // createWindow() helper.

    // Parse the values, and then work with a copy of the parsed values
    // so we can restore the values we may not want to overwrite after
    // we do the multiple monitor fixes.
    WindowFeatures rawFeatures(toWebCoreStringWithNullOrUndefinedCheck(args[2]));
    WindowFeatures windowFeatures(rawFeatures);
    FloatRect screenRect = screenAvailableRect(page->mainFrame()->view());

    // Set default size and location near parent window if none were specified.
    // These may be further modified by adjustWindowRect, below.
    if (!windowFeatures.xSet) {
        windowFeatures.x = parent->screenX() - screenRect.x() + popupTilePixels;
        windowFeatures.xSet = true;
    }
    if (!windowFeatures.ySet) {
        windowFeatures.y = parent->screenY() - screenRect.y() + popupTilePixels;
        windowFeatures.ySet = true;
    }
    if (!windowFeatures.widthSet) {
        windowFeatures.width = parent->innerWidth();
        windowFeatures.widthSet = true;
    }
    if (!windowFeatures.heightSet) {
        windowFeatures.height = parent->innerHeight();
        windowFeatures.heightSet = true;
    }

    FloatRect windowRect(windowFeatures.x, windowFeatures.y, windowFeatures.width, windowFeatures.height);

    // The new window's location is relative to its current screen, so shift
    // it in case it's on a secondary monitor. See http://b/viewIssue?id=967905.
    windowRect.move(screenRect.x(), screenRect.y());
    WebCore::DOMWindow::adjustWindowRect(screenRect, windowRect, windowRect);

    windowFeatures.x = windowRect.x();
    windowFeatures.y = windowRect.y();
    windowFeatures.height = windowRect.height();
    windowFeatures.width = windowRect.width();

    // If either of the origin coordinates weren't set in the original
    // string, make sure they aren't set now.
    if (!rawFeatures.xSet) {
        windowFeatures.x = 0;
        windowFeatures.xSet = false;
    }
    if (!rawFeatures.ySet) {
        windowFeatures.y = 0;
        windowFeatures.ySet = false;
    }

    frame = V8BindingDOMWindow::createWindow(V8BindingState::Only(), callingFrame, enteredFrame, frame, urlString, frameName, windowFeatures, v8::Local<v8::Value>());

    if (!frame)
        return v8::Undefined();

    return toV8(frame->domWindow());
}


v8::Handle<v8::Value> V8DOMWindow::indexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DOMWindow.IndexedPropertyGetter");

    DOMWindow* window = V8DOMWindow::toNative(info.Holder());
    if (!window)
        return notHandledByInterceptor();

    Frame* frame = window->frame();
    if (!frame)
        return notHandledByInterceptor();

    Frame* child = frame->tree()->child(index);
    if (child)
        return toV8(child->domWindow());

    return notHandledByInterceptor();
}


v8::Handle<v8::Value> V8DOMWindow::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DOMWindow.NamedPropertyGetter");

    DOMWindow* window = V8DOMWindow::toNative(info.Holder());
    if (!window)
        return notHandledByInterceptor();

    Frame* frame = window->frame();
    // window is detached from a frame.
    if (!frame)
        return notHandledByInterceptor();

    // Search sub-frames.
    AtomicString propName = v8StringToAtomicWebCoreString(name);
    Frame* child = frame->tree()->child(propName);
    if (child)
        return toV8(child->domWindow());

    // Search IDL functions defined in the prototype
    v8::Handle<v8::Value> result = info.Holder()->GetRealNamedProperty(name);
    if (!result.IsEmpty())
        return result;

    // Search named items in the document.
    Document* doc = frame->document();

    if (doc && doc->isHTMLDocument()) {
        if (static_cast<HTMLDocument*>(doc)->hasNamedItem(propName.impl()) || doc->hasElementWithId(propName.impl())) {
            RefPtr<HTMLCollection> items = doc->windowNamedItems(propName);
            if (items->length() >= 1) {
                if (items->length() == 1)
                    return toV8(items->firstItem());
                return toV8(items.release());
            }
        }
    }

    return notHandledByInterceptor();
}


v8::Handle<v8::Value> V8DOMWindow::setTimeoutCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.setTimeout()");
    return WindowSetTimeoutImpl(args, true);
}


v8::Handle<v8::Value> V8DOMWindow::setIntervalCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.setInterval()");
    return WindowSetTimeoutImpl(args, false);
}

v8::Handle<v8::Value> V8DOMWindow::openDatabaseCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DOMWindow.openDatabase");
    if (args.Length() < 4)
        return v8::Undefined();

    DOMWindow* imp = V8DOMWindow::toNative(args.Holder());
    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), true))
        return v8::Undefined();

    ExceptionCode ec = 0;
    String name = toWebCoreString(args[0]);
    String version = toWebCoreString(args[1]);
    String displayName = toWebCoreString(args[2]);
    unsigned long estimatedSize = args[3]->IntegerValue();
    RefPtr<DatabaseCallback> creationCallback;
    if ((args.Length() >= 5) && args[4]->IsObject())
        creationCallback = V8DatabaseCallback::create(args[4], imp->frame());

    v8::Handle<v8::Value> result = toV8(imp->openDatabase(name, version, displayName, estimatedSize, creationCallback.release(), ec));

    V8Proxy::setDOMException(ec);
    return result;
}

bool V8DOMWindow::namedSecurityCheck(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    v8::Handle<v8::Object> window = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), host);
    if (window.IsEmpty())
        return false;  // the frame is gone.

    DOMWindow* targetWindow = V8DOMWindow::toNative(window);

    ASSERT(targetWindow);

    Frame* target = targetWindow->frame();
    if (!target)
        return false;

    if (key->IsString()) {
        String name = toWebCoreString(key);

        // Allow access of GET and HAS if index is a subframe.
        if ((type == v8::ACCESS_GET || type == v8::ACCESS_HAS) && target->tree()->child(name))
            return true;
    }

    return V8BindingSecurity::canAccessFrame(V8BindingState::Only(), target, false);
}

bool V8DOMWindow::indexedSecurityCheck(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    v8::Handle<v8::Object> window = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), host);
    if (window.IsEmpty())
        return false;

    DOMWindow* targetWindow = V8DOMWindow::toNative(window);

    ASSERT(targetWindow);

    Frame* target = targetWindow->frame();
    if (!target)
        return false;

    // Allow access of GET and HAS if index is a subframe.
    if ((type == v8::ACCESS_GET || type == v8::ACCESS_HAS) && target->tree()->child(index))
        return true;

    return V8BindingSecurity::canAccessFrame(V8BindingState::Only(), target, false);
}

v8::Handle<v8::Value> toV8(DOMWindow* window)
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
    v8::Handle<v8::Object> windowWrapper = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), currentGlobal);
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

} // namespace WebCore
