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
#include "PlatformSupport.h"

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "FileMetadata.h"
#include "Page.h"
#include "WebFileInfo.h"
#include "WebFileUtilities.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebPluginContainerImpl.h"
#include "WebPluginListBuilderImpl.h"
#include "WebSandboxSupport.h"
#include "WebScreenInfo.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "WebWorkerClientImpl.h"
#include "platform/WebAudioBus.h"
#include "platform/WebData.h"
#include "platform/WebDragData.h"
#include "platform/WebImage.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebSerializedScriptValue.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "platform/WebVector.h"

#if OS(WINDOWS)
#include "platform/WebRect.h"
#include <public/win/WebThemeEngine.h>
#endif

#if OS(DARWIN)
#include <public/mac/WebThemeEngine.h>
#elif OS(UNIX)
#include "WebFontRenderStyle.h"
#if OS(ANDROID)
#include <public/android/WebThemeEngine.h>
#else
#include "WebFontInfo.h"
#include <public/linux/WebThemeEngine.h>
#endif // OS(ANDROID)
#endif // elif OS(UNIX)

#include "NativeImageSkia.h"

#include "BitmapImage.h"
#include "Cookie.h"
#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IDBFactoryBackendProxy.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "PluginData.h"
#include "SharedBuffer.h"

#include "Worker.h"
#include "WorkerContextProxy.h"
#include <public/WebCookie.h>
#include <public/WebCookieJar.h>
#include <public/WebMimeRegistry.h>
#include <wtf/Assertions.h>

// We are part of the WebKit implementation.
using namespace WebKit;

namespace WebCore {

static WebWidgetClient* toWebWidgetClient(Widget* widget)
{
    if (!widget)
        return 0;

    FrameView* view;
    if (widget->isFrameView())
        view = static_cast<FrameView*>(widget);
    else if (widget->parent() && widget->parent()->isFrameView())
        view = static_cast<FrameView*>(widget->parent());
    else
        return 0;

    Page* page = view->frame() ? view->frame()->page() : 0;
    if (!page)
        return 0;

    void* webView = page->chrome()->client()->webView();
    if (!webView)
        return 0;

    return static_cast<WebViewImpl*>(webView)->client();
}

static WebCookieJar* getCookieJar(const Document* document)
{
    WebFrameImpl* frameImpl = WebFrameImpl::fromFrame(document->frame());
    if (!frameImpl || !frameImpl->client())
        return 0;
    WebCookieJar* cookieJar = frameImpl->client()->cookieJar(frameImpl);
    if (!cookieJar)
        cookieJar = WebKit::Platform::current()->cookieJar();
    return cookieJar;
}

// Cookies --------------------------------------------------------------------

void PlatformSupport::setCookies(const Document* document, const KURL& url,
                                const String& value)
{
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        cookieJar->setCookie(url, document->firstPartyForCookies(), value);
}

String PlatformSupport::cookies(const Document* document, const KURL& url)
{
    String result;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookies(url, document->firstPartyForCookies());
    return result;
}

String PlatformSupport::cookieRequestHeaderFieldValue(const Document* document,
                                                     const KURL& url)
{
    String result;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookieRequestHeaderFieldValue(url, document->firstPartyForCookies());
    return result;
}

bool PlatformSupport::rawCookies(const Document* document, const KURL& url, Vector<Cookie>& rawCookies)
{
    rawCookies.clear();
    WebVector<WebCookie> webCookies;

    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        cookieJar->rawCookies(url, document->firstPartyForCookies(), webCookies);

    for (unsigned i = 0; i < webCookies.size(); ++i) {
        const WebCookie& webCookie = webCookies[i];
        Cookie cookie(webCookie.name,
                      webCookie.value,
                      webCookie.domain,
                      webCookie.path,
                      webCookie.expires,
                      webCookie.httpOnly,
                      webCookie.secure,
                      webCookie.session);
        rawCookies.append(cookie);
    }
    return true;
}

void PlatformSupport::deleteCookie(const Document* document, const KURL& url, const String& cookieName)
{
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        cookieJar->deleteCookie(url, cookieName);
}

bool PlatformSupport::cookiesEnabled(const Document* document)
{
    bool result = false;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookiesEnabled(document->cookieURL(), document->firstPartyForCookies());
    return result;
}

// Font -----------------------------------------------------------------------

#if OS(WINDOWS)
bool PlatformSupport::ensureFontLoaded(HFONT font)
{
    WebSandboxSupport* ss = WebKit::Platform::current()->sandboxSupport();

    // if there is no sandbox, then we can assume the font
    // was able to be loaded successfully already
    return ss ? ss->ensureFontLoaded(font) : true;
}
#endif

#if OS(DARWIN)
bool PlatformSupport::loadFont(NSFont* srcFont, CGFontRef* out, uint32_t* fontID)
{
    WebSandboxSupport* ss = WebKit::Platform::current()->sandboxSupport();
    if (ss)
        return ss->loadFont(srcFont, out, fontID);

    // This function should only be called in response to an error loading a
    // font due to being blocked by the sandbox.
    // This by definition shouldn't happen if there is no sandbox support.
    ASSERT_NOT_REACHED();
    *out = 0;
    *fontID = 0;
    return false;
}
#elif OS(UNIX)
void PlatformSupport::getFontFamilyForCharacters(const UChar* characters, size_t numCharacters, const char* preferredLocale, FontFamily* family)
{
#if OS(ANDROID)
    // FIXME: We do not use fontconfig on Android, so use simple logic for now.
    // https://bugs.webkit.org/show_bug.cgi?id=67587
    family->name = "Arial";
    family->isBold = false;
    family->isItalic = false;
#else
    WebFontFamily webFamily;
    if (WebKit::Platform::current()->sandboxSupport())
        WebKit::Platform::current()->sandboxSupport()->getFontFamilyForCharacters(characters, numCharacters, preferredLocale, &webFamily);
    else
        WebFontInfo::familyForChars(characters, numCharacters, preferredLocale, &webFamily);
    family->name = String::fromUTF8(webFamily.name.data(), webFamily.name.length());
    family->isBold = webFamily.isBold;
    family->isItalic = webFamily.isItalic;
#endif
}

#endif

// Indexed Database -----------------------------------------------------------

PassRefPtr<IDBFactoryBackendInterface> PlatformSupport::idbFactory()
{
    // There's no reason why we need to allocate a new proxy each time, but
    // there's also no strong reason not to.
    return IDBFactoryBackendProxy::create();
}

// Plugin ---------------------------------------------------------------------

bool PlatformSupport::plugins(bool refresh, Vector<PluginInfo>* results)
{
    WebPluginListBuilderImpl builder(results);
    webKitPlatformSupport()->getPluginList(refresh, &builder);
    return true;  // FIXME: There is no need for this function to return a value.
}

NPObject* PlatformSupport::pluginScriptableObject(Widget* widget)
{
    if (!widget || !widget->isPluginContainer())
        return 0;

    return static_cast<WebPluginContainerImpl*>(widget)->scriptableObject();
}

// Theming --------------------------------------------------------------------

#if OS(WINDOWS)

void PlatformSupport::paintButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    WebKit::Platform::current()->themeEngine()->paintButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintMenuList(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    WebKit::Platform::current()->themeEngine()->paintMenuList(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintScrollbarArrow(
    GraphicsContext* gc, int state, int classicState,
    const IntRect& rect)
{
    WebKit::Platform::current()->themeEngine()->paintScrollbarArrow(
        gc->platformContext()->canvas(), state, classicState, rect);
}

void PlatformSupport::paintScrollbarThumb(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    WebKit::Platform::current()->themeEngine()->paintScrollbarThumb(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintScrollbarTrack(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const IntRect& alignRect)
{
    WebKit::Platform::current()->themeEngine()->paintScrollbarTrack(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        alignRect);
}

void PlatformSupport::paintSpinButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    WebKit::Platform::current()->themeEngine()->paintSpinButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintTextField(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const Color& color, bool fillContentArea,
    bool drawEdges)
{
    // Fallback to white when |color| is invalid.
    RGBA32 backgroundColor = color.isValid() ? color.rgb() : Color::white;

    WebKit::Platform::current()->themeEngine()->paintTextField(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        backgroundColor, fillContentArea, drawEdges);
}

void PlatformSupport::paintTrackbar(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    WebKit::Platform::current()->themeEngine()->paintTrackbar(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintProgressBar(
    GraphicsContext* gc, const IntRect& barRect, const IntRect& valueRect, bool determinate, double animatedSeconds)
{
    WebKit::Platform::current()->themeEngine()->paintProgressBar(
        gc->platformContext()->canvas(), barRect, valueRect, determinate, animatedSeconds);
}

#elif OS(DARWIN)

void PlatformSupport::paintScrollbarThumb(
    GraphicsContext* gc, ThemePaintState state, ThemePaintSize size, const IntRect& rect, const ThemePaintScrollbarInfo& scrollbarInfo)
{
    WebThemeEngine::ScrollbarInfo webThemeScrollbarInfo;

    webThemeScrollbarInfo.orientation = static_cast<WebThemeEngine::ScrollbarOrientation>(scrollbarInfo.orientation);
    webThemeScrollbarInfo.parent = static_cast<WebThemeEngine::ScrollbarParent>(scrollbarInfo.parent);
    webThemeScrollbarInfo.maxValue = scrollbarInfo.maxValue;
    webThemeScrollbarInfo.currentValue = scrollbarInfo.currentValue;
    webThemeScrollbarInfo.visibleSize = scrollbarInfo.visibleSize;
    webThemeScrollbarInfo.totalSize = scrollbarInfo.totalSize;

    WebKit::WebCanvas* webCanvas = gc->platformContext()->canvas();
    WebKit::Platform::current()->themeEngine()->paintScrollbarThumb(
        webCanvas,
        static_cast<WebThemeEngine::State>(state),
        static_cast<WebThemeEngine::Size>(size),
        rect,
        webThemeScrollbarInfo);
}

#elif OS(UNIX)

static WebThemeEngine::Part WebThemePart(PlatformSupport::ThemePart part)
{
    switch (part) {
    case PlatformSupport::PartScrollbarDownArrow: return WebThemeEngine::PartScrollbarDownArrow;
    case PlatformSupport::PartScrollbarLeftArrow: return WebThemeEngine::PartScrollbarLeftArrow;
    case PlatformSupport::PartScrollbarRightArrow: return WebThemeEngine::PartScrollbarRightArrow;
    case PlatformSupport::PartScrollbarUpArrow: return WebThemeEngine::PartScrollbarUpArrow;
    case PlatformSupport::PartScrollbarHorizontalThumb: return WebThemeEngine::PartScrollbarHorizontalThumb;
    case PlatformSupport::PartScrollbarVerticalThumb: return WebThemeEngine::PartScrollbarVerticalThumb;
    case PlatformSupport::PartScrollbarHorizontalTrack: return WebThemeEngine::PartScrollbarHorizontalTrack;
    case PlatformSupport::PartScrollbarVerticalTrack: return WebThemeEngine::PartScrollbarVerticalTrack;
    case PlatformSupport::PartCheckbox: return WebThemeEngine::PartCheckbox;
    case PlatformSupport::PartRadio: return WebThemeEngine::PartRadio;
    case PlatformSupport::PartButton: return WebThemeEngine::PartButton;
    case PlatformSupport::PartTextField: return WebThemeEngine::PartTextField;
    case PlatformSupport::PartMenuList: return WebThemeEngine::PartMenuList;
    case PlatformSupport::PartSliderTrack: return WebThemeEngine::PartSliderTrack;
    case PlatformSupport::PartSliderThumb: return WebThemeEngine::PartSliderThumb;
    case PlatformSupport::PartInnerSpinButton: return WebThemeEngine::PartInnerSpinButton;
    case PlatformSupport::PartProgressBar: return WebThemeEngine::PartProgressBar;
    }
    ASSERT_NOT_REACHED();
    return WebThemeEngine::PartScrollbarDownArrow;
}

static WebThemeEngine::State WebThemeState(PlatformSupport::ThemePaintState state)
{
    switch (state) {
    case PlatformSupport::StateDisabled: return WebThemeEngine::StateDisabled;
    case PlatformSupport::StateHover: return WebThemeEngine::StateHover;
    case PlatformSupport::StateNormal: return WebThemeEngine::StateNormal;
    case PlatformSupport::StatePressed: return WebThemeEngine::StatePressed;
    }
    ASSERT_NOT_REACHED();
    return WebThemeEngine::StateDisabled;
}

static void GetWebThemeExtraParams(PlatformSupport::ThemePart part, PlatformSupport::ThemePaintState state, const PlatformSupport::ThemePaintExtraParams* extraParams, WebThemeEngine::ExtraParams* webThemeExtraParams)
{
    switch (part) {
    case PlatformSupport::PartScrollbarHorizontalTrack:
    case PlatformSupport::PartScrollbarVerticalTrack:
        webThemeExtraParams->scrollbarTrack.trackX = extraParams->scrollbarTrack.trackX;
        webThemeExtraParams->scrollbarTrack.trackY = extraParams->scrollbarTrack.trackY;
        webThemeExtraParams->scrollbarTrack.trackWidth = extraParams->scrollbarTrack.trackWidth;
        webThemeExtraParams->scrollbarTrack.trackHeight = extraParams->scrollbarTrack.trackHeight;
        break;
    case PlatformSupport::PartCheckbox:
        webThemeExtraParams->button.checked = extraParams->button.checked;
        webThemeExtraParams->button.indeterminate = extraParams->button.indeterminate;
        break;
    case PlatformSupport::PartRadio:
        webThemeExtraParams->button.checked = extraParams->button.checked;
        break;
    case PlatformSupport::PartButton:
        webThemeExtraParams->button.isDefault = extraParams->button.isDefault;
        webThemeExtraParams->button.hasBorder = extraParams->button.hasBorder;
        webThemeExtraParams->button.backgroundColor = extraParams->button.backgroundColor;
        break;
    case PlatformSupport::PartTextField:
        webThemeExtraParams->textField.isTextArea = extraParams->textField.isTextArea;
        webThemeExtraParams->textField.isListbox = extraParams->textField.isListbox;
        webThemeExtraParams->textField.backgroundColor = extraParams->textField.backgroundColor;
        break;
    case PlatformSupport::PartMenuList:
        webThemeExtraParams->menuList.hasBorder = extraParams->menuList.hasBorder;
        webThemeExtraParams->menuList.hasBorderRadius = extraParams->menuList.hasBorderRadius;
        webThemeExtraParams->menuList.arrowX = extraParams->menuList.arrowX;
        webThemeExtraParams->menuList.arrowY = extraParams->menuList.arrowY;
        webThemeExtraParams->menuList.backgroundColor = extraParams->menuList.backgroundColor;
        break;
    case PlatformSupport::PartSliderTrack:
    case PlatformSupport::PartSliderThumb:
        webThemeExtraParams->slider.vertical = extraParams->slider.vertical;
        webThemeExtraParams->slider.inDrag = extraParams->slider.inDrag;
        break;
    case PlatformSupport::PartInnerSpinButton:
        webThemeExtraParams->innerSpin.spinUp = extraParams->innerSpin.spinUp;
        webThemeExtraParams->innerSpin.readOnly = extraParams->innerSpin.readOnly;
        break;
    case PlatformSupport::PartProgressBar:
        webThemeExtraParams->progressBar.determinate = extraParams->progressBar.determinate;
        webThemeExtraParams->progressBar.valueRectX = extraParams->progressBar.valueRectX;
        webThemeExtraParams->progressBar.valueRectY = extraParams->progressBar.valueRectY;
        webThemeExtraParams->progressBar.valueRectWidth = extraParams->progressBar.valueRectWidth;
        webThemeExtraParams->progressBar.valueRectHeight = extraParams->progressBar.valueRectHeight;
        break;
    default:
        break; // Parts that have no extra params get here.
    }
}

IntSize PlatformSupport::getThemePartSize(ThemePart part)
{
     return WebKit::Platform::current()->themeEngine()->getSize(WebThemePart(part));
}

void PlatformSupport::paintThemePart(
    GraphicsContext* gc, ThemePart part, ThemePaintState state, const IntRect& rect, const ThemePaintExtraParams* extraParams)
{
    WebThemeEngine::ExtraParams webThemeExtraParams;
    GetWebThemeExtraParams(part, state, extraParams, &webThemeExtraParams);
    WebKit::Platform::current()->themeEngine()->paint(
        gc->platformContext()->canvas(), WebThemePart(part), WebThemeState(state), rect, &webThemeExtraParams);
}

#endif

// These are temporary methods that the WebKit layer can use to call to the
// Glue layer. Once the Glue layer moves entirely into the WebKit layer, these
// methods will be deleted.

void PlatformSupport::notifyJSOutOfMemory(Frame* frame)
{
    if (!frame)
        return;

    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);
    if (!webFrame->client())
        return;
    webFrame->client()->didExhaustMemoryAvailableForScript(webFrame);
}

int PlatformSupport::screenHorizontalDPI(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().horizontalDPI;
}

int PlatformSupport::screenVerticalDPI(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().verticalDPI;
}

int PlatformSupport::screenDepth(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().depth;
}

int PlatformSupport::screenDepthPerComponent(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().depthPerComponent;
}

bool PlatformSupport::screenIsMonochrome(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().isMonochrome;
}

IntRect PlatformSupport::screenRect(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return IntRect();
    return client->screenInfo().rect;
}

IntRect PlatformSupport::screenAvailableRect(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return IntRect();
    return client->screenInfo().availableRect;
}

#if ENABLE(WORKERS)
WorkerContextProxy* WorkerContextProxy::create(Worker* worker)
{
    return WebWorkerClientImpl::createWorkerContextProxy(worker);
}
#endif

} // namespace WebCore
