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

#include "EmptyClients.h"
#include "FrameView.h"
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
#define PADDING 80

using namespace BlackBerry::Platform::Graphics;
using namespace BlackBerry::WebKit;
namespace WebCore {

PagePopupBlackBerry::PagePopupBlackBerry(BlackBerry::WebKit::WebPagePrivate* webPage, PagePopupClient* client, const IntRect& rect)
    : m_webPagePrivate(webPage)
    , m_client(adoptPtr(client))
{
    m_rect = IntRect(rect.x(), rect.y() - URL_BAR_HEIGHT, client->contentSize().width(), client->contentSize().height());
}

PagePopupBlackBerry::~PagePopupBlackBerry()
{
}

bool PagePopupBlackBerry::sendCreatePopupWebViewRequest()
{
    return m_webPagePrivate->client()->createPopupWebView(m_rect);
}

bool PagePopupBlackBerry::init(WebPage* webpage)
{
    generateHTML(webpage);

    installDomFunction(webpage->d->mainFrame());

    return true;
}

void PagePopupBlackBerry::generateHTML(WebPage* webpage)
{
    DocumentWriter* writer = webpage->d->mainFrame()->loader()->activeDocumentLoader()->writer();
    writer->setMIMEType("text/html");
    writer->begin(KURL());

    // All the popups have the same html head and the page content should be non-zoomable.
    StringBuilder source;
    // FIXME: the hardcoding padding will be removed soon.
    int screenWidth = webpage->d->screenSize().width() - PADDING;
    source.append("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/>\n");
    source.append("<meta name=\"viewport\" content=\"width=" + String::number(screenWidth));
    source.append("; user-scalable=no\" />\n");
    writer->addData(source.toString().utf8().data(), source.toString().utf8().length());

    m_client->writeDocument(*writer);
    writer->end();
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
    Vector<char> strArgs(sizeUTF8 + 1);
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
    JSDOMWindow* window = toJSDOMWindow(frame, mainThreadNormalWorld());
    ASSERT(window);

    JSC::ExecState* exec = window->globalExec();
    ASSERT(exec);
    JSC::JSLockHolder lock(exec);

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

void PagePopupBlackBerry::closePopup()
{
    m_client->didClosePopup();
    m_webPagePrivate->client()->closePopupWebView();
    m_webPagePrivate->m_webPage->popupClosed();
}

}

