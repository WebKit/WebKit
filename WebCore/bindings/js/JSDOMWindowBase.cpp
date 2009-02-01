/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "JSDOMWindowBase.h"

#include "CString.h"
#include "Console.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "Element.h"
#include "EventListener.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "GCController.h"
#include "HTMLDocument.h"
#include "InspectorController.h"
#include "JSAudioConstructor.h"
#include "JSDOMWindowCustom.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "JSHTMLCollection.h"
#include "JSImageConstructor.h"
#include "JSMessageChannelConstructor.h"
#include "JSNode.h"
#include "JSWebKitCSSMatrixConstructor.h"
#include "JSOptionConstructor.h"
#include "JSWorkerConstructor.h"
#include "JSXMLHttpRequestConstructor.h"
#include "JSXSLTProcessorConstructor.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "PluginInfoStore.h"
#include "RenderView.h"
#include "ScheduledAction.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WindowFeatures.h"
#include "htmlediting.h"
#include <runtime/Error.h>
#include <runtime/JSLock.h>
#include <wtf/AlwaysInline.h>
#include <wtf/MathExtras.h>

using namespace JSC;

static JSValuePtr windowProtoFuncOpen(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr windowProtoFuncShowModalDialog(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr windowProtoFuncNotImplemented(ExecState*, JSObject*, JSValuePtr, const ArgList&);

static JSValuePtr jsDOMWindowBaseCrypto(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr jsDOMWindowBaseEvent(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseEvent(ExecState*, JSObject*, JSValuePtr);

// Constructors
static JSValuePtr jsDOMWindowBaseAudio(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseAudio(ExecState*, JSObject*, JSValuePtr);
static JSValuePtr jsDOMWindowBaseImage(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseImage(ExecState*, JSObject*, JSValuePtr);
static JSValuePtr jsDOMWindowBaseMessageChannel(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseMessageChannel(ExecState*, JSObject*, JSValuePtr);
static JSValuePtr jsDOMWindowBaseWorker(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseWorker(ExecState*, JSObject*, JSValuePtr);
static JSValuePtr jsDOMWindowBaseOption(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOption(ExecState*, JSObject*, JSValuePtr);
static JSValuePtr jsDOMWindowBaseWebKitCSSMatrix(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseWebKitCSSMatrix(ExecState*, JSObject*, JSValuePtr);
static JSValuePtr jsDOMWindowBaseXMLHttpRequest(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseXMLHttpRequest(ExecState*, JSObject*, JSValuePtr);
static JSValuePtr jsDOMWindowBaseXSLTProcessor(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseXSLTProcessor(ExecState*, JSObject*, JSValuePtr);

#include "JSDOMWindowBase.lut.h"

namespace WebCore {

////////////////////// JSDOMWindowBase Object ////////////////////////

const ClassInfo JSDOMWindowBase::s_info = { "Window", 0, &JSDOMWindowBaseTable, 0 };

/*
@begin JSDOMWindowBaseTable
# -- Functions --
  open                          windowProtoFuncOpen                         DontDelete|Function 3
  showModalDialog               windowProtoFuncShowModalDialog              DontDelete|Function 1
# Not implemented
  captureEvents                 windowProtoFuncNotImplemented               DontDelete|Function 0
  releaseEvents                 windowProtoFuncNotImplemented               DontDelete|Function 0
# -- Attributes --
  crypto                        jsDOMWindowBaseCrypto                       DontDelete|ReadOnly
  event                         jsDOMWindowBaseEvent                        DontDelete
# -- Constructors --
  Audio                         jsDOMWindowBaseAudio                        DontDelete
  Image                         jsDOMWindowBaseImage                        DontDelete
  MessageChannel                jsDOMWindowBaseMessageChannel               DontDelete
  Option                        jsDOMWindowBaseOption                       DontDelete
  WebKitCSSMatrix               jsDOMWindowBaseWebKitCSSMatrix              DontDelete
  Worker                        jsDOMWindowBaseWorker                       DontDelete
  XMLHttpRequest                jsDOMWindowBaseXMLHttpRequest               DontDelete
  XSLTProcessor                 jsDOMWindowBaseXSLTProcessor                DontDelete
@end
*/

JSDOMWindowBase::JSDOMWindowBaseData::JSDOMWindowBaseData(PassRefPtr<DOMWindow> window, JSDOMWindowShell* shell)
    : impl(window)
    , returnValueSlot(0)
    , shell(shell)
{
}

JSDOMWindowBase::JSDOMWindowBase(PassRefPtr<Structure> structure, PassRefPtr<DOMWindow> window, JSDOMWindowShell* shell)
    : JSDOMGlobalObject(structure, new JSDOMWindowBaseData(window, shell), shell)
{
    // Time in milliseconds before the script timeout handler kicks in.
    setTimeoutTime(10000);

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(globalExec(), "document"), jsNull(), DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(globalExec(), "window"), d()->shell, DontDelete | ReadOnly)
    };
    
    addStaticGlobals(staticGlobals, sizeof(staticGlobals) / sizeof(GlobalPropertyInfo));
}

void JSDOMWindowBase::updateDocument()
{
    ASSERT(d()->impl->document());
    ExecState* exec = globalExec();
    symbolTablePutWithAttributes(Identifier(exec, "document"), toJS(exec, d()->impl->document()), DontDelete | ReadOnly);
}

JSDOMWindowBase::~JSDOMWindowBase()
{
    if (d()->impl->frame())
        d()->impl->frame()->script()->clearFormerWindow(asJSDOMWindow(this));
}

ScriptExecutionContext* JSDOMWindowBase::scriptExecutionContext() const
{
    return d()->impl->document();
}

static bool allowPopUp(ExecState* exec)
{
    Frame* frame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();

    ASSERT(frame);
    if (frame->script()->processingUserGesture())
        return true;
    Settings* settings = frame->settings();
    return settings && settings->JavaScriptCanOpenWindowsAutomatically();
}

static HashMap<String, String> parseModalDialogFeatures(const String& featuresArg)
{
    HashMap<String, String> map;

    Vector<String> features;
    featuresArg.split(';', features);
    Vector<String>::const_iterator end = features.end();
    for (Vector<String>::const_iterator it = features.begin(); it != end; ++it) {
        String s = *it;
        int pos = s.find('=');
        int colonPos = s.find(':');
        if (pos >= 0 && colonPos >= 0)
            continue; // ignore any strings that have both = and :
        if (pos < 0)
            pos = colonPos;
        if (pos < 0) {
            // null string for value means key without value
            map.set(s.stripWhiteSpace().lower(), String());
        } else {
            String key = s.left(pos).stripWhiteSpace().lower();
            String val = s.substring(pos + 1).stripWhiteSpace().lower();
            int spacePos = val.find(' ');
            if (spacePos != -1)
                val = val.left(spacePos);
            map.set(key, val);
        }
    }

    return map;
}

static Frame* createWindow(ExecState* exec, Frame* openerFrame, const String& url,
    const String& frameName, const WindowFeatures& windowFeatures, JSValuePtr dialogArgs)
{
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
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
            newFrame->loader()->changeLocation(completedURL, activeFrame->loader()->outgoingReferrer(), false, userGesture);
        else if (!url.isEmpty())
            newFrame->loader()->scheduleLocationChange(completedURL.string(), activeFrame->loader()->outgoingReferrer(), false, false, userGesture);
    }

    return newFrame;
}

static bool canShowModalDialog(const Frame* frame)
{
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    return page->chrome()->canRunModal();
}

static bool canShowModalDialogNow(const Frame* frame)
{
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    return page->chrome()->canRunModalNow();
}

static JSValuePtr showModalDialog(ExecState* exec, Frame* frame, const String& url, JSValuePtr dialogArgs, const String& featureArgs)
{
    if (!canShowModalDialogNow(frame) || !allowPopUp(exec))
        return jsUndefined();

    const HashMap<String, String> features = parseModalDialogFeatures(featureArgs);

    const bool trusted = false;

    // The following features from Microsoft's documentation are not implemented:
    // - default font settings
    // - width, height, left, and top specified in units other than "px"
    // - edge (sunken or raised, default is raised)
    // - dialogHide: trusted && boolFeature(features, "dialoghide"), makes dialog hide when you print
    // - help: boolFeature(features, "help", true), makes help icon appear in dialog (what does it do on Windows?)
    // - unadorned: trusted && boolFeature(features, "unadorned");

    if (!frame)
        return jsUndefined();

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

    Frame* dialogFrame = createWindow(exec, frame, url, "", wargs, dialogArgs);
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

} // namespace WebCore

using namespace WebCore;

JSValuePtr jsDOMWindowBaseCrypto(ExecState*, const Identifier&, const PropertySlot&)
{
    return jsUndefined(); // FIXME: implement this
}

JSValuePtr jsDOMWindowBaseEvent(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->currentEvent())
        return jsUndefined();
    return toJS(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->currentEvent());
}

JSValuePtr jsDOMWindowBaseImage(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSImageConstructor>(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase())));
}

#if !ENABLE(CHANNEL_MESSAGING)

JSValuePtr jsDOMWindowBaseMessageChannel(ExecState*, const Identifier&, const PropertySlot&)
{
    return jsUndefined();
}

#else

JSValuePtr jsDOMWindowBaseMessageChannel(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSMessageChannelConstructor>(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase())));
}

#endif

JSValuePtr jsDOMWindowBaseOption(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSOptionConstructor>(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase())));
}

JSValuePtr jsDOMWindowBaseWebKitCSSMatrix(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSWebKitCSSMatrixConstructor>(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase())));
}
 
JSValuePtr jsDOMWindowBaseXMLHttpRequest(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSXMLHttpRequestConstructor>(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase())));
}

JSValuePtr jsDOMWindowBaseAudio(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
#if ENABLE(VIDEO)
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    if (!MediaPlayer::isAvailable())
        return jsUndefined();
    return getDOMConstructor<JSAudioConstructor>(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase())));
#else
    return jsUndefined();
#endif
}

JSValuePtr jsDOMWindowBaseWorker(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
#if ENABLE(WORKERS)
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSWorkerConstructor>(exec);
#else
    return jsUndefined();
#endif
}

JSValuePtr jsDOMWindowBaseXSLTProcessor(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
#if ENABLE(XSLT)
    if (!static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSXSLTProcessorConstructor>(exec);
#else
    return jsUndefined();
#endif
}

void setJSDOMWindowBaseEvent(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "Event"), value);
}

void setJSDOMWindowBaseAudio(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "Audio"), value);
}

void setJSDOMWindowBaseImage(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "Image"), value);
}

void setJSDOMWindowBaseMessageChannel(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "MessageChannel"), value);
}

void setJSDOMWindowBaseOption(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "Option"), value);
}

void setJSDOMWindowBaseWorker(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "Worker"), value);
}

void setJSDOMWindowBaseWebKitCSSMatrix(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "WebKitCSSMatrix"), value);
}

void setJSDOMWindowBaseXMLHttpRequest(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "XMLHttpRequest"), value);
}

void setJSDOMWindowBaseXSLTProcessor(ExecState* exec, JSObject* thisObject, JSValuePtr value)
{
    if (!static_cast<JSDOMWindowBase*>(thisObject)->allowsAccessFrom(exec))
        return;
    // Shadowing a built-in constructor
    static_cast<JSDOMWindowBase*>(thisObject)->putDirect(Identifier(exec, "XSLTProcessor"), value);
}

namespace WebCore {

JSValuePtr JSDOMWindowBase::childFrameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->impl()->frame()->tree()->child(AtomicString(propertyName))->domWindow());
}

JSValuePtr JSDOMWindowBase::indexGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->impl()->frame()->tree()->child(slot.index())->domWindow());
}

JSValuePtr JSDOMWindowBase::namedItemGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSDOMWindowBase* thisObj = static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()));
    Document* doc = thisObj->impl()->frame()->document();
    ASSERT(thisObj->allowsAccessFrom(exec));
    ASSERT(doc);
    ASSERT(doc->isHTMLDocument());

    RefPtr<HTMLCollection> collection = doc->windowNamedItems(propertyName);
    if (collection->length() == 1)
        return toJS(exec, collection->firstItem());
    return toJS(exec, collection.get());
}

bool JSDOMWindowBase::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // Check for child frames by name before built-in properties to
    // match Mozilla. This does not match IE, but some sites end up
    // naming frames things that conflict with window properties that
    // are in Moz but not IE. Since we have some of these, we have to do
    // it the Moz way.
    if (impl()->frame()->tree()->child(propertyName)) {
        slot.setCustom(this, childFrameGetter);
        return true;
    }

    const HashEntry* entry = JSDOMWindowBaseTable.entry(exec, propertyName);
    if (entry) {
        if (entry->attributes() & Function) {
            if (entry->function() == windowProtoFuncShowModalDialog) {
                if (!canShowModalDialog(impl()->frame()))
                    return false;
            }
            if (allowsAccessFrom(exec))
                setUpStaticFunctionSlot(exec, entry, this, propertyName, slot);
            else
                slot.setUndefined();
        } else
            slot.setCustom(this, entry->propertyGetter());
        return true;
    }

    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.  
    JSValuePtr proto = prototype();
    if (proto.isObject()) {
        if (asObject(proto)->getPropertySlot(exec, propertyName, slot)) {
            if (!allowsAccessFrom(exec))
                slot.setUndefined();
            return true;
        }
    }

    // FIXME: Search the whole frame hierachy somewhere around here.
    // We need to test the correct priority order.

    // allow window[1] or parent[1] etc. (#56983)
    bool ok;
    unsigned i = propertyName.toArrayIndex(&ok);
    if (ok && i < impl()->frame()->tree()->childCount()) {
        slot.setCustomIndex(this, i, indexGetter);
        return true;
    }

    if (!allowsAccessFrom(exec)) {
        slot.setUndefined();
        return true;
    }

    // Allow shortcuts like 'Image1' instead of document.images.Image1
    Document* document = impl()->frame()->document();
    if (document && document->isHTMLDocument()) {
        AtomicStringImpl* atomicPropertyName = AtomicString::find(propertyName);
        if (atomicPropertyName && (static_cast<HTMLDocument*>(document)->hasNamedItem(atomicPropertyName) || document->hasElementWithId(atomicPropertyName))) {
            slot.setCustom(this, namedItemGetter);
            return true;
        }
    }

    return Base::getOwnPropertySlot(exec, propertyName, slot);
}

void JSDOMWindowBase::put(ExecState* exec, const Identifier& propertyName, JSValuePtr value, PutPropertySlot& slot)
{
    const HashEntry* entry = JSDOMWindowBaseTable.entry(exec, propertyName);
    if (entry) {
        if (entry->attributes() & Function) {
            if (allowsAccessFrom(exec))
                Base::put(exec, propertyName, value, slot);
            return;
        }
        if (entry->attributes() & ReadOnly)
            return;
    }

    if (allowsAccessFrom(exec))
        Base::put(exec, propertyName, value, slot);
}

String JSDOMWindowBase::crossDomainAccessErrorMessage(const JSGlobalObject* other) const
{
    KURL originURL = asJSDOMWindow(other)->impl()->url();
    KURL targetURL = impl()->frame()->document()->url();
    if (originURL.isNull() || targetURL.isNull())
        return String();

    // FIXME: this error message should contain more specifics of why the same origin check has failed.
    return String::format("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains, protocols and ports must match.\n",
        targetURL.string().utf8().data(), originURL.string().utf8().data());
}

void JSDOMWindowBase::printErrorMessage(const String& message) const
{
    if (message.isEmpty())
        return;

    Frame* frame = impl()->frame();
    if (!frame)
        return;

    Settings* settings = frame->settings();
    if (!settings)
        return;
    
    if (settings->privateBrowsingEnabled())
        return;

    impl()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, 1, String()); // FIXME: provide a real line number and source URL.
}

ExecState* JSDOMWindowBase::globalExec()
{
    // We need to make sure that any script execution happening in this
    // frame does not destroy it
    if (Frame *frame = impl()->frame())
        frame->keepAlive();
    return Base::globalExec();
}

bool JSDOMWindowBase::supportsProfiling() const
{
    Frame* frame = impl()->frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    return page->inspectorController()->profilerEnabled();
}

bool JSDOMWindowBase::shouldInterruptScript() const
{
    ASSERT(impl()->frame());
    Page* page = impl()->frame()->page();

    // See <rdar://problem/5479443>. We don't think that page can ever be NULL
    // in this case, but if it is, we've gotten into a state where we may have
    // hung the UI, with no way to ask the client whether to cancel execution.
    // For now, our solution is just to cancel execution no matter what,
    // ensuring that we never hang. We might want to consider other solutions
    // if we discover problems with this one.
    ASSERT(page);
    if (!page)
        return true;

    return page->chrome()->shouldInterruptJavaScript();
}

void JSDOMWindowBase::clearHelperObjectProperties()
{
    setCurrentEvent(0);
}

void JSDOMWindowBase::clear()
{
    JSLock lock(false);

    if (d()->returnValueSlot && !*d()->returnValueSlot)
        *d()->returnValueSlot = getDirect(Identifier(globalExec(), "returnValue"));

    clearHelperObjectProperties();
}

JSObject* JSDOMWindowBase::toThisObject(ExecState*) const
{
    return shell();
}

JSDOMWindowShell* JSDOMWindowBase::shell() const
{
    return d()->shell;
}

JSGlobalData* JSDOMWindowBase::commonJSGlobalData()
{
    static JSGlobalData* globalData = JSGlobalData::createLeaked().releaseRef();
    return globalData;
}

} // namespace WebCore

using namespace WebCore;

JSValuePtr windowProtoFuncOpen(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
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
    if (!allowPopUp(exec) && (frameName.isEmpty() || !frame->tree()->find(frameName)))
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
            frame->loader()->scheduleLocationChange(completedURL, activeFrame->loader()->outgoingReferrer(), false, false, userGesture);
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

    frame = createWindow(exec, frame, urlString, frameName, windowFeatures, noValue());

    if (!frame)
        return jsUndefined();

    return toJS(exec, frame->domWindow()); // global object
}

JSValuePtr windowProtoFuncShowModalDialog(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    return showModalDialog(exec, frame, valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0)), args.at(exec, 1), valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 2)));
}

JSValuePtr windowProtoFuncNotImplemented(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!toJSDOMWindow(thisValue))
        return throwError(exec, TypeError);
    return jsUndefined();
}

namespace WebCore {

void JSDOMWindowBase::setReturnValueSlot(JSValuePtr* slot)
{
    d()->returnValueSlot = slot;
}

////////////////////// timeouts ////////////////////////

int JSDOMWindowBase::installTimeout(ScheduledAction* a, int t, bool singleShot)
{
    return DOMTimer::install(scriptExecutionContext(), a, t, singleShot);
}

int JSDOMWindowBase::installTimeout(const UString& handler, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(handler), t, singleShot);
}

int JSDOMWindowBase::installTimeout(ExecState* exec, JSValuePtr func, const ArgList& args, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(exec, func, args), t, singleShot);
}

void JSDOMWindowBase::removeTimeout(int timeoutId)
{
    DOMTimer::removeById(scriptExecutionContext(), timeoutId);
}

void JSDOMWindowBase::disconnectFrame()
{
}

JSValuePtr toJS(ExecState*, DOMWindow* domWindow)
{
    if (!domWindow)
        return jsNull();
    Frame* frame = domWindow->frame();
    if (!frame)
        return jsNull();
    return frame->script()->windowShell();
}

JSDOMWindow* toJSDOMWindow(Frame* frame)
{
    if (!frame)
        return 0;
    return frame->script()->windowShell()->window();
}

JSDOMWindow* toJSDOMWindow(JSValuePtr value)
{
    if (!value.isObject())
        return 0;
    const ClassInfo* classInfo = asObject(value)->classInfo();
    if (classInfo == &JSDOMWindow::s_info)
        return static_cast<JSDOMWindow*>(asObject(value));
    if (classInfo == &JSDOMWindowShell::s_info)
        return static_cast<JSDOMWindowShell*>(asObject(value))->window();
    return 0;
}

} // namespace WebCore
