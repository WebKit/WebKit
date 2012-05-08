/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "config.h"

#include "PagePopupBlackBerry.h"

#include "ChromeClientBlackBerry.h"
#include "EmptyClients.h"
#include "FrameView.h"
#include "InspectorClientBlackBerry.h"
#include "JSDOMBinding.h"
#include "JSDOMWindowBase.h"
#include "JSObject.h"
#include "JSRetainPtr.h"
#include "Page.h"
#include "PageGroup.h"
#include "PagePopupClient.h"
#include "PlatformMouseEvent.h"
#include "Settings.h"
#include "WebPage.h"
#include "WebPage_p.h"

#include <JavaScriptCore/API/JSCallbackObject.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>

// Fixme: should get the height from runtime.
#define URL_BAR_HEIGHT 70

using namespace BlackBerry::Platform::Graphics;
using namespace BlackBerry::WebKit;
namespace WebCore {

class PagePopupChromeClient : public ChromeClientBlackBerry {
public:
    explicit PagePopupChromeClient(WebPagePrivate* webpage, PagePopupBlackBerry* popup)
        : ChromeClientBlackBerry(webpage)
        , m_popup(popup)
        , m_webPage(webpage)
    {
    }

    virtual void closeWindowSoon()
    {
        m_popup->closePopup();
    }

    WebPagePrivate* webPage()
    {
        return m_webPage;
    }

    PagePopupBlackBerry* m_popup;
    WebPagePrivate* m_webPage;
    IntRect m_rect;
};

PagePopupBlackBerry::PagePopupBlackBerry(BlackBerry::WebKit::WebPagePrivate* webPage, PagePopupClient* client, const IntRect& rect)
    : m_webPagePrivate(webPage)
    , m_client(adoptPtr(client))
{
    m_rect = IntRect(rect.x(), rect.y() - URL_BAR_HEIGHT, client->contentSize().width(), client->contentSize().height());
}

PagePopupBlackBerry::~PagePopupBlackBerry()
{
}

void PagePopupBlackBerry::sendCreatePopupWebViewRequest()
{
    m_webPagePrivate->client()->createPopupWebView(m_rect);
}

bool PagePopupBlackBerry::init(WebPage* webpage)
{
    static FrameLoaderClient* emptyFrameLoaderClient = new EmptyFrameLoaderClient;
    Page::PageClients pageClients;
    m_chromeClient = adoptPtr(new PagePopupChromeClient(webpage->d, this));
    static EditorClient* emptyEditorClient = new EmptyEditorClient;
    pageClients.chromeClient = m_chromeClient.get();
    pageClients.editorClient = emptyEditorClient;
#if ENABLE(CONTEXT_MENUS)
    static ContextMenuClient* emptyContextMenuClient = new EmptyContextMenuClient;
    pageClients.contextMenuClient = emptyContextMenuClient;
#endif
#if ENABLE(DRAG_SUPPORT)
    static DragClient* emptyDragClient = new EmptyDragClient;
    pageClients.dragClient = emptyDragClient;
#endif
#if ENABLE(INSPECTOR)
    static InspectorClient* emptyInspectorClient = new EmptyInspectorClient;
    pageClients.inspectorClient = emptyInspectorClient;
#endif

    m_page = adoptPtr(new Page(pageClients));
    m_page->settings()->setScriptEnabled(true);
    m_page->settings()->setAllowScriptsToCloseWindows(true);

    RefPtr<Frame> frame = Frame::create(m_page.get(), 0,
            emptyFrameLoaderClient);
    frame->setView(FrameView::create(frame.get()));
    frame->init();
    frame->view()->resize(m_client->contentSize());

    CString htmlSource = m_client->htmlSource().utf8();
    DocumentWriter* writer = frame->loader()->activeDocumentLoader()->writer();
    m_client->writeDocument(*writer);

    installDomFunction(frame.get());

    webpage->d->setParentPopup(this);

    return true;
}

static JSValueRef setValueAndClosePopupCallback(JSContextRef context,
        JSObjectRef, JSObjectRef, size_t argumentCount,
        const JSValueRef arguments[], JSValueRef*)
{
    JSValueRef jsRetVal = JSValueMakeUndefined(context);
    if (argumentCount <= 0)
        return jsRetVal;

    JSStringRef string = JSValueToStringCopy(context, arguments[0], 0);
    size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(string);
    WTF::Vector<char> strArgs(sizeUTF8 + 1);
    strArgs[sizeUTF8] = 0;
    JSStringGetUTF8CString(string, strArgs.data(), sizeUTF8);
    JSStringRelease(string);
    JSObjectRef popUpObject = JSValueToObject(context,
            arguments[argumentCount - 1], 0);
    PagePopupClient* client =
            reinterpret_cast<PagePopupClient*>(JSObjectGetPrivate(popUpObject));

    ASSERT(client);
    client->setValueAndClosePopup(0, strArgs.data());

    return jsRetVal;
}

static void popUpExtensionInitialize(JSContextRef context, JSObjectRef object)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
}

static void popUpExtensionFinalize(JSObjectRef object)
{
    UNUSED_PARAM(object);
}

static JSStaticFunction popUpExtensionStaticFunctions[] =
{
{ 0, 0, 0 },
{ 0, 0, 0 }
};

static JSStaticValue popUpExtensionStaticValues[] =
{
{ 0, 0, 0, 0 }
};

void PagePopupBlackBerry::installDomFunction(Frame* frame)
{
    JSC::JSLock lock(JSC::SilenceAssertionsOnly);

    JSDOMWindow* window = toJSDOMWindow(frame, mainThreadNormalWorld());
    ASSERT(window);

    JSC::ExecState* exec = window->globalExec();
    ASSERT(exec);

    JSContextRef context = ::toRef(exec);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    JSStringRef functionName = JSStringCreateWithUTF8CString(
            "setValueAndClosePopup");
    JSObjectRef function = JSObjectMakeFunctionWithCallback(context,
            functionName, setValueAndClosePopupCallback);
    JSObjectSetProperty(context, globalObject, functionName, function,
            kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly, 0);

    // Register client into DOM
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.staticValues = popUpExtensionStaticValues;
    definition.staticFunctions = popUpExtensionStaticFunctions;
    definition.initialize = popUpExtensionInitialize;
    definition.finalize = popUpExtensionFinalize;
    JSClassRef clientClass = JSClassCreate(&definition);

    JSObjectRef clientClassObject = JSObjectMake(context, clientClass, 0);
    JSObjectSetPrivate(clientClassObject, reinterpret_cast<void*>(m_client.get()));

    JSC::UString name("popUp");

    JSC::PutPropertySlot slot;
    window->put(window, exec, JSC::Identifier(exec, name),
            toJS(clientClassObject), slot);

    JSClassRelease(clientClass);
}

bool PagePopupBlackBerry::handleMouseEvent(PlatformMouseEvent& event)
{
    if (!m_page->mainFrame() || !m_page->mainFrame()->view())
        return false;

    switch (event.type()) {
    case PlatformEvent::MouseMoved:
        return m_page->mainFrame()->eventHandler()->handleMouseMoveEvent(event);
    case PlatformEvent::MousePressed:
        return m_page->mainFrame()->eventHandler()->handleMousePressEvent(event);
    case PlatformEvent::MouseReleased:
        return m_page->mainFrame()->eventHandler()->handleMouseReleaseEvent(event);
    default:
        return false;
    }
}

void PagePopupBlackBerry::closePopup()
{
    closeWebPage();
    m_client->didClosePopup();
    m_webPagePrivate->client()->closePopupWebView();
}

void PagePopupBlackBerry::closeWebPage()
{
    if (!m_page)
        return;

    m_page->setGroupName(String());
    m_page->mainFrame()->loader()->stopAllLoaders();
    m_page->mainFrame()->loader()->stopLoading(UnloadEventPolicyNone);
    m_page->mainFrame()->view()->clear();
    m_page.clear();
}
}

