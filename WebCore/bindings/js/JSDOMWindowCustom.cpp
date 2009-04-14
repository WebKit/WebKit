/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSDOMWindowCustom.h"

#include "AtomicString.h"
#include "Base64.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "History.h"
#include "JSAudioConstructor.h"
#include "JSDOMWindowShell.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "JSHistory.h"
#include "JSImageConstructor.h"
#include "JSLocation.h"
#include "JSMessageChannelConstructor.h"
#include "JSMessagePort.h"
#include "JSOptionConstructor.h"
#include "JSWebKitCSSMatrixConstructor.h"
#include "JSWebKitPointConstructor.h"
#include "JSWorkerConstructor.h"
#include "JSXMLHttpRequestConstructor.h"
#include "JSXSLTProcessorConstructor.h"
#include "Location.h"
#include "MediaPlayer.h"
#include "MessagePort.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "RegisteredEventListener.h"
#include "ScriptController.h"
#include "Settings.h"
#include "WindowFeatures.h"
#include <runtime/JSObject.h>
#include <runtime/PrototypeFunction.h>

using namespace JSC;

namespace WebCore {

void JSDOMWindow::mark()
{
    Base::mark();

    markEventListeners(impl()->eventListeners());

    JSGlobalData& globalData = *Heap::heap(this)->globalData();

    markDOMObjectWrapper(globalData, impl()->optionalConsole());
    markDOMObjectWrapper(globalData, impl()->optionalHistory());
    markDOMObjectWrapper(globalData, impl()->optionalLocationbar());
    markDOMObjectWrapper(globalData, impl()->optionalMenubar());
    markDOMObjectWrapper(globalData, impl()->optionalNavigator());
    markDOMObjectWrapper(globalData, impl()->optionalPersonalbar());
    markDOMObjectWrapper(globalData, impl()->optionalScreen());
    markDOMObjectWrapper(globalData, impl()->optionalScrollbars());
    markDOMObjectWrapper(globalData, impl()->optionalSelection());
    markDOMObjectWrapper(globalData, impl()->optionalStatusbar());
    markDOMObjectWrapper(globalData, impl()->optionalToolbar());
    markDOMObjectWrapper(globalData, impl()->optionalLocation());
#if ENABLE(DOM_STORAGE)
    markDOMObjectWrapper(globalData, impl()->optionalSessionStorage());
    markDOMObjectWrapper(globalData, impl()->optionalLocalStorage());
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    markDOMObjectWrapper(globalData, impl()->optionalApplicationCache());
#endif
}

bool JSDOMWindow::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    // Only allow deleting properties by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return false;
    return Base::deleteProperty(exec, propertyName);
}

bool JSDOMWindow::customGetPropertyNames(ExecState* exec, PropertyNameArray&)
{
    // Only allow the window to enumerated by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return true;
    return false;
}

bool JSDOMWindow::getPropertyAttributes(JSC::ExecState* exec, const Identifier& propertyName, unsigned& attributes) const
{
    // Only allow getting property attributes properties by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return false;
    return Base::getPropertyAttributes(exec, propertyName, attributes);
}

void JSDOMWindow::defineGetter(ExecState* exec, const Identifier& propertyName, JSObject* getterFunction)
{
    // Only allow defining getters by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return;

    // Don't allow shadowing location using defineGetter.
    if (propertyName == "location")
        return;

    Base::defineGetter(exec, propertyName, getterFunction);
}

void JSDOMWindow::defineSetter(ExecState* exec, const Identifier& propertyName, JSObject* setterFunction)
{
    // Only allow defining setters by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return;
    Base::defineSetter(exec, propertyName, setterFunction);
}

JSValuePtr JSDOMWindow::lookupGetter(ExecState* exec, const Identifier& propertyName)
{
    // Only allow looking-up getters by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return jsUndefined();
    return Base::lookupGetter(exec, propertyName);
}

JSValuePtr JSDOMWindow::lookupSetter(ExecState* exec, const Identifier& propertyName)
{
    // Only allow looking-up setters by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return jsUndefined();
    return Base::lookupSetter(exec, propertyName);
}

// Custom Attributes

JSValuePtr JSDOMWindow::history(ExecState* exec) const
{
    History* history = impl()->history();
    if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), history))
        return wrapper;

    JSHistory* jsHistory = new (exec) JSHistory(getDOMStructure<JSHistory>(exec, const_cast<JSDOMWindow*>(this)), history);
    cacheDOMObjectWrapper(exec->globalData(), history, jsHistory);
    return jsHistory;
}

JSValuePtr JSDOMWindow::location(ExecState* exec) const
{
    Location* location = impl()->location();
    if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), location))
        return wrapper;

    JSLocation* jsLocation = new (exec) JSLocation(getDOMStructure<JSLocation>(exec, const_cast<JSDOMWindow*>(this)), location);
    cacheDOMObjectWrapper(exec->globalData(), location, jsLocation);
    return jsLocation;
}

void JSDOMWindow::setLocation(ExecState* exec, JSValuePtr value)
{
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame)
        return;

#if ENABLE(DASHBOARD_SUPPORT)
    // To avoid breaking old widgets, make "var location =" in a top-level frame create
    // a property named "location" instead of performing a navigation (<rdar://problem/5688039>).
    if (Settings* settings = activeFrame->settings()) {
        if (settings->usesDashboardBackwardCompatibilityMode() && !activeFrame->tree()->parent()) {
            if (allowsAccessFrom(exec))
                putDirect(Identifier(exec, "location"), value);
            return;
        }
    }
#endif

    if (!activeFrame->loader()->shouldAllowNavigation(impl()->frame()))
        return;
    String dstUrl = activeFrame->loader()->completeURL(value.toString(exec)).string();
    if (!protocolIs(dstUrl, "javascript") || allowsAccessFrom(exec)) {
        bool userGesture = activeFrame->script()->processingUserGesture();
        // We want a new history item if this JS was called via a user gesture
        impl()->frame()->loader()->scheduleLocationChange(dstUrl, activeFrame->loader()->outgoingReferrer(), !activeFrame->script()->anyPageIsProcessingUserGesture(), false, userGesture);
    }
}

JSValuePtr JSDOMWindow::crypto(ExecState*) const
{
    return jsUndefined();
}

JSValuePtr JSDOMWindow::event(ExecState* exec) const
{
    Event* event = currentEvent();
    if (!event)
        return jsUndefined();
    return toJS(exec, event);
}

JSValuePtr JSDOMWindow::image(ExecState* exec) const
{
    return getDOMConstructor<JSImageConstructor>(exec, this);
}

JSValuePtr JSDOMWindow::option(ExecState* exec) const
{
    return getDOMConstructor<JSOptionConstructor>(exec, this);
}

#if ENABLE(VIDEO)
JSValuePtr JSDOMWindow::audio(ExecState* exec) const
{
    if (!MediaPlayer::isAvailable())
        return jsUndefined();
    return getDOMConstructor<JSAudioConstructor>(exec, this);
}
#endif

JSValuePtr JSDOMWindow::webKitPoint(ExecState* exec) const
{
    return getDOMConstructor<JSWebKitPointConstructor>(exec);
}

JSValuePtr JSDOMWindow::webKitCSSMatrix(ExecState* exec) const
{
    return getDOMConstructor<JSWebKitCSSMatrixConstructor>(exec);
}
 
JSValuePtr JSDOMWindow::xMLHttpRequest(ExecState* exec) const
{
    return getDOMConstructor<JSXMLHttpRequestConstructor>(exec, this);
}

#if ENABLE(XSLT)
JSValuePtr JSDOMWindow::xSLTProcessor(ExecState* exec) const
{
    return getDOMConstructor<JSXSLTProcessorConstructor>(exec);
}
#endif

#if ENABLE(CHANNEL_MESSAGING)
JSValuePtr JSDOMWindow::messageChannel(ExecState* exec) const
{
    return getDOMConstructor<JSMessageChannelConstructor>(exec, this);
}
#endif

#if ENABLE(WORKERS)
JSValuePtr JSDOMWindow::worker(ExecState* exec) const
{
    return getDOMConstructor<JSWorkerConstructor>(exec);
}
#endif

// Custom functions

// Helper for window.open() and window.showModalDialog()
static Frame* createWindow(ExecState* exec, Frame* activeFrame, Frame* openerFrame, 
                           const String& url, const String& frameName, 
                           const WindowFeatures& windowFeatures, JSValuePtr dialogArgs)
{
    ASSERT(activeFrame);

    ResourceRequest request;

    request.setHTTPReferrer(activeFrame->loader()->outgoingReferrer());
    FrameLoader::addHTTPOriginIfNeeded(request, activeFrame->loader()->outgoingOrigin());
    FrameLoadRequest frameRequest(request, frameName);

    // FIXME: It's much better for client API if a new window starts with a URL, here where we
    // know what URL we are going to open. Unfortunately, this code passes the empty string
    // for the URL, but there's a reason for that. Before loading we have to set up the opener,
    // openedByDOM, and dialogArguments values. Also, to decide whether to use the URL we currently
    // do an allowsAccessFrom call using the window we create, which can't be done before creating it.
    // We'd have to resolve all those issues to pass the URL instead of "".

    bool created;
    // We pass in the opener frame here so it can be used for looking up the frame name, in case the active frame
    // is different from the opener frame, and the name references a frame relative to the opener frame, for example
    // "_self" or "_parent".
    Frame* newFrame = activeFrame->loader()->createWindow(openerFrame->loader(), frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->loader()->setOpenedByDOM();

    JSDOMWindow* newWindow = toJSDOMWindow(newFrame);

    if (dialogArgs)
        newWindow->putDirect(Identifier(exec, "dialogArguments"), dialogArgs);

    if (!protocolIs(url, "javascript") || newWindow->allowsAccessFrom(exec)) {
        KURL completedURL = url.isEmpty() ? KURL("") : activeFrame->document()->completeURL(url);
        bool userGesture = activeFrame->script()->processingUserGesture();

        if (created)
            newFrame->loader()->changeLocation(completedURL, activeFrame->loader()->outgoingReferrer(), false, false, userGesture);
        else if (!url.isEmpty())
            newFrame->loader()->scheduleLocationChange(completedURL.string(), activeFrame->loader()->outgoingReferrer(), !activeFrame->script()->anyPageIsProcessingUserGesture(), false, userGesture);
    }

    return newFrame;
}

JSValuePtr JSDOMWindow::open(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame)
        return  jsUndefined();

    Page* page = frame->page();

    String urlString = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0));
    AtomicString frameName = args.at(exec, 1).isUndefinedOrNull() ? "_blank" : AtomicString(args.at(exec, 1).toString(exec));

    // Because FrameTree::find() returns true for empty strings, we must check for empty framenames.
    // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
    if (!DOMWindow::allowPopUp(activeFrame) && (frameName.isEmpty() || !frame->tree()->find(frameName)))
        return jsUndefined();

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
        if (!activeFrame->loader()->shouldAllowNavigation(frame))
            return jsUndefined();

        String completedURL;
        if (!urlString.isEmpty())
            completedURL = activeFrame->document()->completeURL(urlString).string();

        const JSDOMWindow* targetedWindow = toJSDOMWindow(frame);
        if (!completedURL.isEmpty() && (!protocolIs(completedURL, "javascript") || (targetedWindow && targetedWindow->allowsAccessFrom(exec)))) {
            bool userGesture = activeFrame->script()->processingUserGesture();
            frame->loader()->scheduleLocationChange(completedURL, activeFrame->loader()->outgoingReferrer(), !activeFrame->script()->anyPageIsProcessingUserGesture(), false, userGesture);
        }
        return toJS(exec, frame->domWindow());
    }

    // In the case of a named frame or a new window, we'll use the createWindow() helper
    WindowFeatures windowFeatures(valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 2)));
    FloatRect windowRect(windowFeatures.xSet ? windowFeatures.x : 0, windowFeatures.ySet ? windowFeatures.y : 0,
                         windowFeatures.widthSet ? windowFeatures.width : 0, windowFeatures.heightSet ? windowFeatures.height : 0);
    DOMWindow::adjustWindowRect(screenAvailableRect(page ? page->mainFrame()->view() : 0), windowRect, windowRect);

    windowFeatures.x = windowRect.x();
    windowFeatures.y = windowRect.y();
    windowFeatures.height = windowRect.height();
    windowFeatures.width = windowRect.width();

    frame = createWindow(exec, activeFrame, frame, urlString, frameName, windowFeatures, noValue());

    if (!frame)
        return jsUndefined();

    return toJS(exec, frame->domWindow());
}

JSValuePtr JSDOMWindow::showModalDialog(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();

    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();

    if (!DOMWindow::canShowModalDialogNow(frame) || !DOMWindow::allowPopUp(activeFrame))
        return jsUndefined();

    String url = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0));
    JSValuePtr dialogArgs = args.at(exec, 1);
    String featureArgs = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 2));

    HashMap<String, String> features;
    DOMWindow::parseModalDialogFeatures(featureArgs, features);

    const bool trusted = false;

    // The following features from Microsoft's documentation are not implemented:
    // - default font settings
    // - width, height, left, and top specified in units other than "px"
    // - edge (sunken or raised, default is raised)
    // - dialogHide: trusted && boolFeature(features, "dialoghide"), makes dialog hide when you print
    // - help: boolFeature(features, "help", true), makes help icon appear in dialog (what does it do on Windows?)
    // - unadorned: trusted && boolFeature(features, "unadorned");

    FloatRect screenRect = screenAvailableRect(frame->view());

    WindowFeatures wargs;
    wargs.width = WindowFeatures::floatFeature(features, "dialogwidth", 100, screenRect.width(), 620); // default here came from frame size of dialog in MacIE
    wargs.widthSet = true;
    wargs.height = WindowFeatures::floatFeature(features, "dialogheight", 100, screenRect.height(), 450); // default here came from frame size of dialog in MacIE
    wargs.heightSet = true;

    wargs.x = WindowFeatures::floatFeature(features, "dialogleft", screenRect.x(), screenRect.right() - wargs.width, -1);
    wargs.xSet = wargs.x > 0;
    wargs.y = WindowFeatures::floatFeature(features, "dialogtop", screenRect.y(), screenRect.bottom() - wargs.height, -1);
    wargs.ySet = wargs.y > 0;

    if (WindowFeatures::boolFeature(features, "center", true)) {
        if (!wargs.xSet) {
            wargs.x = screenRect.x() + (screenRect.width() - wargs.width) / 2;
            wargs.xSet = true;
        }
        if (!wargs.ySet) {
            wargs.y = screenRect.y() + (screenRect.height() - wargs.height) / 2;
            wargs.ySet = true;
        }
    }

    wargs.dialog = true;
    wargs.resizable = WindowFeatures::boolFeature(features, "resizable");
    wargs.scrollbarsVisible = WindowFeatures::boolFeature(features, "scroll", true);
    wargs.statusBarVisible = WindowFeatures::boolFeature(features, "status", !trusted);
    wargs.menuBarVisible = false;
    wargs.toolBarVisible = false;
    wargs.locationBarVisible = false;
    wargs.fullscreen = false;

    Frame* dialogFrame = createWindow(exec, activeFrame, frame, url, "", wargs, dialogArgs);
    if (!dialogFrame)
        return jsUndefined();

    JSDOMWindow* dialogWindow = toJSDOMWindow(dialogFrame);

    // Get the return value either just before clearing the dialog window's
    // properties (in JSDOMWindowBase::clear), or when on return from runModal.
    JSValuePtr returnValue = noValue();
    dialogWindow->setReturnValueSlot(&returnValue);
    dialogFrame->page()->chrome()->runModal();
    dialogWindow->setReturnValueSlot(0);

    // If we don't have a return value, get it now.
    // Either JSDOMWindowBase::clear was not called yet, or there was no return value,
    // and in that case, there's no harm in trying again (no benefit either).
    if (!returnValue)
        returnValue = dialogWindow->getDirect(Identifier(exec, "returnValue"));

    return returnValue ? returnValue : jsUndefined();
}

JSValuePtr JSDOMWindow::postMessage(ExecState* exec, const ArgList& args)
{
    DOMWindow* window = impl();

    DOMWindow* source = asJSDOMWindow(exec->dynamicGlobalObject())->impl();
    String message = args.at(exec, 0).toString(exec);

    if (exec->hadException())
        return jsUndefined();

    MessagePort* messagePort = (args.size() == 2) ? 0 : toMessagePort(args.at(exec, 1));

    String targetOrigin = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, (args.size() == 2) ? 1 : 2));
    if (exec->hadException())
        return jsUndefined();

    ExceptionCode ec = 0;
    window->postMessage(message, messagePort, targetOrigin, source, ec);
    setDOMException(exec, ec);

    return jsUndefined();
}

static JSValuePtr setTimeoutOrInterval(ExecState* exec, JSDOMWindow* window, const ArgList& args, bool timeout)
{
    JSValuePtr v = args.at(exec, 0);
    int delay = args.at(exec, 1).toInt32(exec);
    if (v.isString())
        return jsNumber(exec, window->installTimeout(asString(v)->value(), delay, timeout));
    CallData callData;
    if (v.getCallData(callData) == CallTypeNone)
        return jsUndefined();
    ArgList argsTail;
    args.getSlice(2, argsTail);
    return jsNumber(exec, window->installTimeout(exec, v, argsTail, delay, timeout));
}

JSValuePtr JSDOMWindow::setTimeout(ExecState* exec, const ArgList& args)
{
    return setTimeoutOrInterval(exec, this, args, true);
}

JSValuePtr JSDOMWindow::clearTimeout(ExecState* exec, const ArgList& args)
{
    removeTimeout(args.at(exec, 0).toInt32(exec));
    return jsUndefined();
}

JSValuePtr JSDOMWindow::setInterval(ExecState* exec, const ArgList& args)
{
    return setTimeoutOrInterval(exec, this, args, false);
}

JSValuePtr JSDOMWindow::clearInterval(ExecState* exec, const ArgList& args)
{
    removeTimeout(args.at(exec, 0).toInt32(exec));
    return jsUndefined();
}

JSValuePtr JSDOMWindow::atob(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValuePtr v = args.at(exec, 0);
    if (v.isNull())
        return jsEmptyString(exec);

    UString s = v.toString(exec);
    if (!s.is8Bit()) {
        setDOMException(exec, INVALID_CHARACTER_ERR);
        return jsUndefined();
    }

    Vector<char> in(s.size());
    for (int i = 0; i < s.size(); ++i)
        in[i] = static_cast<char>(s.data()[i]);
    Vector<char> out;

    if (!base64Decode(in, out))
        return throwError(exec, GeneralError, "Cannot decode base64");

    return jsString(exec, String(out.data(), out.size()));
}

JSValuePtr JSDOMWindow::btoa(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValuePtr v = args.at(exec, 0);
    if (v.isNull())
        return jsEmptyString(exec);

    UString s = v.toString(exec);
    if (!s.is8Bit()) {
        setDOMException(exec, INVALID_CHARACTER_ERR);
        return jsUndefined();
    }

    Vector<char> in(s.size());
    for (int i = 0; i < s.size(); ++i)
        in[i] = static_cast<char>(s.data()[i]);
    Vector<char> out;

    base64Encode(in, out);

    return jsString(exec, String(out.data(), out.size()));
}

JSValuePtr JSDOMWindow::addEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();

    if (RefPtr<JSEventListener> listener = findOrCreateJSEventListener(args.at(exec, 1)))
        impl()->addEventListener(AtomicString(args.at(exec, 0).toString(exec)), listener.release(), args.at(exec, 2).toBoolean(exec));

    return jsUndefined();
}

JSValuePtr JSDOMWindow::removeEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = findJSEventListener(args.at(exec, 1)))
        impl()->removeEventListener(AtomicString(args.at(exec, 0).toString(exec)), listener, args.at(exec, 2).toBoolean(exec));

    return jsUndefined();
}

JSValuePtr JSDOMWindow::captureEvents(ExecState*, const ArgList&)
{
    return jsUndefined();
}

JSValuePtr JSDOMWindow::releaseEvents(ExecState*, const ArgList&)
{
    return jsUndefined();
}

DOMWindow* toDOMWindow(JSValuePtr value)
{
    if (!value.isObject())
        return 0;
    JSObject* object = asObject(value);
    if (object->inherits(&JSDOMWindow::s_info))
        return static_cast<JSDOMWindow*>(object)->impl();
    if (object->inherits(&JSDOMWindowShell::s_info))
        return static_cast<JSDOMWindowShell*>(object)->impl();
    return 0;
}

JSValuePtr nonCachingStaticCloseFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) PrototypeFunction(exec, 0, propertyName, jsDOMWindowPrototypeFunctionClose);
}

JSValuePtr nonCachingStaticBlurFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) PrototypeFunction(exec, 0, propertyName, jsDOMWindowPrototypeFunctionBlur);
}

JSValuePtr nonCachingStaticFocusFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) PrototypeFunction(exec, 0, propertyName, jsDOMWindowPrototypeFunctionFocus);
}

JSValuePtr nonCachingStaticPostMessageFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) PrototypeFunction(exec, 2, propertyName, jsDOMWindowPrototypeFunctionPostMessage);
}

} // namespace WebCore
