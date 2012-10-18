/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreArgumentCoders.h"

#include "ShareableBitmap.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/Credential.h>
#include <WebCore/Cursor.h>
#include <WebCore/DatabaseDetails.h>
#include <WebCore/DictationAlternative.h>
#include <WebCore/DragSession.h>
#include <WebCore/Editor.h>
#include <WebCore/FileChooser.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/Image.h>
#include <WebCore/KURL.h>
#include <WebCore/PluginData.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/text/StringHash.h>

using namespace WebCore;
using namespace WebKit;

namespace CoreIPC {

void ArgumentCoder<AffineTransform>::encode(ArgumentEncoder* encoder, const AffineTransform& affineTransform)
{
    SimpleArgumentCoder<AffineTransform>::encode(encoder, affineTransform);
}

bool ArgumentCoder<AffineTransform>::decode(ArgumentDecoder* decoder, AffineTransform& affineTransform)
{
    return SimpleArgumentCoder<AffineTransform>::decode(decoder, affineTransform);
}


void ArgumentCoder<FloatPoint>::encode(ArgumentEncoder* encoder, const FloatPoint& floatPoint)
{
    SimpleArgumentCoder<FloatPoint>::encode(encoder, floatPoint);
}

bool ArgumentCoder<FloatPoint>::decode(ArgumentDecoder* decoder, FloatPoint& floatPoint)
{
    return SimpleArgumentCoder<FloatPoint>::decode(decoder, floatPoint);
}


void ArgumentCoder<FloatRect>::encode(ArgumentEncoder* encoder, const FloatRect& floatRect)
{
    SimpleArgumentCoder<FloatRect>::encode(encoder, floatRect);
}

bool ArgumentCoder<FloatRect>::decode(ArgumentDecoder* decoder, FloatRect& floatRect)
{
    return SimpleArgumentCoder<FloatRect>::decode(decoder, floatRect);
}


void ArgumentCoder<FloatSize>::encode(ArgumentEncoder* encoder, const FloatSize& floatSize)
{
    SimpleArgumentCoder<FloatSize>::encode(encoder, floatSize);
}

bool ArgumentCoder<FloatSize>::decode(ArgumentDecoder* decoder, FloatSize& floatSize)
{
    return SimpleArgumentCoder<FloatSize>::decode(decoder, floatSize);
}


void ArgumentCoder<IntPoint>::encode(ArgumentEncoder* encoder, const IntPoint& intPoint)
{
    SimpleArgumentCoder<IntPoint>::encode(encoder, intPoint);
}

bool ArgumentCoder<IntPoint>::decode(ArgumentDecoder* decoder, IntPoint& intPoint)
{
    return SimpleArgumentCoder<IntPoint>::decode(decoder, intPoint);
}


void ArgumentCoder<IntRect>::encode(ArgumentEncoder* encoder, const IntRect& intRect)
{
    SimpleArgumentCoder<IntRect>::encode(encoder, intRect);
}

bool ArgumentCoder<IntRect>::decode(ArgumentDecoder* decoder, IntRect& intRect)
{
    return SimpleArgumentCoder<IntRect>::decode(decoder, intRect);
}


void ArgumentCoder<IntSize>::encode(ArgumentEncoder* encoder, const IntSize& intSize)
{
    SimpleArgumentCoder<IntSize>::encode(encoder, intSize);
}

bool ArgumentCoder<IntSize>::decode(ArgumentDecoder* decoder, IntSize& intSize)
{
    return SimpleArgumentCoder<IntSize>::decode(decoder, intSize);
}

void ArgumentCoder<ViewportAttributes>::encode(ArgumentEncoder* encoder, const ViewportAttributes& viewportAttributes)
{
    SimpleArgumentCoder<ViewportAttributes>::encode(encoder, viewportAttributes);
}

bool ArgumentCoder<ViewportAttributes>::decode(ArgumentDecoder* decoder, ViewportAttributes& viewportAttributes)
{
    return SimpleArgumentCoder<ViewportAttributes>::decode(decoder, viewportAttributes);
}

void ArgumentCoder<MimeClassInfo>::encode(ArgumentEncoder* encoder, const MimeClassInfo& mimeClassInfo)
{
    encoder->encode(mimeClassInfo.type);
    encoder->encode(mimeClassInfo.desc);
    encoder->encode(mimeClassInfo.extensions);
}

bool ArgumentCoder<MimeClassInfo>::decode(ArgumentDecoder* decoder, MimeClassInfo& mimeClassInfo)
{
    if (!decoder->decode(mimeClassInfo.type))
        return false;
    if (!decoder->decode(mimeClassInfo.desc))
        return false;
    if (!decoder->decode(mimeClassInfo.extensions))
        return false;

    return true;
}


void ArgumentCoder<PluginInfo>::encode(ArgumentEncoder* encoder, const PluginInfo& pluginInfo)
{
    encoder->encode(pluginInfo.name);
    encoder->encode(pluginInfo.file);
    encoder->encode(pluginInfo.desc);
    encoder->encode(pluginInfo.mimes);
}
    
bool ArgumentCoder<PluginInfo>::decode(ArgumentDecoder* decoder, PluginInfo& pluginInfo)
{
    if (!decoder->decode(pluginInfo.name))
        return false;
    if (!decoder->decode(pluginInfo.file))
        return false;
    if (!decoder->decode(pluginInfo.desc))
        return false;
    if (!decoder->decode(pluginInfo.mimes))
        return false;

    return true;
}


void ArgumentCoder<HTTPHeaderMap>::encode(ArgumentEncoder* encoder, const HTTPHeaderMap& headerMap)
{
    encoder->encode(static_cast<const HashMap<AtomicString, String, CaseFoldingHash>&>(headerMap));
}

bool ArgumentCoder<HTTPHeaderMap>::decode(ArgumentDecoder* decoder, HTTPHeaderMap& headerMap)
{
    return decoder->decode(static_cast<HashMap<AtomicString, String, CaseFoldingHash>&>(headerMap));
}


void ArgumentCoder<AuthenticationChallenge>::encode(ArgumentEncoder* encoder, const AuthenticationChallenge& challenge)
{
    encoder->encode(challenge.protectionSpace());
    encoder->encode(challenge.proposedCredential());
    encoder->encode(challenge.previousFailureCount());
    encoder->encode(challenge.failureResponse());
    encoder->encode(challenge.error());
}

bool ArgumentCoder<AuthenticationChallenge>::decode(ArgumentDecoder* decoder, AuthenticationChallenge& challenge)
{    
    ProtectionSpace protectionSpace;
    if (!decoder->decode(protectionSpace))
        return false;

    Credential proposedCredential;
    if (!decoder->decode(proposedCredential))
        return false;

    unsigned previousFailureCount;    
    if (!decoder->decode(previousFailureCount))
        return false;

    ResourceResponse failureResponse;
    if (!decoder->decode(failureResponse))
        return false;

    ResourceError error;
    if (!decoder->decode(error))
        return false;
    
    challenge = AuthenticationChallenge(protectionSpace, proposedCredential, previousFailureCount, failureResponse, error);
    return true;
}


void ArgumentCoder<ProtectionSpace>::encode(ArgumentEncoder* encoder, const ProtectionSpace& space)
{
    encoder->encode(space.host());
    encoder->encode(space.port());
    encoder->encodeEnum(space.serverType());
    encoder->encode(space.realm());
    encoder->encodeEnum(space.authenticationScheme());
}

bool ArgumentCoder<ProtectionSpace>::decode(ArgumentDecoder* decoder, ProtectionSpace& space)
{
    String host;
    if (!decoder->decode(host))
        return false;

    int port;
    if (!decoder->decode(port))
        return false;

    ProtectionSpaceServerType serverType;
    if (!decoder->decodeEnum(serverType))
        return false;

    String realm;
    if (!decoder->decode(realm))
        return false;
    
    ProtectionSpaceAuthenticationScheme authenticationScheme;
    if (!decoder->decodeEnum(authenticationScheme))
        return false;

    space = ProtectionSpace(host, port, serverType, realm, authenticationScheme);
    return true;
}

void ArgumentCoder<Credential>::encode(ArgumentEncoder* encoder, const Credential& credential)
{
    encoder->encode(credential.user());
    encoder->encode(credential.password());
    encoder->encodeEnum(credential.persistence());
}

bool ArgumentCoder<Credential>::decode(ArgumentDecoder* decoder, Credential& credential)
{
    String user;
    if (!decoder->decode(user))
        return false;

    String password;
    if (!decoder->decode(password))
        return false;

    CredentialPersistence persistence;
    if (!decoder->decodeEnum(persistence))
        return false;
    
    credential = Credential(user, password, persistence);
    return true;
}

static void encodeImage(ArgumentEncoder* encoder, Image* image)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(image->size(), ShareableBitmap::SupportsAlpha);
    bitmap->createGraphicsContext()->drawImage(image, ColorSpaceDeviceRGB, IntPoint());

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);

    encoder->encode(handle);
}

static bool decodeImage(ArgumentDecoder* decoder, RefPtr<Image>& image)
{
    ShareableBitmap::Handle handle;
    if (!decoder->decode(handle))
        return false;
    
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle);
    if (!bitmap)
        return false;
    image = bitmap->createImage();
    if (!image)
        return false;
    return true;
}

void ArgumentCoder<Cursor>::encode(ArgumentEncoder* encoder, const Cursor& cursor)
{
    encoder->encodeEnum(cursor.type());
        
    if (cursor.type() != Cursor::Custom)
        return;

    if (cursor.image()->isNull()) {
        encoder->encodeBool(false); // There is no valid image being encoded.
        return;
    }

    encoder->encodeBool(true);
    encodeImage(encoder, cursor.image());
    encoder->encode(cursor.hotSpot());
}

bool ArgumentCoder<Cursor>::decode(ArgumentDecoder* decoder, Cursor& cursor)
{
    Cursor::Type type;
    if (!decoder->decodeEnum(type))
        return false;

    if (type > Cursor::Custom)
        return false;

    if (type != Cursor::Custom) {
        const Cursor& cursorReference = Cursor::fromType(type);
        // Calling platformCursor here will eagerly create the platform cursor for the cursor singletons inside WebCore.
        // This will avoid having to re-create the platform cursors over and over.
        (void)cursorReference.platformCursor();

        cursor = cursorReference;
        return true;
    }

    bool isValidImagePresent;
    if (!decoder->decode(isValidImagePresent))
        return false;

    if (!isValidImagePresent) {
        cursor = Cursor(Image::nullImage(), IntPoint());
        return true;
    }

    RefPtr<Image> image;
    if (!decodeImage(decoder, image))
        return false;

    IntPoint hotSpot;
    if (!decoder->decode(hotSpot))
        return false;

    if (!image->rect().contains(hotSpot))
        return false;

    cursor = Cursor(image.get(), hotSpot);
    return true;
}

void ArgumentCoder<ResourceRequest>::encode(ArgumentEncoder* encoder, const ResourceRequest& resourceRequest)
{
    if (kShouldSerializeWebCoreData) {
        encoder->encode(resourceRequest.url().string());
        encoder->encode(resourceRequest.httpMethod());

        const HTTPHeaderMap& headers = resourceRequest.httpHeaderFields();
        encoder->encode(headers);

        FormData* httpBody = resourceRequest.httpBody();
        encoder->encode(static_cast<bool>(httpBody));
        if (httpBody)
            encoder->encode(httpBody->flattenToString());

        encoder->encode(resourceRequest.firstPartyForCookies().string());
    }

    encodePlatformData(encoder, resourceRequest);
}

bool ArgumentCoder<ResourceRequest>::decode(ArgumentDecoder* decoder, ResourceRequest& resourceRequest)
{
    if (kShouldSerializeWebCoreData) {
        ResourceRequest request;

        String url;
        if (!decoder->decode(url))
            return false;
        request.setURL(KURL(KURL(), url));

        String httpMethod;
        if (!decoder->decode(httpMethod))
            return false;
        request.setHTTPMethod(httpMethod);

        HTTPHeaderMap headers;
        if (!decoder->decode(headers))
            return false;
        request.addHTTPHeaderFields(headers);

        bool hasHTTPBody;
        if (!decoder->decode(hasHTTPBody))
            return false;
        if (hasHTTPBody) {
            String httpBody;
            if (!decoder->decode(httpBody))
                return false;
            request.setHTTPBody(FormData::create(httpBody.utf8()));
        }

        String firstPartyForCookies;
        if (!decoder->decode(firstPartyForCookies))
            return false;
        request.setFirstPartyForCookies(KURL(KURL(), firstPartyForCookies));

        resourceRequest = request;
    }

    return decodePlatformData(decoder, resourceRequest);
}

void ArgumentCoder<ResourceResponse>::encode(ArgumentEncoder* encoder, const ResourceResponse& resourceResponse)
{
    if (kShouldSerializeWebCoreData) {
        bool responseIsNull = resourceResponse.isNull();
        encoder->encode(responseIsNull);
        if (responseIsNull)
            return;

        encoder->encode(resourceResponse.url().string());
        encoder->encode(static_cast<int32_t>(resourceResponse.httpStatusCode()));

        const HTTPHeaderMap& headers = resourceResponse.httpHeaderFields();
        encoder->encode(headers);

        encoder->encode(resourceResponse.mimeType());
        encoder->encode(resourceResponse.textEncodingName());
        encoder->encode(static_cast<int64_t>(resourceResponse.expectedContentLength()));
        encoder->encode(resourceResponse.httpStatusText());
        encoder->encode(resourceResponse.suggestedFilename());
    }

    encodePlatformData(encoder, resourceResponse);
}

bool ArgumentCoder<ResourceResponse>::decode(ArgumentDecoder* decoder, ResourceResponse& resourceResponse)
{
    if (kShouldSerializeWebCoreData) {
        bool responseIsNull;
        if (!decoder->decode(responseIsNull))
            return false;
        if (responseIsNull) {
            resourceResponse = ResourceResponse();
            return true;
        }

        ResourceResponse response;

        String url;
        if (!decoder->decode(url))
            return false;
        response.setURL(KURL(KURL(), url));

        int32_t httpStatusCode;
        if (!decoder->decode(httpStatusCode))
            return false;
        response.setHTTPStatusCode(httpStatusCode);

        HTTPHeaderMap headers;
        if (!decoder->decode(headers))
            return false;
        for (HTTPHeaderMap::const_iterator it = headers.begin(), end = headers.end(); it != end; ++it)
            response.setHTTPHeaderField(it->key, it->value);

        String mimeType;
        if (!decoder->decode(mimeType))
            return false;
        response.setMimeType(mimeType);

        String textEncodingName;
        if (!decoder->decode(textEncodingName))
            return false;
        response.setTextEncodingName(textEncodingName);

        int64_t contentLength;
        if (!decoder->decode(contentLength))
            return false;
        response.setExpectedContentLength(contentLength);

        String httpStatusText;
        if (!decoder->decode(httpStatusText))
            return false;
        response.setHTTPStatusText(httpStatusText);

        String suggestedFilename;
        if (!decoder->decode(suggestedFilename))
            return false;
        response.setSuggestedFilename(suggestedFilename);

        resourceResponse = response;
    }

    return decodePlatformData(decoder, resourceResponse);
}

void ArgumentCoder<ResourceError>::encode(ArgumentEncoder* encoder, const ResourceError& resourceError)
{
    if (kShouldSerializeWebCoreData) {
        bool errorIsNull = resourceError.isNull();
        encoder->encode(errorIsNull);
        if (errorIsNull)
            return;

        encoder->encode(resourceError.domain());
        encoder->encode(resourceError.errorCode());
        encoder->encode(resourceError.failingURL());
        encoder->encode(resourceError.localizedDescription());
        encoder->encode(resourceError.isCancellation());
        encoder->encode(resourceError.isTimeout());
    }

    encodePlatformData(encoder, resourceError);
}

bool ArgumentCoder<ResourceError>::decode(ArgumentDecoder* decoder, ResourceError& resourceError)
{
    if (kShouldSerializeWebCoreData) {
        bool errorIsNull;
        if (!decoder->decode(errorIsNull))
            return false;
        if (errorIsNull) {
            resourceError = ResourceError();
            return true;
        }

        String domain;
        if (!decoder->decode(domain))
            return false;

        int errorCode;
        if (!decoder->decode(errorCode))
            return false;

        String failingURL;
        if (!decoder->decode(failingURL))
            return false;

        String localizedDescription;
        if (!decoder->decode(localizedDescription))
            return false;

        bool isCancellation;
        if (!decoder->decode(isCancellation))
            return false;

        bool isTimeout;
        if (!decoder->decode(isTimeout))
            return false;

        resourceError = ResourceError(domain, errorCode, failingURL, localizedDescription);
        resourceError.setIsCancellation(isCancellation);
        resourceError.setIsTimeout(isTimeout);
    }

    return decodePlatformData(decoder, resourceError);
}

void ArgumentCoder<WindowFeatures>::encode(ArgumentEncoder* encoder, const WindowFeatures& windowFeatures)
{
    encoder->encode(windowFeatures.x);
    encoder->encode(windowFeatures.y);
    encoder->encode(windowFeatures.width);
    encoder->encode(windowFeatures.height);
    encoder->encode(windowFeatures.xSet);
    encoder->encode(windowFeatures.ySet);
    encoder->encode(windowFeatures.widthSet);
    encoder->encode(windowFeatures.heightSet);
    encoder->encode(windowFeatures.menuBarVisible);
    encoder->encode(windowFeatures.statusBarVisible);
    encoder->encode(windowFeatures.toolBarVisible);
    encoder->encode(windowFeatures.locationBarVisible);
    encoder->encode(windowFeatures.scrollbarsVisible);
    encoder->encode(windowFeatures.resizable);
    encoder->encode(windowFeatures.fullscreen);
    encoder->encode(windowFeatures.dialog);
}

bool ArgumentCoder<WindowFeatures>::decode(ArgumentDecoder* decoder, WindowFeatures& windowFeatures)
{
    if (!decoder->decode(windowFeatures.x))
        return false;
    if (!decoder->decode(windowFeatures.y))
        return false;
    if (!decoder->decode(windowFeatures.width))
        return false;
    if (!decoder->decode(windowFeatures.height))
        return false;
    if (!decoder->decode(windowFeatures.xSet))
        return false;
    if (!decoder->decode(windowFeatures.ySet))
        return false;
    if (!decoder->decode(windowFeatures.widthSet))
        return false;
    if (!decoder->decode(windowFeatures.heightSet))
        return false;
    if (!decoder->decode(windowFeatures.menuBarVisible))
        return false;
    if (!decoder->decode(windowFeatures.statusBarVisible))
        return false;
    if (!decoder->decode(windowFeatures.toolBarVisible))
        return false;
    if (!decoder->decode(windowFeatures.locationBarVisible))
        return false;
    if (!decoder->decode(windowFeatures.scrollbarsVisible))
        return false;
    if (!decoder->decode(windowFeatures.resizable))
        return false;
    if (!decoder->decode(windowFeatures.fullscreen))
        return false;
    if (!decoder->decode(windowFeatures.dialog))
        return false;
    return true;
}


void ArgumentCoder<Color>::encode(ArgumentEncoder* encoder, const Color& color)
{
    if (!color.isValid()) {
        encoder->encodeBool(false);
        return;
    }

    encoder->encodeBool(true);
    encoder->encode(color.rgb());
}

bool ArgumentCoder<Color>::decode(ArgumentDecoder* decoder, Color& color)
{
    bool isValid;
    if (!decoder->decode(isValid))
        return false;

    if (!isValid) {
        color = Color();
        return true;
    }

    RGBA32 rgba;
    if (!decoder->decode(rgba))
        return false;

    color = Color(rgba);
    return true;
}


void ArgumentCoder<CompositionUnderline>::encode(ArgumentEncoder* encoder, const CompositionUnderline& underline)
{
    encoder->encode(underline.startOffset);
    encoder->encode(underline.endOffset);
    encoder->encode(underline.thick);
    encoder->encode(underline.color);
}

bool ArgumentCoder<CompositionUnderline>::decode(ArgumentDecoder* decoder, CompositionUnderline& underline)
{
    if (!decoder->decode(underline.startOffset))
        return false;
    if (!decoder->decode(underline.endOffset))
        return false;
    if (!decoder->decode(underline.thick))
        return false;
    if (!decoder->decode(underline.color))
        return false;

    return true;
}

#if ENABLE(SQL_DATABASE)
void ArgumentCoder<DatabaseDetails>::encode(ArgumentEncoder* encoder, const DatabaseDetails& details)
{
    encoder->encode(details.name());
    encoder->encode(details.displayName());
    encoder->encode(details.expectedUsage());
    encoder->encode(details.currentUsage());
}
    
bool ArgumentCoder<DatabaseDetails>::decode(ArgumentDecoder* decoder, DatabaseDetails& details)
{
    String name;
    if (!decoder->decode(name))
        return false;

    String displayName;
    if (!decoder->decode(displayName))
        return false;

    uint64_t expectedUsage;
    if (!decoder->decode(expectedUsage))
        return false;

    uint64_t currentUsage;
    if (!decoder->decode(currentUsage))
        return false;
    
    details = DatabaseDetails(name, displayName, expectedUsage, currentUsage);
    return true;
}
#endif

void ArgumentCoder<DictationAlternative>::encode(ArgumentEncoder* encoder, const DictationAlternative& dictationAlternative)
{
    encoder->encode(dictationAlternative.rangeStart);
    encoder->encode(dictationAlternative.rangeLength);
    encoder->encode(dictationAlternative.dictationContext);
}

bool ArgumentCoder<DictationAlternative>::decode(ArgumentDecoder* decoder, DictationAlternative& dictationAlternative)
{
    if (!decoder->decode(dictationAlternative.rangeStart))
        return false;
    if (!decoder->decode(dictationAlternative.rangeLength))
        return false;
    if (!decoder->decode(dictationAlternative.dictationContext))
        return false;
    return true;
}


void ArgumentCoder<FileChooserSettings>::encode(ArgumentEncoder* encoder, const FileChooserSettings& settings)
{
    encoder->encode(settings.allowsMultipleFiles);
#if ENABLE(DIRECTORY_UPLOAD)
    encoder->encode(settings.allowsDirectoryUpload);
#endif
    encoder->encode(settings.acceptMIMETypes);
    encoder->encode(settings.selectedFiles);
#if ENABLE(MEDIA_CAPTURE)
    encoder->encode(settings.capture);
#endif
}

bool ArgumentCoder<FileChooserSettings>::decode(ArgumentDecoder* decoder, FileChooserSettings& settings)
{
    if (!decoder->decode(settings.allowsMultipleFiles))
        return false;
#if ENABLE(DIRECTORY_UPLOAD)
    if (!decoder->decode(settings.allowsDirectoryUpload))
        return false;
#endif
    if (!decoder->decode(settings.acceptMIMETypes))
        return false;
    if (!decoder->decode(settings.selectedFiles))
        return false;
#if ENABLE(MEDIA_CAPTURE)
    if (!decoder->decode(settings.capture))
        return false;
#endif

    return true;
}


void ArgumentCoder<GrammarDetail>::encode(ArgumentEncoder* encoder, const GrammarDetail& detail)
{
    encoder->encode(detail.location);
    encoder->encode(detail.length);
    encoder->encode(detail.guesses);
    encoder->encode(detail.userDescription);
}

bool ArgumentCoder<GrammarDetail>::decode(ArgumentDecoder* decoder, GrammarDetail& detail)
{
    if (!decoder->decode(detail.location))
        return false;
    if (!decoder->decode(detail.length))
        return false;
    if (!decoder->decode(detail.guesses))
        return false;
    if (!decoder->decode(detail.userDescription))
        return false;

    return true;
}


void ArgumentCoder<TextCheckingResult>::encode(ArgumentEncoder* encoder, const TextCheckingResult& result)
{
    encoder->encodeEnum(result.type);
    encoder->encode(result.location);
    encoder->encode(result.length);
    encoder->encode(result.details);
    encoder->encode(result.replacement);
}

bool ArgumentCoder<TextCheckingResult>::decode(ArgumentDecoder* decoder, TextCheckingResult& result)
{
    if (!decoder->decodeEnum(result.type))
        return false;
    if (!decoder->decode(result.location))
        return false;
    if (!decoder->decode(result.length))
        return false;
    if (!decoder->decode(result.details))
        return false;
    if (!decoder->decode(result.replacement))
        return false;
    return true;
}

void ArgumentCoder<DragSession>::encode(ArgumentEncoder* encoder, const DragSession& result)
{
    encoder->encodeEnum(result.operation);
    encoder->encode(result.mouseIsOverFileInput);
    encoder->encode(result.numberOfItemsToBeAccepted);
}

bool ArgumentCoder<DragSession>::decode(ArgumentDecoder* decoder, DragSession& result)
{
    if (!decoder->decodeEnum(result.operation))
        return false;
    if (!decoder->decode(result.mouseIsOverFileInput))
        return false;
    if (!decoder->decode(result.numberOfItemsToBeAccepted))
        return false;
    return true;
}

void ArgumentCoder<KURL>::encode(ArgumentEncoder* encoder, const KURL& result)
{
    encoder->encode(result.string());
}
    
bool ArgumentCoder<KURL>::decode(ArgumentDecoder* decoder, KURL& result)
{
    String urlAsString;
    if (!decoder->decode(urlAsString))
        return false;
    result = KURL(WebCore::ParsedURLString, urlAsString);
    return true;
}

void ArgumentCoder<WebCore::UserStyleSheet>::encode(ArgumentEncoder* encoder, const WebCore::UserStyleSheet& userStyleSheet)
{
    encoder->encode(userStyleSheet.source());
    encoder->encode(userStyleSheet.url());
    encoder->encode(userStyleSheet.whitelist());
    encoder->encode(userStyleSheet.blacklist());
    encoder->encodeEnum(userStyleSheet.injectedFrames());
    encoder->encodeEnum(userStyleSheet.level());
}

bool ArgumentCoder<WebCore::UserStyleSheet>::decode(ArgumentDecoder* decoder, WebCore::UserStyleSheet& userStyleSheet)
{
    String source;
    if (!decoder->decode(source))
        return false;

    KURL url;
    if (!decoder->decode(url))
        return false;

    Vector<String> whitelist;
    if (!decoder->decode(whitelist))
        return false;

    Vector<String> blacklist;
    if (!decoder->decode(blacklist))
        return false;

    WebCore::UserContentInjectedFrames injectedFrames;
    if (!decoder->decodeEnum(injectedFrames))
        return false;

    WebCore::UserStyleLevel level;
    if (!decoder->decodeEnum(level))
        return false;

    userStyleSheet = WebCore::UserStyleSheet(source, url, whitelist, blacklist, injectedFrames, level);
    return true;
}

void ArgumentCoder<WebCore::UserScript>::encode(ArgumentEncoder* encoder, const WebCore::UserScript& userScript)
{
    encoder->encode(userScript.source());
    encoder->encode(userScript.url());
    encoder->encode(userScript.whitelist());
    encoder->encode(userScript.blacklist());
    encoder->encodeEnum(userScript.injectionTime());
    encoder->encodeEnum(userScript.injectedFrames());
}

bool ArgumentCoder<WebCore::UserScript>::decode(ArgumentDecoder* decoder, WebCore::UserScript& userScript)
{
    String source;
    if (!decoder->decode(source))
        return false;

    KURL url;
    if (!decoder->decode(url))
        return false;

    Vector<String> whitelist;
    if (!decoder->decode(whitelist))
        return false;

    Vector<String> blacklist;
    if (!decoder->decode(blacklist))
        return false;

    WebCore::UserScriptInjectionTime injectionTime;
    if (!decoder->decodeEnum(injectionTime))
        return false;

    WebCore::UserContentInjectedFrames injectedFrames;
    if (!decoder->decodeEnum(injectedFrames))
        return false;

    userScript = WebCore::UserScript(source, url, whitelist, blacklist, injectionTime, injectedFrames);
    return true;
}

} // namespace CoreIPC
