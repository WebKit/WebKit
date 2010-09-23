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
#include "ChromiumBridge.h"

#include <googleurl/src/url_util.h>

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "WebClipboard.h"
#include "WebCookie.h"
#include "WebCookieJar.h"
#include "WebCursorInfo.h"
#include "WebData.h"
#include "WebDragData.h"
#include "WebFileUtilities.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebIDBKey.h"
#include "WebImage.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebMimeRegistry.h"
#include "WebPluginContainerImpl.h"
#include "WebPluginListBuilderImpl.h"
#include "WebSandboxSupport.h"
#include "WebSerializedScriptValue.h"
#include "WebScreenInfo.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebVector.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "WebWorkerClientImpl.h"

#if OS(WINDOWS)
#include "WebRect.h"
#include "WebThemeEngine.h"
#endif

#if OS(LINUX) || OS(FREEBSD)
#include "WebFontInfo.h"
#include "WebFontRenderStyle.h"
#endif

#if WEBKIT_USING_SKIA
#include "NativeImageSkia.h"
#endif

#include "BitmapImage.h"
#include "Cookie.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IDBFactoryBackendProxy.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "PluginData.h"
#include "SharedBuffer.h"
#include "WebGeolocationServiceBridgeImpl.h"
#include "Worker.h"
#include "WorkerContextProxy.h"
#include <wtf/Assertions.h>

// We are part of the WebKit implementation.
using namespace WebKit;

namespace WebCore {

static ChromeClientImpl* toChromeClientImpl(Widget* widget)
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

    return static_cast<ChromeClientImpl*>(page->chrome()->client());
}

static WebWidgetClient* toWebWidgetClient(Widget* widget)
{
    ChromeClientImpl* chromeClientImpl = toChromeClientImpl(widget);
    if (!chromeClientImpl || !chromeClientImpl->webView())
        return 0;
    return chromeClientImpl->webView()->client();
}

static WebCookieJar* getCookieJar(const Document* document)
{
    WebFrameImpl* frameImpl = WebFrameImpl::fromFrame(document->frame());
    if (!frameImpl || !frameImpl->client())
        return 0;
    WebCookieJar* cookieJar = frameImpl->client()->cookieJar();
    if (!cookieJar)
        cookieJar = webKitClient()->cookieJar();
    return cookieJar;
}

// Cache ----------------------------------------------------------------------

void ChromiumBridge::cacheMetadata(const KURL& url, double responseTime, const Vector<char>& data)
{
    webKitClient()->cacheMetadata(url, responseTime, data.data(), data.size());
}

// Clipboard ------------------------------------------------------------------

bool ChromiumBridge::clipboardIsFormatAvailable(
    PasteboardPrivate::ClipboardFormat format,
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitClient()->clipboard()->isFormatAvailable(
        static_cast<WebClipboard::Format>(format),
        static_cast<WebClipboard::Buffer>(buffer));
}

String ChromiumBridge::clipboardReadPlainText(
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitClient()->clipboard()->readPlainText(
        static_cast<WebClipboard::Buffer>(buffer));
}

void ChromiumBridge::clipboardReadHTML(
    PasteboardPrivate::ClipboardBuffer buffer,
    String* htmlText, KURL* sourceURL)
{
    WebURL url;
    *htmlText = webKitClient()->clipboard()->readHTML(
        static_cast<WebClipboard::Buffer>(buffer), &url);
    *sourceURL = url;
}

void ChromiumBridge::clipboardWriteSelection(const String& htmlText,
                                             const KURL& sourceURL,
                                             const String& plainText,
                                             bool writeSmartPaste)
{
    webKitClient()->clipboard()->writeHTML(
        htmlText, sourceURL, plainText, writeSmartPaste);
}

void ChromiumBridge::clipboardWritePlainText(const String& plainText)
{
    webKitClient()->clipboard()->writePlainText(plainText);
}

void ChromiumBridge::clipboardWriteURL(const KURL& url, const String& title)
{
    webKitClient()->clipboard()->writeURL(url, title);
}

void ChromiumBridge::clipboardWriteImage(NativeImagePtr image,
                                         const KURL& sourceURL,
                                         const String& title)
{
#if WEBKIT_USING_SKIA
    WebImage webImage(*image);
#else
    WebImage webImage(image);
#endif
    webKitClient()->clipboard()->writeImage(webImage, sourceURL, title);
}

void ChromiumBridge::clipboardWriteData(const String& type,
                                        const String& data,
                                        const String& metadata)
{
    webKitClient()->clipboard()->writeData(type, data, metadata);
}

HashSet<String> ChromiumBridge::clipboardReadAvailableTypes(
    PasteboardPrivate::ClipboardBuffer buffer, bool* containsFilenames)
{
    WebVector<WebString> result = webKitClient()->clipboard()->readAvailableTypes(
        static_cast<WebClipboard::Buffer>(buffer), containsFilenames);
    HashSet<String> types;
    for (size_t i = 0; i < result.size(); ++i)
        types.add(result[i]);
    return types;
}

bool ChromiumBridge::clipboardReadData(PasteboardPrivate::ClipboardBuffer buffer,
                                       const String& type, String& data, String& metadata)
{
    WebString resultData;
    WebString resultMetadata;
    bool succeeded = webKitClient()->clipboard()->readData(
        static_cast<WebClipboard::Buffer>(buffer), type, &resultData, &resultMetadata);
    if (succeeded) {
        data = resultData;
        metadata = resultMetadata;
    }
    return succeeded;
}

Vector<String> ChromiumBridge::clipboardReadFilenames(PasteboardPrivate::ClipboardBuffer buffer)
{
    WebVector<WebString> result = webKitClient()->clipboard()->readFilenames(
        static_cast<WebClipboard::Buffer>(buffer));
    Vector<String> convertedResult;
    for (size_t i = 0; i < result.size(); ++i)
        convertedResult.append(result[i]);
    return convertedResult;
}

// Cookies --------------------------------------------------------------------

void ChromiumBridge::setCookies(const Document* document, const KURL& url,
                                const String& value)
{
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        cookieJar->setCookie(url, document->firstPartyForCookies(), value);
}

String ChromiumBridge::cookies(const Document* document, const KURL& url)
{
    String result;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookies(url, document->firstPartyForCookies());
    return result;
}

String ChromiumBridge::cookieRequestHeaderFieldValue(const Document* document,
                                                     const KURL& url)
{
    String result;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookieRequestHeaderFieldValue(url, document->firstPartyForCookies());
    return result;
}

bool ChromiumBridge::rawCookies(const Document* document, const KURL& url, Vector<Cookie>& rawCookies)
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

void ChromiumBridge::deleteCookie(const Document* document, const KURL& url, const String& cookieName)
{
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        cookieJar->deleteCookie(url, cookieName);
}

bool ChromiumBridge::cookiesEnabled(const Document* document)
{
    bool result = false;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookiesEnabled(document->cookieURL(), document->firstPartyForCookies());
    return result;
}

// DNS ------------------------------------------------------------------------

void ChromiumBridge::prefetchDNS(const String& hostname)
{
    webKitClient()->prefetchHostName(hostname);
}

// File ------------------------------------------------------------------------

bool ChromiumBridge::fileExists(const String& path)
{
    return webKitClient()->fileUtilities()->fileExists(path);
}

bool ChromiumBridge::deleteFile(const String& path)
{
    return webKitClient()->fileUtilities()->deleteFile(path);
}

bool ChromiumBridge::deleteEmptyDirectory(const String& path)
{
    return webKitClient()->fileUtilities()->deleteEmptyDirectory(path);
}

bool ChromiumBridge::getFileSize(const String& path, long long& result)
{
    return webKitClient()->fileUtilities()->getFileSize(path, result);
}

bool ChromiumBridge::getFileModificationTime(const String& path, time_t& result)
{
    double modificationTime;
    if (!webKitClient()->fileUtilities()->getFileModificationTime(path, modificationTime))
        return false;
    result = static_cast<time_t>(modificationTime);
    return true;
}

String ChromiumBridge::directoryName(const String& path)
{
    return webKitClient()->fileUtilities()->directoryName(path);
}

String ChromiumBridge::pathByAppendingComponent(const String& path, const String& component)
{
    return webKitClient()->fileUtilities()->pathByAppendingComponent(path, component);
}

bool ChromiumBridge::makeAllDirectories(const String& path)
{
    return webKitClient()->fileUtilities()->makeAllDirectories(path);
}

String ChromiumBridge::getAbsolutePath(const String& path)
{
    return webKitClient()->fileUtilities()->getAbsolutePath(path);
}

bool ChromiumBridge::isDirectory(const String& path)
{
    return webKitClient()->fileUtilities()->isDirectory(path);
}

KURL ChromiumBridge::filePathToURL(const String& path)
{
    return webKitClient()->fileUtilities()->filePathToURL(path);
}

PlatformFileHandle ChromiumBridge::openFile(const String& path, FileOpenMode mode)
{
    return webKitClient()->fileUtilities()->openFile(path, mode);
}

void ChromiumBridge::closeFile(PlatformFileHandle& handle)
{
    webKitClient()->fileUtilities()->closeFile(handle);
}

long long ChromiumBridge::seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    return webKitClient()->fileUtilities()->seekFile(handle, offset, origin);
}

bool ChromiumBridge::truncateFile(PlatformFileHandle handle, long long offset)
{
    return webKitClient()->fileUtilities()->truncateFile(handle, offset);
}

int ChromiumBridge::readFromFile(PlatformFileHandle handle, char* data, int length)
{
    return webKitClient()->fileUtilities()->readFromFile(handle, data, length);
}

int ChromiumBridge::writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    return webKitClient()->fileUtilities()->writeToFile(handle, data, length);
}

// Font -----------------------------------------------------------------------

#if OS(WINDOWS)
bool ChromiumBridge::ensureFontLoaded(HFONT font)
{
    WebSandboxSupport* ss = webKitClient()->sandboxSupport();

    // if there is no sandbox, then we can assume the font
    // was able to be loaded successfully already
    return ss ? ss->ensureFontLoaded(font) : true;
}
#endif

#if OS(LINUX) || OS(FREEBSD)
String ChromiumBridge::getFontFamilyForCharacters(const UChar* characters, size_t numCharacters)
{
    if (webKitClient()->sandboxSupport())
        return webKitClient()->sandboxSupport()->getFontFamilyForCharacters(characters, numCharacters);

    WebCString family = WebFontInfo::familyForChars(characters, numCharacters);
    if (family.data())
        return WebString::fromUTF8(family.data());

    return WebString();
}

void ChromiumBridge::getRenderStyleForStrike(const char* font, int sizeAndStyle, FontRenderStyle* result)
{
    WebFontRenderStyle style;

    if (webKitClient()->sandboxSupport())
        webKitClient()->sandboxSupport()->getRenderStyleForStrike(font, sizeAndStyle, &style);
    else
        WebFontInfo::renderStyleForStrike(font, sizeAndStyle, &style);

    style.toFontRenderStyle(result);
}
#endif

#if OS(DARWIN)
bool ChromiumBridge::loadFont(NSFont* srcFont, ATSFontContainerRef* out)
{
    WebSandboxSupport* ss = webKitClient()->sandboxSupport();
    if (ss)
        return ss->loadFont(srcFont, out);

    // This function should only be called in response to an error loading a
    // font due to being blocked by the sandbox.
    // This by definition shouldn't happen if there is no sandbox support.
    ASSERT_NOT_REACHED();
    *out = 0;
    return false;
}
#endif

// Geolocation ----------------------------------------------------------------

GeolocationServiceBridge* ChromiumBridge::createGeolocationServiceBridge(GeolocationServiceChromium* geolocationServiceChromium)
{
    return createGeolocationServiceBridgeImpl(geolocationServiceChromium);
}

// Databases ------------------------------------------------------------------

PlatformFileHandle ChromiumBridge::databaseOpenFile(const String& vfsFileName, int desiredFlags)
{
    return webKitClient()->databaseOpenFile(WebString(vfsFileName), desiredFlags);
}

int ChromiumBridge::databaseDeleteFile(const String& vfsFileName, bool syncDir)
{
    return webKitClient()->databaseDeleteFile(WebString(vfsFileName), syncDir);
}

long ChromiumBridge::databaseGetFileAttributes(const String& vfsFileName)
{
    return webKitClient()->databaseGetFileAttributes(WebString(vfsFileName));
}

long long ChromiumBridge::databaseGetFileSize(const String& vfsFileName)
{
    return webKitClient()->databaseGetFileSize(WebString(vfsFileName));
}

// Indexed Database -----------------------------------------------------------

PassRefPtr<IDBFactoryBackendInterface> ChromiumBridge::idbFactory()
{
    // There's no reason why we need to allocate a new proxy each time, but
    // there's also no strong reason not to.
    return IDBFactoryBackendProxy::create();
}

void ChromiumBridge::createIDBKeysFromSerializedValuesAndKeyPath(const Vector<RefPtr<SerializedScriptValue> >& values, const String& keyPath, Vector<RefPtr<IDBKey> >& keys)
{
    WebVector<WebSerializedScriptValue> webValues = values;
    WebVector<WebIDBKey> webKeys;
    webKitClient()->createIDBKeysFromSerializedValuesAndKeyPath(webValues, WebString(keyPath), webKeys);

    size_t webKeysSize = webKeys.size();
    keys.reserveCapacity(webKeysSize);
    for (size_t i = 0; i < webKeysSize; ++i)
        keys.append(PassRefPtr<IDBKey>(webKeys[i]));
}

// Keygen ---------------------------------------------------------------------

String ChromiumBridge::signedPublicKeyAndChallengeString(
    unsigned keySizeIndex, const String& challenge, const KURL& url)
{
    return webKitClient()->signedPublicKeyAndChallengeString(keySizeIndex,
                                                             WebString(challenge),
                                                             WebURL(url));
}

// Language -------------------------------------------------------------------

String ChromiumBridge::computedDefaultLanguage()
{
    return webKitClient()->defaultLocale();
}

// LayoutTestMode -------------------------------------------------------------

bool ChromiumBridge::layoutTestMode()
{
    return WebKit::layoutTestMode();
}

// MimeType -------------------------------------------------------------------

bool ChromiumBridge::isSupportedImageMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsImageMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

bool ChromiumBridge::isSupportedJavaScriptMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsJavaScriptMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

bool ChromiumBridge::isSupportedNonImageMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsNonImageMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

String ChromiumBridge::mimeTypeForExtension(const String& extension)
{
    return webKitClient()->mimeRegistry()->mimeTypeForExtension(extension);
}

String ChromiumBridge::mimeTypeFromFile(const String& path)
{
    return webKitClient()->mimeRegistry()->mimeTypeFromFile(path);
}

String ChromiumBridge::preferredExtensionForMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->preferredExtensionForMIMEType(mimeType);
}

// Plugin ---------------------------------------------------------------------

bool ChromiumBridge::plugins(bool refresh, Vector<PluginInfo>* results)
{
    WebPluginListBuilderImpl builder(results);
    webKitClient()->getPluginList(refresh, &builder);
    return true;  // FIXME: There is no need for this function to return a value.
}

NPObject* ChromiumBridge::pluginScriptableObject(Widget* widget)
{
    if (!widget)
        return 0;

    ASSERT(!widget->isFrameView());

    // NOTE:  We have to trust that the widget passed to us here is a
    // WebPluginContainerImpl.  There isn't a way to dynamically verify it,
    // since the derived class (Widget) has no identifier.
    return static_cast<WebPluginContainerImpl*>(widget)->scriptableObject();
}

// Resources ------------------------------------------------------------------

PassRefPtr<Image> ChromiumBridge::loadPlatformImageResource(const char* name)
{
    const WebData& resource = webKitClient()->loadResource(name);
    if (resource.isEmpty())
        return Image::nullImage();

    RefPtr<Image> image = BitmapImage::create();
    image->setData(resource, true);
    return image;
}

// Sandbox --------------------------------------------------------------------

bool ChromiumBridge::sandboxEnabled()
{
    return webKitClient()->sandboxEnabled();
}

// SharedTimers ---------------------------------------------------------------

void ChromiumBridge::setSharedTimerFiredFunction(void (*func)())
{
    webKitClient()->setSharedTimerFiredFunction(func);
}

void ChromiumBridge::setSharedTimerFireTime(double fireTime)
{
    webKitClient()->setSharedTimerFireTime(fireTime);
}

void ChromiumBridge::stopSharedTimer()
{
    webKitClient()->stopSharedTimer();
}

// StatsCounters --------------------------------------------------------------

void ChromiumBridge::decrementStatsCounter(const char* name)
{
    webKitClient()->decrementStatsCounter(name);
}

void ChromiumBridge::incrementStatsCounter(const char* name)
{
    webKitClient()->incrementStatsCounter(name);
}

// Sudden Termination ---------------------------------------------------------

void ChromiumBridge::suddenTerminationChanged(bool enabled)
{
    webKitClient()->suddenTerminationChanged(enabled);
}

// SystemTime -----------------------------------------------------------------

double ChromiumBridge::currentTime()
{
    return webKitClient()->currentTime();
}

// Theming --------------------------------------------------------------------

#if OS(WINDOWS)

void ChromiumBridge::paintButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void ChromiumBridge::paintMenuList(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintMenuList(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void ChromiumBridge::paintScrollbarArrow(
    GraphicsContext* gc, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintScrollbarArrow(
        gc->platformContext()->canvas(), state, classicState, rect);
}

void ChromiumBridge::paintScrollbarThumb(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintScrollbarThumb(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void ChromiumBridge::paintScrollbarTrack(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const IntRect& alignRect)
{
    webKitClient()->themeEngine()->paintScrollbarTrack(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        alignRect);
}

void ChromiumBridge::paintSpinButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintSpinButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void ChromiumBridge::paintTextField(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const Color& color, bool fillContentArea,
    bool drawEdges)
{
    // Fallback to white when |color| is invalid.
    RGBA32 backgroundColor = color.isValid() ? color.rgb() : Color::white;

    webKitClient()->themeEngine()->paintTextField(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        backgroundColor, fillContentArea, drawEdges);
}

void ChromiumBridge::paintTrackbar(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintTrackbar(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void ChromiumBridge::paintProgressBar(
    GraphicsContext* gc, const IntRect& barRect, const IntRect& valueRect, bool determinate, double animatedSeconds)
{
    webKitClient()->themeEngine()->paintProgressBar(
        gc->platformContext()->canvas(), barRect, valueRect, determinate, animatedSeconds);
}

#endif

// Trace Event ----------------------------------------------------------------

void ChromiumBridge::traceEventBegin(const char* name, void* id, const char* extra)
{
    webKitClient()->traceEventBegin(name, id, extra);
}

void ChromiumBridge::traceEventEnd(const char* name, void* id, const char* extra)
{
    webKitClient()->traceEventEnd(name, id, extra);
}

// Visited Links --------------------------------------------------------------

LinkHash ChromiumBridge::visitedLinkHash(const UChar* url, unsigned length)
{
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;
    if (!url_util::Canonicalize(url, length, 0, &buffer, &parsed))
        return 0;  // Invalid URLs are unvisited.
    return webKitClient()->visitedLinkHash(buffer.data(), buffer.length());
}

LinkHash ChromiumBridge::visitedLinkHash(const KURL& base,
                                         const AtomicString& attributeURL)
{
    // Resolve the relative URL using googleurl and pass the absolute URL up to
    // the embedder. We could create a GURL object from the base and resolve
    // the relative URL that way, but calling the lower-level functions
    // directly saves us the string allocation in most cases.
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;

#if USE(GOOGLEURL)
    const CString& cstr = base.utf8String();
    const char* data = cstr.data();
    int length = cstr.length();
    const url_parse::Parsed& srcParsed = base.parsed();
#else
    // When we're not using GoogleURL, first canonicalize it so we can resolve it
    // below.
    url_canon::RawCanonOutput<2048> srcCanon;
    url_parse::Parsed srcParsed;
    String str = base.string();
    if (!url_util::Canonicalize(str.characters(), str.length(), 0, &srcCanon, &srcParsed))
        return 0;
    const char* data = srcCanon.data();
    int length = srcCanon.length();
#endif

    if (!url_util::ResolveRelative(data, length, srcParsed, attributeURL.characters(),
                                   attributeURL.length(), 0, &buffer, &parsed))
        return 0;  // Invalid resolved URL.

    return webKitClient()->visitedLinkHash(buffer.data(), buffer.length());
}

bool ChromiumBridge::isLinkVisited(LinkHash visitedLinkHash)
{
    return webKitClient()->isLinkVisited(visitedLinkHash);
}

// These are temporary methods that the WebKit layer can use to call to the
// Glue layer. Once the Glue layer moves entirely into the WebKit layer, these
// methods will be deleted.

void ChromiumBridge::notifyJSOutOfMemory(Frame* frame)
{
    if (!frame)
        return;

    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);
    if (!webFrame->client())
        return;
    webFrame->client()->didExhaustMemoryAvailableForScript(webFrame);
}

int ChromiumBridge::memoryUsageMB()
{
    return static_cast<int>(webKitClient()->memoryUsageMB());
}

int ChromiumBridge::actualMemoryUsageMB()
{
    return static_cast<int>(webKitClient()->actualMemoryUsageMB());
}

int ChromiumBridge::screenDepth(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().depth;
}

int ChromiumBridge::screenDepthPerComponent(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().depthPerComponent;
}

bool ChromiumBridge::screenIsMonochrome(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().isMonochrome;
}

IntRect ChromiumBridge::screenRect(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return IntRect();
    return client->screenInfo().rect;
}

IntRect ChromiumBridge::screenAvailableRect(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return IntRect();
    return client->screenInfo().availableRect;
}

bool ChromiumBridge::popupsAllowed(NPP npp)
{
    // FIXME: Give the embedder a way to control this.
    return false;
}

void ChromiumBridge::widgetSetCursor(Widget* widget, const Cursor& cursor)
{
    ChromeClientImpl* client = toChromeClientImpl(widget);
    if (client)
        client->setCursor(WebCursorInfo(cursor));
}

WorkerContextProxy* WorkerContextProxy::create(Worker* worker)
{
    return WebWorkerClientImpl::createWorkerContextProxy(worker);
}

} // namespace WebCore
