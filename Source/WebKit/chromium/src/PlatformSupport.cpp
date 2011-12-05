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

#include <googleurl/src/url_util.h>

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "Page.h"
#include "WebFileUtilities.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebIDBKey.h"
#include "WebKit.h"
#include "WebPluginContainerImpl.h"
#include "WebPluginListBuilderImpl.h"
#include "WebSandboxSupport.h"
#include "WebScreenInfo.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "WebWorkerClientImpl.h"
#include "WebWorkerRunLoop.h"
#include "platform/WebAudioBus.h"
#include "platform/WebClipboard.h"
#include "platform/WebCookie.h"
#include "platform/WebCookieJar.h"
#include "platform/WebData.h"
#include "platform/WebDragData.h"
#include "platform/WebImage.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebMimeRegistry.h"
#include "platform/WebSerializedScriptValue.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "platform/WebVector.h"

#if USE(CG)
#include <CoreGraphics/CGContext.h>
#endif

#if OS(WINDOWS)
#include "platform/WebRect.h"
#include "platform/win/WebThemeEngine.h"
#endif

#if OS(DARWIN)
#include "platform/mac/WebThemeEngine.h"
#elif OS(UNIX) && !OS(ANDROID)
#include "platform/linux/WebThemeEngine.h"
#include "WebFontInfo.h"
#include "WebFontRenderStyle.h"
#elif OS(ANDROID)
#include "platform/android/WebThemeEngine.h"
#endif

#if WEBKIT_USING_SKIA
#include "NativeImageSkia.h"
#endif

#include "BitmapImage.h"
#include "ClipboardChromium.h"
#include "Cookie.h"
#include "Document.h"
#include "FrameView.h"
#include "GamepadList.h"
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
        cookieJar = webKitPlatformSupport()->cookieJar();
    return cookieJar;
}

// Cache ----------------------------------------------------------------------

void PlatformSupport::cacheMetadata(const KURL& url, double responseTime, const Vector<char>& data)
{
    webKitPlatformSupport()->cacheMetadata(url, responseTime, data.data(), data.size());
}

// Clipboard ------------------------------------------------------------------

uint64_t PlatformSupport::clipboardSequenceNumber(PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitPlatformSupport()->clipboard()->sequenceNumber(
        static_cast<WebClipboard::Buffer>(buffer));
}

bool PlatformSupport::clipboardIsFormatAvailable(
    PasteboardPrivate::ClipboardFormat format,
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitPlatformSupport()->clipboard()->isFormatAvailable(
        static_cast<WebClipboard::Format>(format),
        static_cast<WebClipboard::Buffer>(buffer));
}

HashSet<String> PlatformSupport::clipboardReadAvailableTypes(
    PasteboardPrivate::ClipboardBuffer buffer, bool* containsFilenames)
{
    WebVector<WebString> result = webKitPlatformSupport()->clipboard()->readAvailableTypes(
        static_cast<WebClipboard::Buffer>(buffer), containsFilenames);
    HashSet<String> types;
    for (size_t i = 0; i < result.size(); ++i)
        types.add(result[i]);
    return types;
}

String PlatformSupport::clipboardReadPlainText(
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitPlatformSupport()->clipboard()->readPlainText(
        static_cast<WebClipboard::Buffer>(buffer));
}

void PlatformSupport::clipboardReadHTML(
    PasteboardPrivate::ClipboardBuffer buffer,
    String* htmlText, KURL* sourceURL, unsigned* fragmentStart, unsigned* fragmentEnd)
{
    WebURL url;
    *htmlText = webKitPlatformSupport()->clipboard()->readHTML(
        static_cast<WebClipboard::Buffer>(buffer), &url, fragmentStart, fragmentEnd);
    *sourceURL = url;
}

PassRefPtr<SharedBuffer> PlatformSupport::clipboardReadImage(
    PasteboardPrivate::ClipboardBuffer buffer)
{
    return webKitPlatformSupport()->clipboard()->readImage(static_cast<WebClipboard::Buffer>(buffer));
}

String PlatformSupport::clipboardReadCustomData(
    PasteboardPrivate::ClipboardBuffer buffer, const String& type)
{
    return webKitPlatformSupport()->clipboard()->readCustomData(static_cast<WebClipboard::Buffer>(buffer), type);
}

void PlatformSupport::clipboardWriteSelection(const String& htmlText,
                                             const KURL& sourceURL,
                                             const String& plainText,
                                             bool writeSmartPaste)
{
    webKitPlatformSupport()->clipboard()->writeHTML(
        htmlText, sourceURL, plainText, writeSmartPaste);
}

void PlatformSupport::clipboardWritePlainText(const String& plainText)
{
    webKitPlatformSupport()->clipboard()->writePlainText(plainText);
}

void PlatformSupport::clipboardWriteURL(const KURL& url, const String& title)
{
    webKitPlatformSupport()->clipboard()->writeURL(url, title);
}

void PlatformSupport::clipboardWriteImage(NativeImagePtr image,
                                         const KURL& sourceURL,
                                         const String& title)
{
#if WEBKIT_USING_SKIA
    WebImage webImage(image->bitmap());
#else
    WebImage webImage(image);
#endif
    webKitPlatformSupport()->clipboard()->writeImage(webImage, sourceURL, title);
}

void PlatformSupport::clipboardWriteDataObject(Clipboard* clipboard)
{
    WebDragData data = static_cast<ClipboardChromium*>(clipboard)->dataObject();
    webKitPlatformSupport()->clipboard()->writeDataObject(data);
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

// DNS ------------------------------------------------------------------------

void PlatformSupport::prefetchDNS(const String& hostname)
{
    webKitPlatformSupport()->prefetchHostName(hostname);
}

// File ------------------------------------------------------------------------

bool PlatformSupport::fileExists(const String& path)
{
    return webKitPlatformSupport()->fileUtilities()->fileExists(path);
}

bool PlatformSupport::deleteFile(const String& path)
{
    return webKitPlatformSupport()->fileUtilities()->deleteFile(path);
}

bool PlatformSupport::deleteEmptyDirectory(const String& path)
{
    return webKitPlatformSupport()->fileUtilities()->deleteEmptyDirectory(path);
}

bool PlatformSupport::getFileSize(const String& path, long long& result)
{
    return webKitPlatformSupport()->fileUtilities()->getFileSize(path, result);
}

void PlatformSupport::revealFolderInOS(const String& path)
{
    webKitPlatformSupport()->fileUtilities()->revealFolderInOS(path);
}

bool PlatformSupport::getFileModificationTime(const String& path, time_t& result)
{
    double modificationTime;
    if (!webKitPlatformSupport()->fileUtilities()->getFileModificationTime(path, modificationTime))
        return false;
    result = static_cast<time_t>(modificationTime);
    return true;
}

String PlatformSupport::directoryName(const String& path)
{
    return webKitPlatformSupport()->fileUtilities()->directoryName(path);
}

String PlatformSupport::pathByAppendingComponent(const String& path, const String& component)
{
    return webKitPlatformSupport()->fileUtilities()->pathByAppendingComponent(path, component);
}

bool PlatformSupport::makeAllDirectories(const String& path)
{
    return webKitPlatformSupport()->fileUtilities()->makeAllDirectories(path);
}

String PlatformSupport::getAbsolutePath(const String& path)
{
    return webKitPlatformSupport()->fileUtilities()->getAbsolutePath(path);
}

bool PlatformSupport::isDirectory(const String& path)
{
    return webKitPlatformSupport()->fileUtilities()->isDirectory(path);
}

KURL PlatformSupport::filePathToURL(const String& path)
{
    return webKitPlatformSupport()->fileUtilities()->filePathToURL(path);
}

PlatformFileHandle PlatformSupport::openFile(const String& path, FileOpenMode mode)
{
    return webKitPlatformSupport()->fileUtilities()->openFile(path, mode);
}

void PlatformSupport::closeFile(PlatformFileHandle& handle)
{
    webKitPlatformSupport()->fileUtilities()->closeFile(handle);
}

long long PlatformSupport::seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    return webKitPlatformSupport()->fileUtilities()->seekFile(handle, offset, origin);
}

bool PlatformSupport::truncateFile(PlatformFileHandle handle, long long offset)
{
    return webKitPlatformSupport()->fileUtilities()->truncateFile(handle, offset);
}

int PlatformSupport::readFromFile(PlatformFileHandle handle, char* data, int length)
{
    return webKitPlatformSupport()->fileUtilities()->readFromFile(handle, data, length);
}

int PlatformSupport::writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    return webKitPlatformSupport()->fileUtilities()->writeToFile(handle, data, length);
}

// Font -----------------------------------------------------------------------

#if OS(WINDOWS)
bool PlatformSupport::ensureFontLoaded(HFONT font)
{
    WebSandboxSupport* ss = webKitPlatformSupport()->sandboxSupport();

    // if there is no sandbox, then we can assume the font
    // was able to be loaded successfully already
    return ss ? ss->ensureFontLoaded(font) : true;
}
#endif

#if OS(DARWIN)
bool PlatformSupport::loadFont(NSFont* srcFont, CGFontRef* out, uint32_t* fontID)
{
    WebSandboxSupport* ss = webKitPlatformSupport()->sandboxSupport();
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
    if (webKitPlatformSupport()->sandboxSupport())
        webKitPlatformSupport()->sandboxSupport()->getFontFamilyForCharacters(characters, numCharacters, preferredLocale, &webFamily);
    else
        WebFontInfo::familyForChars(characters, numCharacters, preferredLocale, &webFamily);
    family->name = String::fromUTF8(webFamily.name.data(), webFamily.name.length());
    family->isBold = webFamily.isBold;
    family->isItalic = webFamily.isItalic;
#endif
}

void PlatformSupport::getRenderStyleForStrike(const char* font, int sizeAndStyle, FontRenderStyle* result)
{
#if !OS(ANDROID)
    WebFontRenderStyle style;

    if (webKitPlatformSupport()->sandboxSupport())
        webKitPlatformSupport()->sandboxSupport()->getRenderStyleForStrike(font, sizeAndStyle, &style);
    else
        WebFontInfo::renderStyleForStrike(font, sizeAndStyle, &style);

    style.toFontRenderStyle(result);
#endif
}
#endif

// Databases ------------------------------------------------------------------

PlatformFileHandle PlatformSupport::databaseOpenFile(const String& vfsFileName, int desiredFlags)
{
    return webKitPlatformSupport()->databaseOpenFile(WebString(vfsFileName), desiredFlags);
}

int PlatformSupport::databaseDeleteFile(const String& vfsFileName, bool syncDir)
{
    return webKitPlatformSupport()->databaseDeleteFile(WebString(vfsFileName), syncDir);
}

long PlatformSupport::databaseGetFileAttributes(const String& vfsFileName)
{
    return webKitPlatformSupport()->databaseGetFileAttributes(WebString(vfsFileName));
}

long long PlatformSupport::databaseGetFileSize(const String& vfsFileName)
{
    return webKitPlatformSupport()->databaseGetFileSize(WebString(vfsFileName));
}

long long PlatformSupport::databaseGetSpaceAvailableForOrigin(const String& originIdentifier)
{
    return webKitPlatformSupport()->databaseGetSpaceAvailableForOrigin(originIdentifier);
}

// Indexed Database -----------------------------------------------------------

PassRefPtr<IDBFactoryBackendInterface> PlatformSupport::idbFactory()
{
    // There's no reason why we need to allocate a new proxy each time, but
    // there's also no strong reason not to.
    return IDBFactoryBackendProxy::create();
}

void PlatformSupport::createIDBKeysFromSerializedValuesAndKeyPath(const Vector<RefPtr<SerializedScriptValue> >& values, const String& keyPath, Vector<RefPtr<IDBKey> >& keys)
{
    WebVector<WebSerializedScriptValue> webValues = values;
    WebVector<WebIDBKey> webKeys;
    webKitPlatformSupport()->createIDBKeysFromSerializedValuesAndKeyPath(webValues, keyPath, webKeys);

    size_t webKeysSize = webKeys.size();
    keys.reserveCapacity(webKeysSize);
    for (size_t i = 0; i < webKeysSize; ++i)
        keys.append(PassRefPtr<IDBKey>(webKeys[i]));
}

PassRefPtr<SerializedScriptValue> PlatformSupport::injectIDBKeyIntoSerializedValue(PassRefPtr<IDBKey> key, PassRefPtr<SerializedScriptValue> value, const String& keyPath)
{
    return webKitPlatformSupport()->injectIDBKeyIntoSerializedValue(key, value, keyPath);
}

// Gamepad --------------------------------------------------------------------

void PlatformSupport::sampleGamepads(GamepadList* into)
{
    WebGamepads gamepads;

    webKitPlatformSupport()->sampleGamepads(gamepads);

    for (unsigned i = 0; i < WebKit::WebGamepads::itemsLengthCap; ++i) {
        WebGamepad& webGamepad = gamepads.items[i];
        if (i < gamepads.length && webGamepad.connected) {
            RefPtr<Gamepad> gamepad = into->item(i);
            if (!gamepad)
                gamepad = Gamepad::create();
            gamepad->id(webGamepad.id);
            gamepad->index(i);
            gamepad->timestamp(webGamepad.timestamp);
            gamepad->axes(webGamepad.axesLength, webGamepad.axes);
            gamepad->buttons(webGamepad.buttonsLength, webGamepad.buttons);
            into->set(i, gamepad);
        } else
            into->set(i, 0);
    }
}

// Keygen ---------------------------------------------------------------------

String PlatformSupport::signedPublicKeyAndChallengeString(
    unsigned keySizeIndex, const String& challenge, const KURL& url)
{
    return webKitPlatformSupport()->signedPublicKeyAndChallengeString(keySizeIndex,
                                                             WebString(challenge),
                                                             WebURL(url));
}

// Language -------------------------------------------------------------------

String PlatformSupport::computedDefaultLanguage()
{
    return webKitPlatformSupport()->defaultLocale();
}

// LayoutTestMode -------------------------------------------------------------

bool PlatformSupport::layoutTestMode()
{
    return WebKit::layoutTestMode();
}

// MimeType -------------------------------------------------------------------

bool PlatformSupport::isSupportedImageMIMEType(const String& mimeType)
{
    return webKitPlatformSupport()->mimeRegistry()->supportsImageMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

bool PlatformSupport::isSupportedJavaScriptMIMEType(const String& mimeType)
{
    return webKitPlatformSupport()->mimeRegistry()->supportsJavaScriptMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

bool PlatformSupport::isSupportedNonImageMIMEType(const String& mimeType)
{
    return webKitPlatformSupport()->mimeRegistry()->supportsNonImageMIMEType(mimeType)
        != WebMimeRegistry::IsNotSupported;
}

String PlatformSupport::mimeTypeForExtension(const String& extension)
{
    return webKitPlatformSupport()->mimeRegistry()->mimeTypeForExtension(extension);
}

String PlatformSupport::wellKnownMimeTypeForExtension(const String& extension)
{
    return webKitPlatformSupport()->mimeRegistry()->wellKnownMimeTypeForExtension(extension);
}

String PlatformSupport::mimeTypeFromFile(const String& path)
{
    return webKitPlatformSupport()->mimeRegistry()->mimeTypeFromFile(path);
}

String PlatformSupport::preferredExtensionForMIMEType(const String& mimeType)
{
    return webKitPlatformSupport()->mimeRegistry()->preferredExtensionForMIMEType(mimeType);
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

// Resources ------------------------------------------------------------------

PassRefPtr<Image> PlatformSupport::loadPlatformImageResource(const char* name)
{
    const WebData& resource = webKitPlatformSupport()->loadResource(name);
    if (resource.isEmpty())
        return Image::nullImage();

    RefPtr<Image> image = BitmapImage::create();
    image->setData(resource, true);
    return image;
}

#if ENABLE(WEB_AUDIO)

PassOwnPtr<AudioBus> PlatformSupport::loadPlatformAudioResource(const char* name, double sampleRate)
{
    const WebData& resource = webKitPlatformSupport()->loadResource(name);
    if (resource.isEmpty())
        return nullptr;
    
    return decodeAudioFileData(resource.data(), resource.size(), sampleRate);
}

PassOwnPtr<AudioBus> PlatformSupport::decodeAudioFileData(const char* data, size_t size, double sampleRate)
{
    WebAudioBus webAudioBus;
    if (webKitPlatformSupport()->loadAudioResource(&webAudioBus, data, size, sampleRate))
        return webAudioBus.release();
    return nullptr;
}

#endif // ENABLE(WEB_AUDIO)

// Sandbox --------------------------------------------------------------------

bool PlatformSupport::sandboxEnabled()
{
    return webKitPlatformSupport()->sandboxEnabled();
}

// SharedTimers ---------------------------------------------------------------

void PlatformSupport::setSharedTimerFiredFunction(void (*func)())
{
    webKitPlatformSupport()->setSharedTimerFiredFunction(func);
}

void PlatformSupport::setSharedTimerFireInterval(double interval)
{
    webKitPlatformSupport()->setSharedTimerFireInterval(interval);
}

void PlatformSupport::stopSharedTimer()
{
    webKitPlatformSupport()->stopSharedTimer();
}

// StatsCounters --------------------------------------------------------------

void PlatformSupport::decrementStatsCounter(const char* name)
{
    webKitPlatformSupport()->decrementStatsCounter(name);
}

void PlatformSupport::incrementStatsCounter(const char* name)
{
    webKitPlatformSupport()->incrementStatsCounter(name);
}

void PlatformSupport::histogramCustomCounts(const char* name, int sample, int min, int max, int bucketCount)
{
    webKitPlatformSupport()->histogramCustomCounts(name, sample, min, max, bucketCount);
}

void PlatformSupport::histogramEnumeration(const char* name, int sample, int boundaryValue)
{
    webKitPlatformSupport()->histogramEnumeration(name, sample, boundaryValue);
}

// Sudden Termination ---------------------------------------------------------

void PlatformSupport::suddenTerminationChanged(bool enabled)
{
    webKitPlatformSupport()->suddenTerminationChanged(enabled);
}

// Theming --------------------------------------------------------------------

#if OS(WINDOWS)

void PlatformSupport::paintButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitPlatformSupport()->themeEngine()->paintButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintMenuList(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitPlatformSupport()->themeEngine()->paintMenuList(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintScrollbarArrow(
    GraphicsContext* gc, int state, int classicState,
    const IntRect& rect)
{
    webKitPlatformSupport()->themeEngine()->paintScrollbarArrow(
        gc->platformContext()->canvas(), state, classicState, rect);
}

void PlatformSupport::paintScrollbarThumb(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitPlatformSupport()->themeEngine()->paintScrollbarThumb(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintScrollbarTrack(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const IntRect& alignRect)
{
    webKitPlatformSupport()->themeEngine()->paintScrollbarTrack(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        alignRect);
}

void PlatformSupport::paintSpinButton(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitPlatformSupport()->themeEngine()->paintSpinButton(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintTextField(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect, const Color& color, bool fillContentArea,
    bool drawEdges)
{
    // Fallback to white when |color| is invalid.
    RGBA32 backgroundColor = color.isValid() ? color.rgb() : Color::white;

    webKitPlatformSupport()->themeEngine()->paintTextField(
        gc->platformContext()->canvas(), part, state, classicState, rect,
        backgroundColor, fillContentArea, drawEdges);
}

void PlatformSupport::paintTrackbar(
    GraphicsContext* gc, int part, int state, int classicState,
    const IntRect& rect)
{
    webKitPlatformSupport()->themeEngine()->paintTrackbar(
        gc->platformContext()->canvas(), part, state, classicState, rect);
}

void PlatformSupport::paintProgressBar(
    GraphicsContext* gc, const IntRect& barRect, const IntRect& valueRect, bool determinate, double animatedSeconds)
{
    webKitPlatformSupport()->themeEngine()->paintProgressBar(
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

#if WEBKIT_USING_SKIA
    WebKit::WebCanvas* webCanvas = gc->platformContext()->canvas();
#else
    WebKit::WebCanvas* webCanvas = gc->platformContext();
#endif
    webKitPlatformSupport()->themeEngine()->paintScrollbarThumb(
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
     return webKitPlatformSupport()->themeEngine()->getSize(WebThemePart(part));
}

void PlatformSupport::paintThemePart(
    GraphicsContext* gc, ThemePart part, ThemePaintState state, const IntRect& rect, const ThemePaintExtraParams* extraParams)
{
    WebThemeEngine::ExtraParams webThemeExtraParams;
    GetWebThemeExtraParams(part, state, extraParams, &webThemeExtraParams);
    webKitPlatformSupport()->themeEngine()->paint(
        gc->platformContext()->canvas(), WebThemePart(part), WebThemeState(state), rect, &webThemeExtraParams);
}

#endif

// Trace Event ----------------------------------------------------------------

bool PlatformSupport::isTraceEventEnabled()
{
    return webKitPlatformSupport()->isTraceEventEnabled();
}

void PlatformSupport::traceEventBegin(const char* name, void* id, const char* extra)
{
    webKitPlatformSupport()->traceEventBegin(name, id, extra);
}

void PlatformSupport::traceEventEnd(const char* name, void* id, const char* extra)
{
    webKitPlatformSupport()->traceEventEnd(name, id, extra);
}

// Visited Links --------------------------------------------------------------

LinkHash PlatformSupport::visitedLinkHash(const UChar* url, unsigned length)
{
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;
    if (!url_util::Canonicalize(url, length, 0, &buffer, &parsed))
        return 0;  // Invalid URLs are unvisited.
    return webKitPlatformSupport()->visitedLinkHash(buffer.data(), buffer.length());
}

LinkHash PlatformSupport::visitedLinkHash(const KURL& base,
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

    return webKitPlatformSupport()->visitedLinkHash(buffer.data(), buffer.length());
}

bool PlatformSupport::isLinkVisited(LinkHash visitedLinkHash)
{
    return webKitPlatformSupport()->isLinkVisited(visitedLinkHash);
}

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

int PlatformSupport::memoryUsageMB()
{
    return static_cast<int>(webKitPlatformSupport()->memoryUsageMB());
}

int PlatformSupport::actualMemoryUsageMB()
{
    return static_cast<int>(webKitPlatformSupport()->actualMemoryUsageMB());
}

int PlatformSupport::lowMemoryUsageMB()
{
    return static_cast<int>(webKitPlatformSupport()->lowMemoryUsageMB());
}

int PlatformSupport::highMemoryUsageMB()
{
    return static_cast<int>(webKitPlatformSupport()->highMemoryUsageMB());
}

int PlatformSupport::highUsageDeltaMB()
{
    return static_cast<int>(webKitPlatformSupport()->highUsageDeltaMB());
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

double PlatformSupport::screenRefreshRate(Widget* widget)
{
    WebWidgetClient* client = toWebWidgetClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().refreshRate;
}

bool PlatformSupport::popupsAllowed(NPP npp)
{
    // FIXME: Give the embedder a way to control this.
    return false;
}

void PlatformSupport::didStartWorkerRunLoop(WorkerRunLoop* loop)
{
    webKitPlatformSupport()->didStartWorkerRunLoop(WebWorkerRunLoop(loop));
}

void PlatformSupport::didStopWorkerRunLoop(WorkerRunLoop* loop)
{
    webKitPlatformSupport()->didStopWorkerRunLoop(WebWorkerRunLoop(loop));
}

#if ENABLE(WORKERS)
WorkerContextProxy* WorkerContextProxy::create(Worker* worker)
{
    return WebWorkerClientImpl::createWorkerContextProxy(worker);
}
#endif

} // namespace WebCore
