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
#include "DOMWindow.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8CustomEventListener.h"
#include "V8MessagePortCustom.h"
#include "V8Proxy.h"
#include "V8Utilities.h"

#include "Base64.h"
#include "ExceptionCode.h"
#include "DOMTimer.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "WindowFeatures.h"

// Horizontal and vertical offset, from the parent content area, around newly
// opened popups that don't specify a location.
static const int popupTilePixels = 10;

namespace WebCore {

v8::Handle<v8::Value> V8Custom::WindowSetTimeoutImpl(const v8::Arguments& args, bool singleShot)
{
    int argumentCount = args.Length();

    if (argumentCount < 1)
        return v8::Undefined();

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

    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, args.Holder());

    if (!V8Proxy::canAccessFrame(imp->frame(), true))
        return v8::Undefined();

    ScriptExecutionContext* scriptContext = static_cast<ScriptExecutionContext*>(imp->document());

    if (!scriptContext)
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

    return v8::Integer::New(id);
}

static bool isAscii(const String& str)
{
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] > 0xFF)
            return false;
    }
    return true;
}

static v8::Handle<v8::Value> convertBase64(const String& str, bool encode)
{
    if (!isAscii(str)) {
        V8Proxy::setDOMException(INVALID_CHARACTER_ERR);
        return notHandledByInterceptor();
    }

    Vector<char> inputCharacters(str.length());
    for (size_t i = 0; i < str.length(); i++)
        inputCharacters[i] = static_cast<char>(str[i]);
    Vector<char> outputCharacters;

    if (encode)
        base64Encode(inputCharacters, outputCharacters);
    else {
        if (!base64Decode(inputCharacters, outputCharacters))
            return throwError("Cannot decode base64", V8Proxy::GeneralError);
    }

    return v8String(String(outputCharacters.data(), outputCharacters.size()));
}

ACCESSOR_GETTER(DOMWindowEvent)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, info.This());
    if (holder.IsEmpty())
        return v8::Undefined();

    Frame* frame = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, holder)->frame();
    if (!V8Proxy::canAccessFrame(frame, true))
        return v8::Undefined();

    v8::Local<v8::Context> context = V8Proxy::context(frame);
    v8::Local<v8::String> eventSymbol = v8::String::NewSymbol("event");
    v8::Handle<v8::Value> jsEvent = context->Global()->GetHiddenValue(eventSymbol);
    if (jsEvent.IsEmpty())
        return v8::Undefined();
    return jsEvent;
}

ACCESSOR_SETTER(DOMWindowEvent)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, info.This());
    if (holder.IsEmpty())
        return;

    Frame* frame = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, holder)->frame();
    if (!V8Proxy::canAccessFrame(frame, true))
        return;

    v8::Local<v8::Context> context = V8Proxy::context(frame);
    v8::Local<v8::String> eventSymbol = v8::String::NewSymbol("event");
    context->Global()->SetHiddenValue(eventSymbol, value);
}

ACCESSOR_GETTER(DOMWindowCrypto)
{
    // FIXME: Implement me.
    return v8::Undefined();
}

ACCESSOR_SETTER(DOMWindowLocation)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, info.This());
    if (holder.IsEmpty())
        return;

    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, holder);
    WindowSetLocation(imp, toWebCoreString(value));
}


ACCESSOR_SETTER(DOMWindowOpener)
{
    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, info.Holder());

    if (!V8Proxy::canAccessFrame(imp->frame(), true))
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

CALLBACK_FUNC_DECL(DOMWindowAddEventListener)
{
    INC_STATS("DOM.DOMWindow.addEventListener()");

    String eventType = toWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();

    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, args.Holder());

    if (!V8Proxy::canAccessFrame(imp->frame(), true))
        return v8::Undefined();

    Document* doc = imp->document();

    if (!doc)
        return v8::Undefined();

    // FIXME: Check if there is not enough arguments
    V8Proxy* proxy = V8Proxy::retrieve(imp->frame());
    if (!proxy)
        return v8::Undefined();

    RefPtr<EventListener> listener = proxy->eventListeners()->findOrCreateWrapper<V8EventListener>(proxy->frame(), args[1], false);

    if (listener)
        imp->addEventListener(eventType, listener, useCapture);

    return v8::Undefined();
}


CALLBACK_FUNC_DECL(DOMWindowRemoveEventListener)
{
    INC_STATS("DOM.DOMWindow.removeEventListener()");

    String eventType = toWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();

    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, args.Holder());

    if (!V8Proxy::canAccessFrame(imp->frame(), true))
        return v8::Undefined();

    Document* doc = imp->document();

    if (!doc)
        return v8::Undefined();

    V8Proxy* proxy = V8Proxy::retrieve(imp->frame());
    if (!proxy)
        return v8::Undefined();

    RefPtr<EventListener> listener = proxy->eventListeners()->findWrapper(args[1], false);

    if (listener)
        imp->removeEventListener(eventType, listener.get(), useCapture);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(DOMWindowPostMessage)
{
    INC_STATS("DOM.DOMWindow.postMessage()");
    DOMWindow* window = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, args.Holder());

    DOMWindow* source = V8Proxy::retrieveFrameForCallingContext()->domWindow();
    ASSERT(source->frame());

    v8::TryCatch tryCatch;
    String message = toWebCoreString(args[0]);
    MessagePortArray portArray;
    String targetOrigin;

    // This function has variable arguments and can either be:
    //   postMessage(message, port, targetOrigin);
    // or
    //   postMessage(message, targetOrigin);
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
    window->postMessage(message, &portArray, targetOrigin, source, ec);
    return throwError(ec);
}

CALLBACK_FUNC_DECL(DOMWindowAtob)
{
    INC_STATS("DOM.DOMWindow.atob()");

    if (args[0]->IsNull())
        return v8String("");
    String str = toWebCoreString(args[0]);

    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, args.Holder());

    if (!V8Proxy::canAccessFrame(imp->frame(), true))
        return v8::Undefined();

    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    return convertBase64(str, false);
}

CALLBACK_FUNC_DECL(DOMWindowBtoa)
{
    INC_STATS("DOM.DOMWindow.btoa()");

    if (args[0]->IsNull())
        return v8String("");
    String str = toWebCoreString(args[0]);

    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, args.Holder());

    if (!V8Proxy::canAccessFrame(imp->frame(), true))
        return v8::Undefined();

    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    return convertBase64(str, true);
}

// FIXME(fqian): returning string is cheating, and we should
// fix this by calling toString function on the receiver.
// However, V8 implements toString in JavaScript, which requires
// switching context of receiver. I consider it is dangerous.
CALLBACK_FUNC_DECL(DOMWindowToString)
{
    INC_STATS("DOM.DOMWindow.toString()");
    return args.This()->ObjectProtoToString();
}

CALLBACK_FUNC_DECL(DOMWindowNOP)
{
    INC_STATS("DOM.DOMWindow.nop()");
    return v8::Undefined();
}

static String eventNameFromAttributeName(const String& name)
{
    ASSERT(name.startsWith("on"));
    String eventType = name.substring(2);

    if (eventType.startsWith("w")) {
        switch(eventType[eventType.length() - 1]) {
        case 't':
            eventType = "webkitAnimationStart";
            break;
        case 'n':
            eventType = "webkitAnimationIteration";
            break;
        case 'd':
            ASSERT(eventType.length() > 7);
            if (eventType[7] == 'a')
                eventType = "webkitAnimationEnd";
            else
                eventType = "webkitTransitionEnd";
            break;
        }
    }

    return eventType;
}

ACCESSOR_SETTER(DOMWindowEventHandler)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, info.This());
    if (holder.IsEmpty())
        return;

    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, holder);

    Document* doc = imp->document();

    if (!doc)
        return;

    String key = toWebCoreString(name);
    String eventType = eventNameFromAttributeName(key);

    if (value->IsNull()) {
        // Clear the event listener
        imp->clearAttributeEventListener(eventType);
    } else {
        V8Proxy* proxy = V8Proxy::retrieve(imp->frame());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->eventListeners()->findOrCreateWrapper<V8EventListener>(proxy->frame(), value, true);
        if (listener)
            imp->setAttributeEventListener(eventType, listener);
    }
}

ACCESSOR_GETTER(DOMWindowEventHandler)
{
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, info.This());
    if (holder.IsEmpty())
        return v8::Undefined();

    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, holder);

    Document* doc = imp->document();

    if (!doc)
        return v8::Undefined();

    String key = toWebCoreString(name);
    String eventType = eventNameFromAttributeName(key);

    EventListener* listener = imp->getAttributeEventListener(eventType);
    return V8DOMWrapper::convertEventListenerToV8Object(listener);
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


static Frame* createWindow(Frame* callingFrame,
                           Frame* enteredFrame,
                           Frame* openerFrame,
                           const String& url,
                           const String& frameName,
                           const WindowFeatures& windowFeatures,
                           v8::Local<v8::Value> dialogArgs)
{
    ASSERT(callingFrame);
    ASSERT(enteredFrame);

    ResourceRequest request;

    // For whatever reason, Firefox uses the entered frame to determine
    // the outgoingReferrer.  We replicate that behavior here.
    String referrer = enteredFrame->loader()->outgoingReferrer();
    request.setHTTPReferrer(referrer);
    FrameLoader::addHTTPOriginIfNeeded(request, enteredFrame->loader()->outgoingOrigin());
    FrameLoadRequest frameRequest(request, frameName);

    // FIXME: It's much better for client API if a new window starts with a URL,
    // here where we know what URL we are going to open. Unfortunately, this
    // code passes the empty string for the URL, but there's a reason for that.
    // Before loading we have to set up the opener, openedByDOM,
    // and dialogArguments values. Also, to decide whether to use the URL
    // we currently do an allowsAccessFrom call using the window we create,
    // which can't be done before creating it. We'd have to resolve all those
    // issues to pass the URL instead of "".

    bool created;
    // We pass in the opener frame here so it can be used for looking up the
    // frame name, in case the active frame is different from the opener frame,
    // and the name references a frame relative to the opener frame, for example
    // "_self" or "_parent".
    Frame* newFrame = callingFrame->loader()->createWindow(openerFrame->loader(), frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->loader()->setOpenedByDOM();

    // Set dialog arguments on the global object of the new frame.
    if (!dialogArgs.IsEmpty()) {
        v8::Local<v8::Context> context = V8Proxy::context(newFrame);
        if (!context.IsEmpty()) {
            v8::Context::Scope scope(context);
            context->Global()->Set(v8::String::New("dialogArguments"), dialogArgs);
        }
    }

    if (protocolIsJavaScript(url) || ScriptController::isSafeScript(newFrame)) {
        KURL completedUrl =
            url.isEmpty() ? KURL(ParsedURLString, "") : completeURL(url);
        bool userGesture = processingUserGesture();

        if (created)
            newFrame->loader()->changeLocation(completedUrl, referrer, false, false, userGesture);
        else if (!url.isEmpty())
            newFrame->loader()->scheduleLocationChange(completedUrl.string(), referrer, false, userGesture);
    }

    return newFrame;
}



CALLBACK_FUNC_DECL(DOMWindowShowModalDialog)
{
    INC_STATS("DOM.DOMWindow.showModalDialog()");

    String url = toWebCoreStringWithNullOrUndefinedCheck(args[0]);
    v8::Local<v8::Value> dialogArgs = args[1];
    String featureArgs = toWebCoreStringWithNullOrUndefinedCheck(args[2]);

    DOMWindow* window = V8DOMWrapper::convertToNativeObject<DOMWindow>(
        V8ClassIndex::DOMWINDOW, args.Holder());
    Frame* frame = window->frame();

    if (!V8Proxy::canAccessFrame(frame, true))
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

    Frame* dialogFrame = createWindow(callingFrame, enteredFrame, frame, url, "", windowFeatures, dialogArgs);
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


CALLBACK_FUNC_DECL(DOMWindowOpen)
{
    INC_STATS("DOM.DOMWindow.open()");

    String urlString = toWebCoreStringWithNullOrUndefinedCheck(args[0]);
    AtomicString frameName = (args[1]->IsUndefined() || args[1]->IsNull()) ? "_blank" : AtomicString(toWebCoreString(args[1]));

    DOMWindow* parent = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, args.Holder());
    Frame* frame = parent->frame();

    if (!V8Proxy::canAccessFrame(frame, true))
        return v8::Undefined();

    Frame* callingFrame = V8Proxy::retrieveFrameForCallingContext();
    if (!callingFrame)
        return v8::Undefined();

    Frame* enteredFrame = V8Proxy::retrieveFrameForEnteredContext();
    if (!enteredFrame)
        return v8::Undefined();

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

            frame->loader()->scheduleLocationChange(completedUrl, referrer, false, userGesture);
        }
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::DOMWINDOW, frame->domWindow());
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

    frame = createWindow(callingFrame, enteredFrame, frame, urlString, frameName, windowFeatures, v8::Local<v8::Value>());

    if (!frame)
        return v8::Undefined();

    return V8DOMWrapper::convertToV8Object(V8ClassIndex::DOMWINDOW, frame->domWindow());
}


INDEXED_PROPERTY_GETTER(DOMWindow)
{
    INC_STATS("DOM.DOMWindow.IndexedPropertyGetter");
    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, info.This());
    if (holder.IsEmpty())
        return notHandledByInterceptor();

    DOMWindow* window = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, holder);
    if (!window)
        return notHandledByInterceptor();

    Frame* frame = window->frame();
    if (!frame)
        return notHandledByInterceptor();

    Frame* child = frame->tree()->child(index);
    if (child)
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::DOMWINDOW, child->domWindow());

    return notHandledByInterceptor();
}


NAMED_PROPERTY_GETTER(DOMWindow)
{
    INC_STATS("DOM.DOMWindow.NamedPropertyGetter");

    v8::Handle<v8::Object> holder = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, info.This());
    if (holder.IsEmpty())
        return notHandledByInterceptor();

    DOMWindow* window = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, holder);
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
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::DOMWINDOW, child->domWindow());

    // Search IDL functions defined in the prototype
    v8::Handle<v8::Value> result = holder->GetRealNamedPropertyInPrototypeChain(name);
    if (!result.IsEmpty())
        return result;

    // Search named items in the document.
    Document* doc = frame->document();

    if (doc) {
        RefPtr<HTMLCollection> items = doc->windowNamedItems(propName);
        if (items->length() >= 1) {
            if (items->length() == 1)
                return V8DOMWrapper::convertNodeToV8Object(items->firstItem());
            else
                return V8DOMWrapper::convertToV8Object(V8ClassIndex::HTMLCOLLECTION, items.release());
        }
    }

    return notHandledByInterceptor();
}


void V8Custom::WindowSetLocation(DOMWindow* window, const String& relativeURL)
{
    Frame* frame = window->frame();
    if (!frame)
        return;

    KURL url = completeURL(relativeURL);
    if (url.isNull())
        return;

    if (!shouldAllowNavigation(frame))
        return;

    navigateIfAllowed(frame, url, false, false);
}


CALLBACK_FUNC_DECL(DOMWindowSetTimeout)
{
    INC_STATS("DOM.DOMWindow.setTimeout()");
    return WindowSetTimeoutImpl(args, true);
}


CALLBACK_FUNC_DECL(DOMWindowSetInterval)
{
    INC_STATS("DOM.DOMWindow.setInterval()");
    return WindowSetTimeoutImpl(args, false);
}


void V8Custom::ClearTimeoutImpl(const v8::Arguments& args)
{
    int handle = toInt32(args[0]);

    v8::Handle<v8::Object> holder = args.Holder();
    DOMWindow* imp = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, holder);
    if (!V8Proxy::canAccessFrame(imp->frame(), true))
        return;
    ScriptExecutionContext* context = static_cast<ScriptExecutionContext*>(imp->document());
    if (!context)
        return;
    DOMTimer::removeById(context, handle);
}


CALLBACK_FUNC_DECL(DOMWindowClearTimeout)
{
    INC_STATS("DOM.DOMWindow.clearTimeout");
    ClearTimeoutImpl(args);
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(DOMWindowClearInterval)
{
    INC_STATS("DOM.DOMWindow.clearInterval");
    ClearTimeoutImpl(args);
    return v8::Undefined();
}

NAMED_ACCESS_CHECK(DOMWindow)
{
    ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::DOMWINDOW);
    v8::Handle<v8::Object> window = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, host);
    if (window.IsEmpty())
        return false;  // the frame is gone.

    DOMWindow* targetWindow = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, window);

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

    return V8Proxy::canAccessFrame(target, false);
}

INDEXED_ACCESS_CHECK(DOMWindow)
{
    ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::DOMWINDOW);
    v8::Handle<v8::Object> window = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, host);
    if (window.IsEmpty())
        return false;

    DOMWindow* targetWindow = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, window);

    ASSERT(targetWindow);

    Frame* target = targetWindow->frame();
    if (!target)
        return false;

    // Allow access of GET and HAS if index is a subframe.
    if ((type == v8::ACCESS_GET || type == v8::ACCESS_HAS) && target->tree()->child(index))
        return true;

    return V8Proxy::canAccessFrame(target, false);
}

} // namespace WebCore
