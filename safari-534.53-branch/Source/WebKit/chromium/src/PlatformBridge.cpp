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
#include "PlatformBridge.h"

#include <googleurl/src/url_util.h>

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "Page.h"
#include "WebAudioBus.h"
#include "WebClipboard.h"
#include "WebCookie.h"
#include "WebCookieJar.h"
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

#if USE(CG)
#include <CoreGraphics/CGContext.h>
#endif

#if OS(WINDOWS)
#include "WebRect.h"
#include "win/WebThemeEngine.h"
#endif

#if OS(DARWIN)
#include "mac/WebThemeEngine.h"
#elif OS(UNIX)
#include "linux/WebThemeEngine.h"
#include "WebFontInfo.h"
#include "WebFontRenderStyle.h"
#endif

#if WEBKIT_USING_SKIA
#include "NativeImageSkia.h"
#endif

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
        cookieJar = webKitClient()->cookieJar();
    return cookieJar;
}

// Cache ----------------------------------------------------------------------

void PlatformBridge::cacheMetadata(const KURL& url, double responseTime, const Vector<char>& data)
{
    webKitClient()->cacheMetadata(url, responseTime, data.data(), data.size());
}

// Clipboard ------------------------------------------------------------------

bool PlatformBridge::clipboardIsFormatAvailable(
    PasteboardPrivate::ClipboardFormat format,
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitClient()->clipboard()->isFormatAvailable(
        static_cast<WebClipboard::Format>(format),
        static_cast<WebClipboard::Buffer>(buffer));
}

String PlatformBridge::clipboardReadPlainText(
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitClient()->clipboard()->readPlainText(
        static_cast<WebClipboard::Buffer>(buffer));
}

void PlatformBridge::clipboardReadHTML(
    PasteboardPrivate::ClipboardBuffer buffer,
    String* htmlText, KURL* sourceURL)
{
    WebURL url;
    *htmlText = webKitClient()->clipboard()->readHTML(
        static_cast<WebClipboard::Buffer>(buffer), &url);
    *sourceURL = url;
}

PassRefPtr<SharedBuffer> PlatformBridge::clipboardReadImage(
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitClient()->clipboard()->readImage(static_cast<WebClipboard::Buffer>(buffer));
}

void PlatformBridge::clipboardWriteSelection(const String& htmlText,
                                             const KURL& sourceURL,
                                             const String& plainText,
                                             bool writeSmartPaste)
{
    webKitClient()->clipboard()->writeHTML(
        htmlText, sourceURL, plainText, writeSmartPaste);
}

void PlatformBridge::clipboardWritePlainText(const String& plainText)
{
    webKitClient()->clipboard()->writePlainText(plainText);
}

void PlatformBridge::clipboardWriteURL(const KURL& url, const String& title)
{
    webKitClient()->clipboard()->writeURL(url, title);
}

void PlatformBridge::clipboardWriteImage(NativeImagePtr image,
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

void PlatformBridge::clipboardWriteData(const String& type,
                                        const String& data,
                                        const String& metadata)
{
    webKitClient()->clipboard()->writeData(type, data, metadata);
}

HashSet<String> PlatformBridge::clipboardReadAvailableTypes(
    PasteboardPrivate::ClipboardBuffer buffer, bool* containsFilenames)
{
    WebVector<WebString> result = webKitClient()->clipboard()->readAvailableTypes(
        static_cast<WebClipboard::Buffer>(buffer), containsFilenames);
    HashSet<String> types;
    for (size_t i = 0; i < result.size(); ++i)
        types.add(result[i]);
    return types;
}

bool PlatformBridge::clipboardReadData(PasteboardPrivate::ClipboardBuffer buffer,
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

Vector<String> PlatformBridge::clipboardReadFilenames(PasteboardPrivate::ClipboardBuffer buffer)
{
    WebVector<WebString> result = webKitClient()->clipboard()->readFilenames(
        static_cast<WebClipboard::Buffer>(buffer));
    Vector<String> convertedResult;
    for (size_t i = 0; i < result.size(); ++i)
        convertedResult.append(result[i]);
    return convertedResult;
}

// Cookies --------------------------------------------------------------------

void PlatformBridge::setCookies(const Document* document, const KURL& url,
                                const String& value)
{
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        cookieJar->setCookie(url, document->firstPartyForCookies(), value);
}

String PlatformBridge::cookies(const Document* document, const KURL& url)
{
    String result;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookies(url, document->firstPartyForCookies());
    return result;
}

String PlatformBridge::cookieRequestHeaderFieldValue(const Document* document,
                                                     const KURL& url)
{
    String result;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookieRequestHeaderFieldValue(url, document->firstPartyForCookies());
    return result;
}

bool PlatformBridge::rawCookies(const Document* document, const KURL& url, Vector<Cookie>& rawCookies)
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

void PlatformBridge::deleteCookie(const Document* document, const KURL& url, const String& cookieName)
{
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        cookieJar->deleteCookie(url, cookieName);
}

bool PlatformBridge::cookiesEnabled(const Document* document)
{
    bool result = false;
    WebCookieJar* cookieJar = getCookieJar(document);
    if (cookieJar)
        result = cookieJar->cookiesEnabled(document->cookieURL(), document->firstPartyForCookies());
    return result;
}

// DNS ------------------------------------------------------------------------

void PlatformBridge::prefetchDNS(const String& hostname)
{
    webKitClient()->prefetchHostName(hostname);
}

// File ------------------------------------------------------------------------

bool PlatformBridge::fileExists(const String& path)
{
    return webKitClient()->fileUtilities()->fileExists(path);
}

bool PlatformBridge::deleteFile(const String& path)
{
    return webKitClient()->fileUtilities()->deleteFile(path);
}

bool PlatformBridge::deleteEmptyDirectory(const String& path)
{
    return webKitClient()->fileUtilities()->deleteEmptyDirectory(path);
}

bool PlatformBridge::getFileSize(const String& path, long long& result)
{
    return webKitClient()->fileUtilities()->getFileSize(path, result);
}

void PlatformBridge::revealFolderInOS(const String& path)
{
    webKitClient()->fileUtilities()->revealFolderInOS(path);
}

bool PlatformBridge::getFileModificationTime(const String& path, time_t& result)
{
    double modificationTime;
    if (!webKitClient()->fileUtilities()->getFileModificationTime(path, modificationTime))
        return false;
    result = static_cast<time_t>(modificationTime);
    return true;
}

String PlatformBridge::directoryName(const String& path)
{
    return webKitClient()->fileUtilities()->directoryName(path);
}

String PlatformBridge::pathByAppendingComponent(const String& path, const String& component)
{
    return webKitClient()->fileUtilities()->pathByAppendingComponent(path, component);
}

bool PlatformBridge::makeAllDirectories(const String& path)
{
    return webKitClient()->fileUtilities()->makeAllDirectories(path);
}

String PlatformBridge::getAbsolutePath(const String& path)
{
    return webKitClient()->fileUtilities()->getAbsolutePath(path);
}

bool PlatformBridge::isDirectory(const String& path)
{
    return webKitClient()->fileUtilities()->isDirectory(path);
}

KURL PlatformBridge::filePathToURL(const String& path)
{
    return webKitClient()->fileUtilities()->filePathToURL(path);
}

PlatformFileHandle PlatformBridge::openFile(const String& path, FileOpenMode mode)
{
    return webKitClient()->fileUtilities()->openFile(path, mode);
}

void PlatformBridge::closeFile(PlatformFileHandle& handle)
{
    webKitClient()->fileUtilities()->closeFile(handle);
}

long long PlatformBridge::seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    return webKitClient()->fileUtilities()->seekFile(handle, offset, origin);
}

bool PlatformBridge::truncateFile(PlatformFileHandle handle, long long offset)
{
    return webKitClient()->fileUtilities()->truncateFile(handle, offset);
}

int PlatformBridge::readFromFile(PlatformFileHandle handle, char* data, int length)
{
    return webKitClient()->fileUtilities()->readFromFile(handle, data, length);
}

int PlatformBridge::writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    return webKitClient()->fileUtilities()->writeToFile(handle, data, length);
}

// Font -----------------------------------------------------------------------

#if OS(WINDOWS)
bool PlatformBridge::ensureFontLoaded(HFONT font)
{
    WebSandboxSupport* ss = webKitClient()->sandboxSupport();

    // if there is no sandbox, then we can assume the font
    // was able to be loaded successfully already
    return ss ? ss->ensureFontLoaded(font) : true;
}
#endif

#if OS(DARWIN)
bool PlatformBridge::loadFont(NSFont* srcFont, ATSFontContainerRef* out)
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
#elif OS(UNIX)
String PlatformBridge::getFontFamilyForCharacters(const UChar* characters, size_t numCharacters, const char* preferredLocale)
{
    if (webKitClient()->sandboxSupport())
        return webKitClient()->sandboxSupport()->getFontFamilyForCharacters(characters, numCharacters, preferredLocale);

    WebCString family = WebFontInfo::familyForChars(characters, numCharacters, preferredLocale);
    if (family.data())
        return WebString::fromUTF8(family.data());

    return WebString();
}

void PlatformBridge::getRenderStyleForStrike(const char* font, int sizeAndStyle, FontRenderStyle* result)
{
    WebFontRenderStyle style;

    if (webKitClient()->sandboxSupport())
        webKitClient()->sandboxSupport()->getRenderStyleForStrike(font, sizeAndStyle, &style);
    else
        WebFontInfo::renderStyleForStrike(font, sizeAndStyle, &style);

    style.toFontRenderStyle(result);
}
#endif

// Databases ------------------------------------------------------------------

PlatformFileHandle PlatformBridge::databaseOpenFile(const String& vfsFileName, int desiredFlags)
{
    return webKitClient()->databaseOpenFile(WebString(vfsFileName), desiredFlags);
}

int PlatformBridge::databaseDeleteFile(const String& vfsFileName, bool syncDir)
{
    return webKitClient()->databaseDeleteFile(WebString(vfsFileName), syncDir);
}

long PlatformBridge::databaseGetFileAttributes(const String& vfsFileName)
{
    return webKitClient()->databaseGetFileAttributes(WebString(vfsFileName));
}

long long PlatformBridge::databaseGetFileSize(const String& vfsFileName)
{
    return webKitClient()->databaseGetFileSize(WebString(vfsFileName));
}

// Indexed Database -----------------------------------------------------------

PassRefPtr<IDBFactoryBackendInterface> PlatformBridge::idbFactory()
{
    // There's no reason why we need to allocate a new proxy each time, but
    // there's also no strong reason not to.
    return IDBFactoryBackendProxy::create();
}

void PlatformBridge::createIDBKeysFromSerializedValuesAndKeyPath(const Vector<RefPtr<SerializedScriptValue> >& values, const String& keyPath, Vector<RefPtr<IDBKey> >& keys)
{
    WebVector<WebSerializedScriptValue> webValues = values;
    WebVector<WebIDBKey> webKeys;
    webKitClient()->createIDBKeysFromSerializedValuesAndKeyPath(webValues, keyPath, webKeys);

    size_t webKeysSize = webKeys.size();
    keys.reserveCapacity(webKeysSize);
    for (size_t i = 0; i < webKeysSize; ++i)
        keys.append(PassRefPtr<IDBKey>(webKeys[i]));
}

PassRefPtr<SerializedScriptValue> PlatformBridge::injectIDBKeyIntoSerializedValue(PassRefPtr<IDBKey> key, PassRefPtr<SerializedScriptValue> value, const String& keyPath)
{
    return webKitClient()->injectIDBKeyIntoSerializedValue(key, value, keyPath);
}

// Keygen ---------------------------------------------------------------------

String PlatformBridge::signedPublicKeyAndChallengeString(
    unsigned keySizeIndex, const String& challenge, const KURL& url)
{
    return webKitClient()->signedPublicKeyAndChallengeString(keySizeIndex,
                                                             WebString(challenge),
                                                             WebURL(url));
}

// Language -------------------------------------------------------------------

String PlatformBridge::computedDefaultLanguage()
{
    return webKitClient()->defaultLocale();
}

// LayoutTestMode -------------------------------------------------------------

bool PlatformBridge::layoutTestMode()
{
    return WebKit::layoutTestMode();
}

// MimeType -------------------------------------------------------------------

bool PlatformBridge::isSupportedImageMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsImageMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

bool PlatformBridge::isSupportedJavaScriptMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsJavaScriptMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

bool PlatformBridge::isSupportedNonImageMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->supportsNonImageMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

String PlatformBridge::mimeTypeForExtension(const String& extension)
{
    return webKitClient()->mimeRegistry()->mimeTypeForExtension(extension);
}

String PlatformBridge::mimeTypeFromFile(const String& path)
{
    return webKitClient()->mimeRegistry()->mimeTypeFromFile(path);
}

String PlatformBridge::preferredExtensionForMIMEType(const String& mimeType)
{
    return webKitClient()->mimeRegistry()->preferredExtensionForMIMEType(mimeType);
}

// Plugin ---------------------------------------------------------------------

bool PlatformBridge::plugins(bool refresh, Vector<PluginInfo>* results)
{
    WebPluginListBuilderImpl builder(results);
    webKitClient()->getPluginList(refresh, &builder);
    return true;  // FIXME: There is no need for this function to return a value.
}

NPObject* PlatformBridge::pluginScriptableObject(Widget* widget)
{
    if (!widget || !widget->isPluginContainer())
        return 0;

    return static_cast<WebPluginContainerImpl*>(widget)->scriptableObject();
}

// Resources ------------------------------------------------------------------

PassRefPtr<Image> PlatformBridge::loadPlatformImageResource(const char* name)
{
    const WebData& resource = webKitClient()->loadResource(name);
    if (resource.isEmpty())
        return Image::nullImage();

    RefPtr<Image> image = BitmapImage::create();
    image->setData(resource, true);
    return image;
}

#if ENABLE(WEB_AUDIO)

PassOwnPtr<AudioBus> PlatformBridge::loadPlatformAudioResource(const char* name, double sampleRate)
{
    const WebData& resource = webKitClient()->loadResource(name);
    if (resource.isEmpty())
        return nullptr;
    
    return decodeAudioFileData(resource.data(), resource.size(), sampleRate);
}

PassOwnPtr<AudioBus> PlatformBridge::decodeAudioFileData(const char* data, size_t size, double sampleRate)
{
    WebAudioBus webAudioBus;
    if (webKitClient()->loadAudioResource(&webAudioBus, data, size, sampleRate))
        return webAudioBus.release();
    return nullptr;
}

#endif // ENABLE(WEB_AUDIO)

// Sandbox --------------------------------------------------------------------

bool PlatformBridge::sandboxEnabled()
{
    return webKitClient()->sandboxEnabled();
}

// SharedTimers ---------------------------------------------------------------

void PlatformBridge::setSharedTimerFiredFunction(void (*func)())
{
    webKitClient()->setSharedTimerFiredFunction(func);
}

void PlatformBridge::setSharedTimerFireTime(double fireTime)
{
    webKitClient()->setSharedTimerFireTime(fireTime);
}

void PlatformBridge::stopSharedTimer()
{
    webKitClient()->stopSharedTimer();
}

// StatsCounters --------------------------------------------------------------

void PlatformBridge::decrementStatsCounter(const char* name)
{
    webKitClient()->decrementStatsCounter(name);
}

void PlatformBridge::incrementStatsCounter(const char* name)
{
    webKitClient()->incrementStatsCounter(name);
}

void PlatformBridge::histogramCustomCounts(const char* name, int sample, int min, int max, int bucketCount)
{
    webKitClient()->histogramCustomCounts(name, sample, min, max, bucketCount);
}

void PlatformBridge::histogramEnumeration(const char* name, int sample, int boundaryValue)
{
    webKitClient()->histogramEnumeration(name, sample, boundaryValue);
}

// Sudden Termination ---------------------------------------------------------

void PlatformBridge::suddenTerminationChanged(bool enabled)
{
    webKitClient()->suddenTerminationChanged(enabled);
}

// SystemTime -----------------------------------------------------------------

double PlatformBridge::currentTime()
{
    return webKitClient()->currentTime();
}

// Theming --------------------------------------------------------------------

#if OS(WINDOWS)

void PlatformBridge::paintButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformBridge::paintMenuList(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintMenuList(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformBridge::paintScrollbarArrow(
    GraphicsContext* gc, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintScrollbarArrow(
        gc->platformContext()->canvas(), state, classicState, rect);
}

void PlatformBridge::paintScrollbarThumb(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintScrollbarThumb(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformBridge::paintScrollbarTrack(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const IntRect& alignRect)
{
    webKitClient()->themeEngine()->paintScrollbarTrack(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        alignRect);
}

void PlatformBridge::paintSpinButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintSpinButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformBridge::paintTextField(
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

void PlatformBridge::paintTrackbar(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitClient()->themeEngine()->paintTrackbar(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformBridge::paintProgressBar(
    GraphicsContext* gc, const IntRect& barRect, const IntRect& valueRect, bool determinate, double animatedSeconds)
{
    webKitClient()->themeEngine()->paintProgressBar(
        gc->platformContext()->canvas(), barRect, valueRect, determinate, animatedSeconds);
}

#elif OS(DARWIN)

void PlatformBridge::paintScrollbarThumb(
    GraphicsContext* gc, ThemePaintState state, ThemePaintSize size, const IntRect& rect, const ThemePaintScrollbarInfo& scrollbarInfo)
{
    WebThemeEngine::ScrollbarInfo webThemeScrollbarInfo;

    webThemeScrollbarInfo.orientation = static_cast<WebThemeEngine::ScrollbarOrientation>(scrollbarInfo.orientation);
    webThemeScrollbarInfo.parent = static_cast<WebThemeEngine::ScrollbarParent>(scrollbarInfo.parent);
    webThemeScrollbarInfo.maxValue = scrollbarInfo.maxValue;
    webThemeScrollbarInfo.currentValue = scrollbarInfo.currentValue;
    webThemeScrollbarInfo.visibleSize = scrollbarInfo.visibleSize;
    webThemeScrollbarInfo.totalSize = scrollbarInfo.totalSize;

    webKitClient()->themeEngine()->paintScrollbarThumb(
        gc->platformContext(),
        static_cast<WebThemeEngine::State>(state),
        static_cast<WebThemeEngine::Size>(size),
        rect,
        webThemeScrollbarInfo);
}

#elif OS(UNIX)

static WebThemeEngine::Part WebThemePart(PlatformBridge::ThemePart part)
{
    switch (part) {
    case PlatformBridge::PartScrollbarDownArrow: return WebThemeEngine::PartScrollbarDownArrow;
    case PlatformBridge::PartScrollbarLeftArrow: return WebThemeEngine::PartScrollbarLeftArrow;
    case PlatformBridge::PartScrollbarRightArrow: return WebThemeEngine::PartScrollbarRightArrow;
    case PlatformBridge::PartScrollbarUpArrow: return WebThemeEngine::PartScrollbarUpArrow;
    case PlatformBridge::PartScrollbarHorizontalThumb: return WebThemeEngine::PartScrollbarHorizontalThumb;
    case PlatformBridge::PartScrollbarVerticalThumb: return WebThemeEngine::PartScrollbarVerticalThumb;
    case PlatformBridge::PartScrollbarHorizontalTrack: return WebThemeEngine::PartScrollbarHorizontalTrack;
    case PlatformBridge::PartScrollbarVerticalTrack: return WebThemeEngine::PartScrollbarVerticalTrack;
    case PlatformBridge::PartCheckbox: return WebThemeEngine::PartCheckbox;
    case PlatformBridge::PartRadio: return WebThemeEngine::PartRadio;
    case PlatformBridge::PartButton: return WebThemeEngine::PartButton;
    case PlatformBridge::PartTextField: return WebThemeEngine::PartTextField;
    case PlatformBridge::PartMenuList: return WebThemeEngine::PartMenuList;
    case PlatformBridge::PartSliderTrack: return WebThemeEngine::PartSliderTrack;
    case PlatformBridge::PartSliderThumb: return WebThemeEngine::PartSliderThumb;
    case PlatformBridge::PartInnerSpinButton: return WebThemeEngine::PartInnerSpinButton;
    case PlatformBridge::PartProgressBar: return WebThemeEngine::PartProgressBar;
    }
    ASSERT_NOT_REACHED();
    return WebThemeEngine::PartScrollbarDownArrow;
}

static WebThemeEngine::State WebThemeState(PlatformBridge::ThemePaintState state)
{
    switch (state) {
    case PlatformBridge::StateDisabled: return WebThemeEngine::StateDisabled;
    case PlatformBridge::StateHover: return WebThemeEngine::StateHover;
    case PlatformBridge::StateNormal: return WebThemeEngine::StateNormal;
    case PlatformBridge::StatePressed: return WebThemeEngine::StatePressed;
    }
    ASSERT_NOT_REACHED();
    return WebThemeEngine::StateDisabled;
}

static void GetWebThemeExtraParams(PlatformBridge::ThemePart part, PlatformBridge::ThemePaintState state, const PlatformBridge::ThemePaintExtraParams* extraParams, WebThemeEngine::ExtraParams* webThemeExtraParams)
{
    switch (part) {
    case PlatformBridge::PartScrollbarHorizontalTrack:
    case PlatformBridge::PartScrollbarVerticalTrack:
        webThemeExtraParams->scrollbarTrack.trackX = extraParams->scrollbarTrack.trackX;
        webThemeExtraParams->scrollbarTrack.trackY = extraParams->scrollbarTrack.trackY;
        webThemeExtraParams->scrollbarTrack.trackWidth = extraParams->scrollbarTrack.trackWidth;
        webThemeExtraParams->scrollbarTrack.trackHeight = extraParams->scrollbarTrack.trackHeight;
        break;
    case PlatformBridge::PartCheckbox:
        webThemeExtraParams->button.checked = extraParams->button.checked;
        webThemeExtraParams->button.indeterminate = extraParams->button.indeterminate;
        break;
    case PlatformBridge::PartRadio:
        webThemeExtraParams->button.checked = extraParams->button.checked;
        break;
    case PlatformBridge::PartButton:
        webThemeExtraParams->button.isDefault = extraParams->button.isDefault;
        webThemeExtraParams->button.hasBorder = extraParams->button.hasBorder;
        webThemeExtraParams->button.backgroundColor = extraParams->button.backgroundColor;
        break;
    case PlatformBridge::PartTextField:
        webThemeExtraParams->textField.isTextArea = extraParams->textField.isTextArea;
        webThemeExtraParams->textField.isListbox = extraParams->textField.isListbox;
        webThemeExtraParams->textField.backgroundColor = extraParams->textField.backgroundColor;
        break;
    case PlatformBridge::PartMenuList:
        webThemeExtraParams->menuList.hasBorder = extraParams->menuList.hasBorder;
        webThemeExtraParams->menuList.hasBorderRadius = extraParams->menuList.hasBorderRadius;
        webThemeExtraParams->menuList.arrowX = extraParams->menuList.arrowX;
        webThemeExtraParams->menuList.arrowY = extraParams->menuList.arrowY;
        webThemeExtraParams->menuList.backgroundColor = extraParams->menuList.backgroundColor;
        break;
    case PlatformBridge::PartSliderTrack:
    case PlatformBridge::PartSliderThumb:
        webThemeExtraParams->slider.vertical = extraParams->slider.vertical;
        webThemeExtraParams->slider.inDrag = extraParams->slider.inDrag;
        break;
    case PlatformBridge::PartInnerSpinButton:
        webThemeExtraParams->innerSpin.spinUp = extraParams->innerSpin.spinUp;
        webThemeExtraParams->innerSpin.readOnly = extraParams->innerSpin.readOnly;
        break;
    case PlatformBridge::PartProgressBar:
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

IntSize PlatformBridge::getThemePartSize(ThemePart part)
{
     return webKitClient()->themeEngine()->getSize(WebThemePart(part));
}

void PlatformBridge::paintThemePart(
    GraphicsContext* gc, ThemePart part, ThemePaintState state, const IntRect& rect, const ThemePaintExtraParams* extraParams)
{
    WebThemeEngine::ExtraParams webThemeExtraParams;
    GetWebThemeExtraParams(part, state, extraParams, &webThemeExtraParams);
    webKitClient()->themeEngine()->paint(
        gc->platformContext()->canvas(), WebThemePart(part), WebThemeState(state), rect, &webThemeExtraParams);
}

#endif

// Trace Event ----------------------------------------------------------------

void PlatformBridge::traceEventBegin(const char* name, void* id, const char* extra)
{
    webKitClient()->traceEventBegin(name, id, extra);
}

void PlatformBridge::traceEventEnd(const char* name, void* id, const char* extra)
{
    webKitClient()->traceEventEnd(name, id, extra);
}

// Visited Links --------------------------------------------------------------

LinkHash PlatformBridge::visitedLinkHash(const UChar* url, unsigned length)
{
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;
    if (!url_util::Canonicalize(url, length, 0, &buffer, &parsed))
        return 0;  // Invalid URLs are unvisited.
    return webKitClient()->visitedLinkHash(buffer.data(), buffer.length());
}

LinkHash PlatformBridge::visitedLinkHash(const KURL& base,
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

bool PlatformBridge::isLinkVisited(LinkHash visitedLinkHash)
{
    return webKitClient()->isLinkVisited(visitedLinkHash);
}

// These are temporary methods that the WebKit layer can use to call to the
// Glue layer. Once the Glue layer moves entirely into the WebKit layer, these
// methods will be deleted.

void PlatformBridge::notifyJSOutOfMemory(Frame* frame)
{
    if (!frame)
        return;

    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);
    if (!webFrame->client())
        return;
    webFrame->client()->didExhaustMemoryAvailableForScript(webFrame);
}

int PlatformBridge::memoryUsageMB()
{
    return static_cast<int>(webKitClient()->memoryUsageMB());
}

int PlatformBridge::actualMemoryUsageMB()
{
    return static_cast<int>(webKitClient()->actualMemoryUsageMB());
}

int PlatformBridge::screenDepth(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().depth;
}

int PlatformBridge::screenDepthPerComponent(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().depthPerComponent;
}

bool PlatformBridge::screenIsMonochrome(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().isMonochrome;
}

IntRect PlatformBridge::screenRect(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return IntRect();
    return client->screenInfo().rect;
}

IntRect PlatformBridge::screenAvailableRect(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return IntRect();
    return client->screenInfo().availableRect;
}

bool PlatformBridge::popupsAllowed(NPP npp)
{
    // FIXME: Give the embedder a way to control this.
    return false;
}

WorkerContextProxy* WorkerContextProxy::create(Worker* worker)
{
    return WebWorkerClientImpl::createWorkerContextProxy(worker);
}

} // namespace WebCore
