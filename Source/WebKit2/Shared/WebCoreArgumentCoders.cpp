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

#include "DataReference.h"
#include "ShareableBitmap.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Cookie.h>
#include <WebCore/Credential.h>
#include <WebCore/Cursor.h>
#include <WebCore/DatabaseDetails.h>
#include <WebCore/DictationAlternative.h>
#include <WebCore/DragSession.h>
#include <WebCore/Editor.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FilterOperation.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyPath.h>
#include <WebCore/IDBKeyRangeData.h>
#include <WebCore/Image.h>
#include <WebCore/Length.h>
#include <WebCore/PluginData.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/Region.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/SessionID.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TimingFunction.h>
#include <WebCore/TransformationMatrix.h>
#include <WebCore/URL.h>
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(IOS)
#include <WebCore/FloatQuad.h>
#include <WebCore/Pasteboard.h>
#include <WebCore/SelectionRect.h>
#include <WebCore/SharedBuffer.h>
#endif // PLATFORM(IOS)

using namespace WebCore;
using namespace WebKit;

namespace IPC {

void ArgumentCoder<AffineTransform>::encode(ArgumentEncoder& encoder, const AffineTransform& affineTransform)
{
    SimpleArgumentCoder<AffineTransform>::encode(encoder, affineTransform);
}

bool ArgumentCoder<AffineTransform>::decode(ArgumentDecoder& decoder, AffineTransform& affineTransform)
{
    return SimpleArgumentCoder<AffineTransform>::decode(decoder, affineTransform);
}

void ArgumentCoder<TransformationMatrix>::encode(ArgumentEncoder& encoder, const TransformationMatrix& transformationMatrix)
{
    SimpleArgumentCoder<TransformationMatrix>::encode(encoder, transformationMatrix);
}

bool ArgumentCoder<TransformationMatrix>::decode(ArgumentDecoder& decoder, TransformationMatrix& transformationMatrix)
{
    return SimpleArgumentCoder<TransformationMatrix>::decode(decoder, transformationMatrix);
}

void ArgumentCoder<LinearTimingFunction>::encode(ArgumentEncoder& encoder, const LinearTimingFunction& timingFunction)
{
    encoder.encodeEnum(timingFunction.type());
}

bool ArgumentCoder<LinearTimingFunction>::decode(ArgumentDecoder& decoder, LinearTimingFunction& timingFunction)
{
    // Type is decoded by the caller. Nothing else to decode.
    return true;
}

void ArgumentCoder<CubicBezierTimingFunction>::encode(ArgumentEncoder& encoder, const CubicBezierTimingFunction& timingFunction)
{
    encoder.encodeEnum(timingFunction.type());
    
    encoder << timingFunction.x1();
    encoder << timingFunction.y1();
    encoder << timingFunction.x2();
    encoder << timingFunction.y2();
    
    encoder.encodeEnum(timingFunction.timingFunctionPreset());
}

bool ArgumentCoder<CubicBezierTimingFunction>::decode(ArgumentDecoder& decoder, CubicBezierTimingFunction& timingFunction)
{
    // Type is decoded by the caller.
    double x1;
    if (!decoder.decode(x1))
        return false;

    double y1;
    if (!decoder.decode(y1))
        return false;

    double x2;
    if (!decoder.decode(x2))
        return false;

    double y2;
    if (!decoder.decode(y2))
        return false;

    CubicBezierTimingFunction::TimingFunctionPreset preset;
    if (!decoder.decodeEnum(preset))
        return false;

    timingFunction.setValues(x1, y1, x2, y2);
    timingFunction.setTimingFunctionPreset(preset);

    return true;
}

void ArgumentCoder<StepsTimingFunction>::encode(ArgumentEncoder& encoder, const StepsTimingFunction& timingFunction)
{
    encoder.encodeEnum(timingFunction.type());
    
    encoder << timingFunction.numberOfSteps();
    encoder << timingFunction.stepAtStart();
}

bool ArgumentCoder<StepsTimingFunction>::decode(ArgumentDecoder& decoder, StepsTimingFunction& timingFunction)
{
    // Type is decoded by the caller.
    int numSteps;
    if (!decoder.decode(numSteps))
        return false;

    bool stepAtStart;
    if (!decoder.decode(stepAtStart))
        return false;

    timingFunction.setNumberOfSteps(numSteps);
    timingFunction.setStepAtStart(stepAtStart);

    return true;
}

void ArgumentCoder<FloatPoint>::encode(ArgumentEncoder& encoder, const FloatPoint& floatPoint)
{
    SimpleArgumentCoder<FloatPoint>::encode(encoder, floatPoint);
}

bool ArgumentCoder<FloatPoint>::decode(ArgumentDecoder& decoder, FloatPoint& floatPoint)
{
    return SimpleArgumentCoder<FloatPoint>::decode(decoder, floatPoint);
}


void ArgumentCoder<FloatPoint3D>::encode(ArgumentEncoder& encoder, const FloatPoint3D& floatPoint)
{
    SimpleArgumentCoder<FloatPoint3D>::encode(encoder, floatPoint);
}

bool ArgumentCoder<FloatPoint3D>::decode(ArgumentDecoder& decoder, FloatPoint3D& floatPoint)
{
    return SimpleArgumentCoder<FloatPoint3D>::decode(decoder, floatPoint);
}


void ArgumentCoder<FloatRect>::encode(ArgumentEncoder& encoder, const FloatRect& floatRect)
{
    SimpleArgumentCoder<FloatRect>::encode(encoder, floatRect);
}

bool ArgumentCoder<FloatRect>::decode(ArgumentDecoder& decoder, FloatRect& floatRect)
{
    return SimpleArgumentCoder<FloatRect>::decode(decoder, floatRect);
}


void ArgumentCoder<FloatSize>::encode(ArgumentEncoder& encoder, const FloatSize& floatSize)
{
    SimpleArgumentCoder<FloatSize>::encode(encoder, floatSize);
}

bool ArgumentCoder<FloatSize>::decode(ArgumentDecoder& decoder, FloatSize& floatSize)
{
    return SimpleArgumentCoder<FloatSize>::decode(decoder, floatSize);
}


#if PLATFORM(IOS)
void ArgumentCoder<FloatQuad>::encode(ArgumentEncoder& encoder, const FloatQuad& floatQuad)
{
    SimpleArgumentCoder<FloatQuad>::encode(encoder, floatQuad);
}

bool ArgumentCoder<FloatQuad>::decode(ArgumentDecoder& decoder, FloatQuad& floatQuad)
{
    return SimpleArgumentCoder<FloatQuad>::decode(decoder, floatQuad);
}

void ArgumentCoder<ViewportArguments>::encode(ArgumentEncoder& encoder, const ViewportArguments& viewportArguments)
{
    SimpleArgumentCoder<ViewportArguments>::encode(encoder, viewportArguments);
}

bool ArgumentCoder<ViewportArguments>::decode(ArgumentDecoder& decoder, ViewportArguments& viewportArguments)
{
    return SimpleArgumentCoder<ViewportArguments>::decode(decoder, viewportArguments);
}
#endif // PLATFORM(IOS)


void ArgumentCoder<IntPoint>::encode(ArgumentEncoder& encoder, const IntPoint& intPoint)
{
    SimpleArgumentCoder<IntPoint>::encode(encoder, intPoint);
}

bool ArgumentCoder<IntPoint>::decode(ArgumentDecoder& decoder, IntPoint& intPoint)
{
    return SimpleArgumentCoder<IntPoint>::decode(decoder, intPoint);
}


void ArgumentCoder<IntRect>::encode(ArgumentEncoder& encoder, const IntRect& intRect)
{
    SimpleArgumentCoder<IntRect>::encode(encoder, intRect);
}

bool ArgumentCoder<IntRect>::decode(ArgumentDecoder& decoder, IntRect& intRect)
{
    return SimpleArgumentCoder<IntRect>::decode(decoder, intRect);
}


void ArgumentCoder<IntSize>::encode(ArgumentEncoder& encoder, const IntSize& intSize)
{
    SimpleArgumentCoder<IntSize>::encode(encoder, intSize);
}

bool ArgumentCoder<IntSize>::decode(ArgumentDecoder& decoder, IntSize& intSize)
{
    return SimpleArgumentCoder<IntSize>::decode(decoder, intSize);
}

template<> struct ArgumentCoder<WebCore::Region::Span> {
    static void encode(ArgumentEncoder&, const WebCore::Region::Span&);
    static bool decode(ArgumentDecoder&, WebCore::Region::Span&);
};

void ArgumentCoder<Region::Span>::encode(ArgumentEncoder& encoder, const Region::Span& span)
{
    encoder << span.y;
    encoder << (uint64_t)span.segmentIndex;
}

bool ArgumentCoder<Region::Span>::decode(ArgumentDecoder& decoder, Region::Span& span)
{
    if (!decoder.decode(span.y))
        return false;
    
    uint64_t segmentIndex;
    if (!decoder.decode(segmentIndex))
        return false;
    
    span.segmentIndex = segmentIndex;
    return true;
}

void ArgumentCoder<Region>::encode(ArgumentEncoder& encoder, const Region& region)
{
    encoder.encode(region.shapeSegments());
    encoder.encode(region.shapeSpans());
}

bool ArgumentCoder<Region>::decode(ArgumentDecoder& decoder, Region& region)
{
    Vector<int> segments;
    if (!decoder.decode(segments))
        return false;

    Vector<Region::Span> spans;
    if (!decoder.decode(spans))
        return false;
    
    region.setShapeSegments(segments);
    region.setShapeSpans(spans);
    region.updateBoundsFromShape();
    
    if (!region.isValid())
        return false;

    return true;
}

void ArgumentCoder<Length>::encode(ArgumentEncoder& encoder, const Length& length)
{
    SimpleArgumentCoder<Length>::encode(encoder, length);
}

bool ArgumentCoder<Length>::decode(ArgumentDecoder& decoder, Length& length)
{
    return SimpleArgumentCoder<Length>::decode(decoder, length);
}


void ArgumentCoder<ViewportAttributes>::encode(ArgumentEncoder& encoder, const ViewportAttributes& viewportAttributes)
{
    SimpleArgumentCoder<ViewportAttributes>::encode(encoder, viewportAttributes);
}

bool ArgumentCoder<ViewportAttributes>::decode(ArgumentDecoder& decoder, ViewportAttributes& viewportAttributes)
{
    return SimpleArgumentCoder<ViewportAttributes>::decode(decoder, viewportAttributes);
}


void ArgumentCoder<MimeClassInfo>::encode(ArgumentEncoder& encoder, const MimeClassInfo& mimeClassInfo)
{
    encoder << mimeClassInfo.type << mimeClassInfo.desc << mimeClassInfo.extensions;
}

bool ArgumentCoder<MimeClassInfo>::decode(ArgumentDecoder& decoder, MimeClassInfo& mimeClassInfo)
{
    if (!decoder.decode(mimeClassInfo.type))
        return false;
    if (!decoder.decode(mimeClassInfo.desc))
        return false;
    if (!decoder.decode(mimeClassInfo.extensions))
        return false;

    return true;
}


void ArgumentCoder<PluginInfo>::encode(ArgumentEncoder& encoder, const PluginInfo& pluginInfo)
{
    encoder << pluginInfo.name << pluginInfo.file << pluginInfo.desc << pluginInfo.mimes << pluginInfo.isApplicationPlugin;
}
    
bool ArgumentCoder<PluginInfo>::decode(ArgumentDecoder& decoder, PluginInfo& pluginInfo)
{
    if (!decoder.decode(pluginInfo.name))
        return false;
    if (!decoder.decode(pluginInfo.file))
        return false;
    if (!decoder.decode(pluginInfo.desc))
        return false;
    if (!decoder.decode(pluginInfo.mimes))
        return false;
    if (!decoder.decode(pluginInfo.isApplicationPlugin))
        return false;

    return true;
}


void ArgumentCoder<HTTPHeaderMap>::encode(ArgumentEncoder& encoder, const HTTPHeaderMap& headerMap)
{
    encoder << static_cast<const HashMap<AtomicString, String, CaseFoldingHash>&>(headerMap);
}

bool ArgumentCoder<HTTPHeaderMap>::decode(ArgumentDecoder& decoder, HTTPHeaderMap& headerMap)
{
    return decoder.decode(static_cast<HashMap<AtomicString, String, CaseFoldingHash>&>(headerMap));
}


void ArgumentCoder<AuthenticationChallenge>::encode(ArgumentEncoder& encoder, const AuthenticationChallenge& challenge)
{
    encoder << challenge.protectionSpace() << challenge.proposedCredential() << challenge.previousFailureCount() << challenge.failureResponse() << challenge.error();
}

bool ArgumentCoder<AuthenticationChallenge>::decode(ArgumentDecoder& decoder, AuthenticationChallenge& challenge)
{    
    ProtectionSpace protectionSpace;
    if (!decoder.decode(protectionSpace))
        return false;

    Credential proposedCredential;
    if (!decoder.decode(proposedCredential))
        return false;

    unsigned previousFailureCount;    
    if (!decoder.decode(previousFailureCount))
        return false;

    ResourceResponse failureResponse;
    if (!decoder.decode(failureResponse))
        return false;

    ResourceError error;
    if (!decoder.decode(error))
        return false;
    
    challenge = AuthenticationChallenge(protectionSpace, proposedCredential, previousFailureCount, failureResponse, error);
    return true;
}


void ArgumentCoder<ProtectionSpace>::encode(ArgumentEncoder& encoder, const ProtectionSpace& space)
{
    encoder << space.host() << space.port() << space.realm();
    encoder.encodeEnum(space.authenticationScheme());
    encoder.encodeEnum(space.serverType());
}

bool ArgumentCoder<ProtectionSpace>::decode(ArgumentDecoder& decoder, ProtectionSpace& space)
{
    String host;
    if (!decoder.decode(host))
        return false;

    int port;
    if (!decoder.decode(port))
        return false;

    String realm;
    if (!decoder.decode(realm))
        return false;
    
    ProtectionSpaceAuthenticationScheme authenticationScheme;
    if (!decoder.decodeEnum(authenticationScheme))
        return false;

    ProtectionSpaceServerType serverType;
    if (!decoder.decodeEnum(serverType))
        return false;

    space = ProtectionSpace(host, port, serverType, realm, authenticationScheme);
    return true;
}

void ArgumentCoder<Credential>::encode(ArgumentEncoder& encoder, const Credential& credential)
{
    encoder << credential.user() << credential.password();
    encoder.encodeEnum(credential.persistence());
}

bool ArgumentCoder<Credential>::decode(ArgumentDecoder& decoder, Credential& credential)
{
    String user;
    if (!decoder.decode(user))
        return false;

    String password;
    if (!decoder.decode(password))
        return false;

    CredentialPersistence persistence;
    if (!decoder.decodeEnum(persistence))
        return false;
    
    credential = Credential(user, password, persistence);
    return true;
}

static void encodeImage(ArgumentEncoder& encoder, Image* image)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(IntSize(image->size()), ShareableBitmap::SupportsAlpha);
    bitmap->createGraphicsContext()->drawImage(image, ColorSpaceDeviceRGB, IntPoint());

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);

    encoder << handle;
}

static bool decodeImage(ArgumentDecoder& decoder, RefPtr<Image>& image)
{
    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;
    
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle);
    if (!bitmap)
        return false;
    image = bitmap->createImage();
    if (!image)
        return false;
    return true;
}

#if !PLATFORM(IOS)
void ArgumentCoder<Cursor>::encode(ArgumentEncoder& encoder, const Cursor& cursor)
{
    encoder.encodeEnum(cursor.type());
        
    if (cursor.type() != Cursor::Custom)
        return;

    if (cursor.image()->isNull()) {
        encoder << false; // There is no valid image being encoded.
        return;
    }

    encoder << true;
    encodeImage(encoder, cursor.image());
    encoder << cursor.hotSpot();
}

bool ArgumentCoder<Cursor>::decode(ArgumentDecoder& decoder, Cursor& cursor)
{
    Cursor::Type type;
    if (!decoder.decodeEnum(type))
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
    if (!decoder.decode(isValidImagePresent))
        return false;

    if (!isValidImagePresent) {
        cursor = Cursor(Image::nullImage(), IntPoint());
        return true;
    }

    RefPtr<Image> image;
    if (!decodeImage(decoder, image))
        return false;

    IntPoint hotSpot;
    if (!decoder.decode(hotSpot))
        return false;

    if (!image->rect().contains(hotSpot))
        return false;

    cursor = Cursor(image.get(), hotSpot);
    return true;
}
#endif

void ArgumentCoder<ResourceRequest>::encode(ArgumentEncoder& encoder, const ResourceRequest& resourceRequest)
{
    if (kShouldSerializeWebCoreData) {
        encoder << resourceRequest.url().string();
        encoder << resourceRequest.httpMethod();
        encoder << resourceRequest.httpHeaderFields();

        // FIXME: Do not encode HTTP message body.
        // 1. It can be large and thus costly to send across.
        // 2. It is misleading to provide a body with some requests, while others use body streams, which cannot be serialized at all.
        FormData* httpBody = resourceRequest.httpBody();
        encoder << static_cast<bool>(httpBody);
        if (httpBody)
            encoder << httpBody->flattenToString();

        encoder << resourceRequest.firstPartyForCookies().string();
    }

#if ENABLE(CACHE_PARTITIONING)
    encoder << resourceRequest.cachePartition();
#endif

#if ENABLE(INSPECTOR)
    encoder << resourceRequest.hiddenFromInspector();
#endif

    encodePlatformData(encoder, resourceRequest);
}

bool ArgumentCoder<ResourceRequest>::decode(ArgumentDecoder& decoder, ResourceRequest& resourceRequest)
{
    if (kShouldSerializeWebCoreData) {
        ResourceRequest request;

        String url;
        if (!decoder.decode(url))
            return false;
        request.setURL(URL(URL(), url));

        String httpMethod;
        if (!decoder.decode(httpMethod))
            return false;
        request.setHTTPMethod(httpMethod);

        HTTPHeaderMap headers;
        if (!decoder.decode(headers))
            return false;
        request.addHTTPHeaderFields(headers);

        bool hasHTTPBody;
        if (!decoder.decode(hasHTTPBody))
            return false;
        if (hasHTTPBody) {
            String httpBody;
            if (!decoder.decode(httpBody))
                return false;
            request.setHTTPBody(FormData::create(httpBody.utf8()));
        }

        String firstPartyForCookies;
        if (!decoder.decode(firstPartyForCookies))
            return false;
        request.setFirstPartyForCookies(URL(URL(), firstPartyForCookies));

        resourceRequest = request;
    }

#if ENABLE(CACHE_PARTITIONING)
    String cachePartition;
    if (!decoder.decode(cachePartition))
        return false;
    resourceRequest.setCachePartition(cachePartition);
#endif

#if ENABLE(INSPECTOR)
    bool isHiddenFromInspector;
    if (!decoder.decode(isHiddenFromInspector))
        return false;
    resourceRequest.setHiddenFromInspector(isHiddenFromInspector);
#endif

    return decodePlatformData(decoder, resourceRequest);
}

void ArgumentCoder<ResourceResponse>::encode(ArgumentEncoder& encoder, const ResourceResponse& resourceResponse)
{
#if PLATFORM(COCOA)
    bool shouldSerializeWebCoreData = !resourceResponse.platformResponseIsUpToDate();
    encoder << shouldSerializeWebCoreData;
#else
    bool shouldSerializeWebCoreData = true;
#endif

    encodePlatformData(encoder, resourceResponse);

    if (shouldSerializeWebCoreData) {
        bool responseIsNull = resourceResponse.isNull();
        encoder << responseIsNull;
        if (responseIsNull)
            return;

        encoder << resourceResponse.url().string();
        encoder << static_cast<int32_t>(resourceResponse.httpStatusCode());
        encoder << resourceResponse.httpHeaderFields();

        encoder << resourceResponse.mimeType();
        encoder << resourceResponse.textEncodingName();
        encoder << static_cast<int64_t>(resourceResponse.expectedContentLength());
        encoder << resourceResponse.httpStatusText();
        encoder << resourceResponse.suggestedFilename();
    }
}

bool ArgumentCoder<ResourceResponse>::decode(ArgumentDecoder& decoder, ResourceResponse& resourceResponse)
{
#if PLATFORM(COCOA)
    bool hasSerializedWebCoreData;
    if (!decoder.decode(hasSerializedWebCoreData))
        return false;
#else
    bool hasSerializedWebCoreData = true;
#endif

    ResourceResponse response;

    if (!decodePlatformData(decoder, response))
        return false;

    if (hasSerializedWebCoreData) {
        bool responseIsNull;
        if (!decoder.decode(responseIsNull))
            return false;
        if (responseIsNull) {
            resourceResponse = ResourceResponse();
            return true;
        }

        String url;
        if (!decoder.decode(url))
            return false;
        response.setURL(URL(URL(), url));

        int32_t httpStatusCode;
        if (!decoder.decode(httpStatusCode))
            return false;
        response.setHTTPStatusCode(httpStatusCode);

        HTTPHeaderMap headers;
        if (!decoder.decode(headers))
            return false;
        for (HTTPHeaderMap::const_iterator it = headers.begin(), end = headers.end(); it != end; ++it)
            response.setHTTPHeaderField(it->key, it->value);

        String mimeType;
        if (!decoder.decode(mimeType))
            return false;
        response.setMimeType(mimeType);

        String textEncodingName;
        if (!decoder.decode(textEncodingName))
            return false;
        response.setTextEncodingName(textEncodingName);

        int64_t contentLength;
        if (!decoder.decode(contentLength))
            return false;
        response.setExpectedContentLength(contentLength);

        String httpStatusText;
        if (!decoder.decode(httpStatusText))
            return false;
        response.setHTTPStatusText(httpStatusText);

        String suggestedFilename;
        if (!decoder.decode(suggestedFilename))
            return false;
        response.setSuggestedFilename(suggestedFilename);
    }

    resourceResponse = response;

    return true;
}

void ArgumentCoder<ResourceError>::encode(ArgumentEncoder& encoder, const ResourceError& resourceError)
{
    if (kShouldSerializeWebCoreData) {
        bool errorIsNull = resourceError.isNull();
        encoder << errorIsNull;
        if (errorIsNull)
            return;

        encoder << resourceError.domain();
        encoder << resourceError.errorCode();
        encoder << resourceError.failingURL();
        encoder << resourceError.localizedDescription();
        encoder << resourceError.isCancellation();
        encoder << resourceError.isTimeout();
    }

    encodePlatformData(encoder, resourceError);
}

bool ArgumentCoder<ResourceError>::decode(ArgumentDecoder& decoder, ResourceError& resourceError)
{
    if (kShouldSerializeWebCoreData) {
        bool errorIsNull;
        if (!decoder.decode(errorIsNull))
            return false;
        if (errorIsNull) {
            resourceError = ResourceError();
            return true;
        }

        String domain;
        if (!decoder.decode(domain))
            return false;

        int errorCode;
        if (!decoder.decode(errorCode))
            return false;

        String failingURL;
        if (!decoder.decode(failingURL))
            return false;

        String localizedDescription;
        if (!decoder.decode(localizedDescription))
            return false;

        bool isCancellation;
        if (!decoder.decode(isCancellation))
            return false;

        bool isTimeout;
        if (!decoder.decode(isTimeout))
            return false;

        resourceError = ResourceError(domain, errorCode, failingURL, localizedDescription);
        resourceError.setIsCancellation(isCancellation);
        resourceError.setIsTimeout(isTimeout);
    }

    return decodePlatformData(decoder, resourceError);
}

#if PLATFORM(IOS)

void ArgumentCoder<SelectionRect>::encode(ArgumentEncoder& encoder, const SelectionRect& selectionRect)
{
    encoder << selectionRect.rect();
    encoder << static_cast<uint32_t>(selectionRect.direction());
    encoder << selectionRect.minX();
    encoder << selectionRect.maxX();
    encoder << selectionRect.maxY();
    encoder << selectionRect.lineNumber();
    encoder << selectionRect.isLineBreak();
    encoder << selectionRect.isFirstOnLine();
    encoder << selectionRect.isLastOnLine();
    encoder << selectionRect.containsStart();
    encoder << selectionRect.containsEnd();
    encoder << selectionRect.isHorizontal();
}

bool ArgumentCoder<SelectionRect>::decode(ArgumentDecoder& decoder, SelectionRect& selectionRect)
{
    WebCore::IntRect rect;
    if (!decoder.decode(rect))
        return false;
    selectionRect.setRect(rect);

    uint32_t direction;
    if (!decoder.decode(direction))
        return false;
    selectionRect.setDirection((WebCore::TextDirection)direction);

    int intValue;
    if (!decoder.decode(intValue))
        return false;
    selectionRect.setMinX(intValue);

    if (!decoder.decode(intValue))
        return false;
    selectionRect.setMaxX(intValue);

    if (!decoder.decode(intValue))
        return false;
    selectionRect.setMaxY(intValue);

    if (!decoder.decode(intValue))
        return false;
    selectionRect.setLineNumber(intValue);

    bool boolValue;
    if (!decoder.decode(boolValue))
        return false;
    selectionRect.setIsLineBreak(boolValue);

    if (!decoder.decode(boolValue))
        return false;
    selectionRect.setIsFirstOnLine(boolValue);

    if (!decoder.decode(boolValue))
        return false;
    selectionRect.setIsLastOnLine(boolValue);

    if (!decoder.decode(boolValue))
        return false;
    selectionRect.setContainsStart(boolValue);

    if (!decoder.decode(boolValue))
        return false;
    selectionRect.setContainsEnd(boolValue);

    if (!decoder.decode(boolValue))
        return false;
    selectionRect.setIsHorizontal(boolValue);

    return true;
}

#endif

void ArgumentCoder<WindowFeatures>::encode(ArgumentEncoder& encoder, const WindowFeatures& windowFeatures)
{
    encoder << windowFeatures.x;
    encoder << windowFeatures.y;
    encoder << windowFeatures.width;
    encoder << windowFeatures.height;
    encoder << windowFeatures.xSet;
    encoder << windowFeatures.ySet;
    encoder << windowFeatures.widthSet;
    encoder << windowFeatures.heightSet;
    encoder << windowFeatures.menuBarVisible;
    encoder << windowFeatures.statusBarVisible;
    encoder << windowFeatures.toolBarVisible;
    encoder << windowFeatures.locationBarVisible;
    encoder << windowFeatures.scrollbarsVisible;
    encoder << windowFeatures.resizable;
    encoder << windowFeatures.fullscreen;
    encoder << windowFeatures.dialog;
}

bool ArgumentCoder<WindowFeatures>::decode(ArgumentDecoder& decoder, WindowFeatures& windowFeatures)
{
    if (!decoder.decode(windowFeatures.x))
        return false;
    if (!decoder.decode(windowFeatures.y))
        return false;
    if (!decoder.decode(windowFeatures.width))
        return false;
    if (!decoder.decode(windowFeatures.height))
        return false;
    if (!decoder.decode(windowFeatures.xSet))
        return false;
    if (!decoder.decode(windowFeatures.ySet))
        return false;
    if (!decoder.decode(windowFeatures.widthSet))
        return false;
    if (!decoder.decode(windowFeatures.heightSet))
        return false;
    if (!decoder.decode(windowFeatures.menuBarVisible))
        return false;
    if (!decoder.decode(windowFeatures.statusBarVisible))
        return false;
    if (!decoder.decode(windowFeatures.toolBarVisible))
        return false;
    if (!decoder.decode(windowFeatures.locationBarVisible))
        return false;
    if (!decoder.decode(windowFeatures.scrollbarsVisible))
        return false;
    if (!decoder.decode(windowFeatures.resizable))
        return false;
    if (!decoder.decode(windowFeatures.fullscreen))
        return false;
    if (!decoder.decode(windowFeatures.dialog))
        return false;
    return true;
}


void ArgumentCoder<Color>::encode(ArgumentEncoder& encoder, const Color& color)
{
    if (!color.isValid()) {
        encoder << false;
        return;
    }

    encoder << true;
    encoder << color.rgb();
}

bool ArgumentCoder<Color>::decode(ArgumentDecoder& decoder, Color& color)
{
    bool isValid;
    if (!decoder.decode(isValid))
        return false;

    if (!isValid) {
        color = Color();
        return true;
    }

    RGBA32 rgba;
    if (!decoder.decode(rgba))
        return false;

    color = Color(rgba);
    return true;
}


void ArgumentCoder<CompositionUnderline>::encode(ArgumentEncoder& encoder, const CompositionUnderline& underline)
{
    encoder << underline.startOffset;
    encoder << underline.endOffset;
    encoder << underline.thick;
    encoder << underline.color;
}

bool ArgumentCoder<CompositionUnderline>::decode(ArgumentDecoder& decoder, CompositionUnderline& underline)
{
    if (!decoder.decode(underline.startOffset))
        return false;
    if (!decoder.decode(underline.endOffset))
        return false;
    if (!decoder.decode(underline.thick))
        return false;
    if (!decoder.decode(underline.color))
        return false;

    return true;
}


void ArgumentCoder<Cookie>::encode(ArgumentEncoder& encoder, const Cookie& cookie)
{
    encoder << cookie.name;
    encoder << cookie.value;
    encoder << cookie.domain;
    encoder << cookie.path;
    encoder << cookie.expires;
    encoder << cookie.httpOnly;
    encoder << cookie.secure;
    encoder << cookie.session;
}

bool ArgumentCoder<Cookie>::decode(ArgumentDecoder& decoder, Cookie& cookie)
{
    if (!decoder.decode(cookie.name))
        return false;
    if (!decoder.decode(cookie.value))
        return false;
    if (!decoder.decode(cookie.domain))
        return false;
    if (!decoder.decode(cookie.path))
        return false;
    if (!decoder.decode(cookie.expires))
        return false;
    if (!decoder.decode(cookie.httpOnly))
        return false;
    if (!decoder.decode(cookie.secure))
        return false;
    if (!decoder.decode(cookie.session))
        return false;

    return true;
}


#if ENABLE(SQL_DATABASE)
void ArgumentCoder<DatabaseDetails>::encode(ArgumentEncoder& encoder, const DatabaseDetails& details)
{
    encoder << details.name();
    encoder << details.displayName();
    encoder << details.expectedUsage();
    encoder << details.currentUsage();
    encoder << details.creationTime();
    encoder << details.modificationTime();
}
    
bool ArgumentCoder<DatabaseDetails>::decode(ArgumentDecoder& decoder, DatabaseDetails& details)
{
    String name;
    if (!decoder.decode(name))
        return false;

    String displayName;
    if (!decoder.decode(displayName))
        return false;

    uint64_t expectedUsage;
    if (!decoder.decode(expectedUsage))
        return false;

    uint64_t currentUsage;
    if (!decoder.decode(currentUsage))
        return false;

    double creationTime;
    if (!decoder.decode(creationTime))
        return false;

    double modificationTime;
    if (!decoder.decode(modificationTime))
        return false;

    details = DatabaseDetails(name, displayName, expectedUsage, currentUsage, creationTime, modificationTime);
    return true;
}

#endif

#if PLATFORM(IOS)

static void encodeSharedBuffer(ArgumentEncoder& encoder, SharedBuffer* buffer)
{
    SharedMemory::Handle handle;
    encoder << (buffer ? static_cast<uint64_t>(buffer->size()): 0);
    if (buffer) {
        RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::create(buffer->size());
        memcpy(sharedMemoryBuffer->data(), buffer->data(), buffer->size());
        sharedMemoryBuffer->createHandle(handle, SharedMemory::ReadOnly);
        encoder << handle;
    }
}

static bool decodeSharedBuffer(ArgumentDecoder& decoder, RefPtr<SharedBuffer>& buffer)
{
    uint64_t bufferSize = 0;
    if (!decoder.decode(bufferSize))
        return false;

    if (bufferSize) {
        SharedMemory::Handle handle;
        if (!decoder.decode(handle))
            return false;

        RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::create(handle, SharedMemory::ReadOnly);
        buffer = SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryBuffer->data()), bufferSize);
    }

    return true;
}

void ArgumentCoder<PasteboardWebContent>::encode(ArgumentEncoder& encoder, const WebCore::PasteboardWebContent& content)
{
    encoder << content.canSmartCopyOrDelete;
    encoder << content.dataInStringFormat;

    encodeSharedBuffer(encoder, content.dataInWebArchiveFormat.get());
    encodeSharedBuffer(encoder, content.dataInRTFDFormat.get());
    encodeSharedBuffer(encoder, content.dataInRTFFormat.get());

    encoder << content.clientTypes;
    encoder << static_cast<uint64_t>(content.clientData.size());
    for (size_t i = 0; i < content.clientData.size(); i++)
        encodeSharedBuffer(encoder, content.clientData[i].get());
}

bool ArgumentCoder<PasteboardWebContent>::decode(ArgumentDecoder& decoder, WebCore::PasteboardWebContent& content)
{
    if (!decoder.decode(content.canSmartCopyOrDelete))
        return false;
    if (!decoder.decode(content.dataInStringFormat))
        return false;
    if (!decodeSharedBuffer(decoder, content.dataInWebArchiveFormat))
        return false;
    if (!decodeSharedBuffer(decoder, content.dataInRTFDFormat))
        return false;
    if (!decodeSharedBuffer(decoder, content.dataInRTFFormat))
        return false;
    if (!decoder.decode(content.clientTypes))
        return false;
    uint64_t clientDataSize;
    if (!decoder.decode(clientDataSize))
        return false;
    if (clientDataSize)
        content.clientData.resize(clientDataSize);
    for (size_t i = 0; i < clientDataSize; i++)
        decodeSharedBuffer(decoder, content.clientData[i]);
    return true;
}

void ArgumentCoder<PasteboardImage>::encode(ArgumentEncoder& encoder, const WebCore::PasteboardImage& pasteboardImage)
{
    encodeImage(encoder, pasteboardImage.image.get());
    encoder << pasteboardImage.url.url;
    encoder << pasteboardImage.url.title;
    encoder << pasteboardImage.resourceMIMEType;
    if (pasteboardImage.resourceData)
        encodeSharedBuffer(encoder, pasteboardImage.resourceData.get());
}

bool ArgumentCoder<PasteboardImage>::decode(ArgumentDecoder& decoder, WebCore::PasteboardImage& pasteboardImage)
{
    if (!decodeImage(decoder, pasteboardImage.image))
        return false;
    if (!decoder.decode(pasteboardImage.url.url))
        return false;
    if (!decoder.decode(pasteboardImage.url.title))
        return false;
    if (!decoder.decode(pasteboardImage.resourceMIMEType))
        return false;
    if (!decodeSharedBuffer(decoder, pasteboardImage.resourceData))
        return false;
    return true;
}

#endif

void ArgumentCoder<DictationAlternative>::encode(ArgumentEncoder& encoder, const DictationAlternative& dictationAlternative)
{
    encoder << dictationAlternative.rangeStart;
    encoder << dictationAlternative.rangeLength;
    encoder << dictationAlternative.dictationContext;
}

bool ArgumentCoder<DictationAlternative>::decode(ArgumentDecoder& decoder, DictationAlternative& dictationAlternative)
{
    if (!decoder.decode(dictationAlternative.rangeStart))
        return false;
    if (!decoder.decode(dictationAlternative.rangeLength))
        return false;
    if (!decoder.decode(dictationAlternative.dictationContext))
        return false;
    return true;
}


void ArgumentCoder<FileChooserSettings>::encode(ArgumentEncoder& encoder, const FileChooserSettings& settings)
{
    encoder << settings.allowsMultipleFiles;
    encoder << settings.acceptMIMETypes;
    encoder << settings.selectedFiles;
#if ENABLE(MEDIA_CAPTURE)
    encoder << settings.capture;
#endif
}

bool ArgumentCoder<FileChooserSettings>::decode(ArgumentDecoder& decoder, FileChooserSettings& settings)
{
    if (!decoder.decode(settings.allowsMultipleFiles))
        return false;
    if (!decoder.decode(settings.acceptMIMETypes))
        return false;
    if (!decoder.decode(settings.selectedFiles))
        return false;
#if ENABLE(MEDIA_CAPTURE)
    if (!decoder.decode(settings.capture))
        return false;
#endif

    return true;
}


void ArgumentCoder<GrammarDetail>::encode(ArgumentEncoder& encoder, const GrammarDetail& detail)
{
    encoder << detail.location;
    encoder << detail.length;
    encoder << detail.guesses;
    encoder << detail.userDescription;
}

bool ArgumentCoder<GrammarDetail>::decode(ArgumentDecoder& decoder, GrammarDetail& detail)
{
    if (!decoder.decode(detail.location))
        return false;
    if (!decoder.decode(detail.length))
        return false;
    if (!decoder.decode(detail.guesses))
        return false;
    if (!decoder.decode(detail.userDescription))
        return false;

    return true;
}

void ArgumentCoder<TextCheckingRequestData>::encode(ArgumentEncoder& encoder, const TextCheckingRequestData& request)
{
    encoder << request.sequence();
    encoder << request.text();
    encoder << request.mask();
    encoder.encodeEnum(request.processType());
}

bool ArgumentCoder<TextCheckingRequestData>::decode(ArgumentDecoder& decoder, TextCheckingRequestData& request)
{
    int sequence;
    if (!decoder.decode(sequence))
        return false;

    String text;
    if (!decoder.decode(text))
        return false;

    TextCheckingTypeMask mask;
    if (!decoder.decode(mask))
        return false;

    TextCheckingProcessType processType;
    if (!decoder.decodeEnum(processType))
        return false;

    request = TextCheckingRequestData(sequence, text, mask, processType);
    return true;
}

void ArgumentCoder<TextCheckingResult>::encode(ArgumentEncoder& encoder, const TextCheckingResult& result)
{
    encoder.encodeEnum(result.type);
    encoder << result.location;
    encoder << result.length;
    encoder << result.details;
    encoder << result.replacement;
}

bool ArgumentCoder<TextCheckingResult>::decode(ArgumentDecoder& decoder, TextCheckingResult& result)
{
    if (!decoder.decodeEnum(result.type))
        return false;
    if (!decoder.decode(result.location))
        return false;
    if (!decoder.decode(result.length))
        return false;
    if (!decoder.decode(result.details))
        return false;
    if (!decoder.decode(result.replacement))
        return false;
    return true;
}

void ArgumentCoder<DragSession>::encode(ArgumentEncoder& encoder, const DragSession& result)
{
    encoder.encodeEnum(result.operation);
    encoder << result.mouseIsOverFileInput;
    encoder << result.numberOfItemsToBeAccepted;
}

bool ArgumentCoder<DragSession>::decode(ArgumentDecoder& decoder, DragSession& result)
{
    if (!decoder.decodeEnum(result.operation))
        return false;
    if (!decoder.decode(result.mouseIsOverFileInput))
        return false;
    if (!decoder.decode(result.numberOfItemsToBeAccepted))
        return false;
    return true;
}

void ArgumentCoder<URL>::encode(ArgumentEncoder& encoder, const URL& result)
{
    encoder << result.string();
}
    
bool ArgumentCoder<URL>::decode(ArgumentDecoder& decoder, URL& result)
{
    String urlAsString;
    if (!decoder.decode(urlAsString))
        return false;
    result = URL(WebCore::ParsedURLString, urlAsString);
    return true;
}

void ArgumentCoder<WebCore::UserStyleSheet>::encode(ArgumentEncoder& encoder, const WebCore::UserStyleSheet& userStyleSheet)
{
    encoder << userStyleSheet.source();
    encoder << userStyleSheet.url();
    encoder << userStyleSheet.whitelist();
    encoder << userStyleSheet.blacklist();
    encoder.encodeEnum(userStyleSheet.injectedFrames());
    encoder.encodeEnum(userStyleSheet.level());
}

bool ArgumentCoder<WebCore::UserStyleSheet>::decode(ArgumentDecoder& decoder, WebCore::UserStyleSheet& userStyleSheet)
{
    String source;
    if (!decoder.decode(source))
        return false;

    URL url;
    if (!decoder.decode(url))
        return false;

    Vector<String> whitelist;
    if (!decoder.decode(whitelist))
        return false;

    Vector<String> blacklist;
    if (!decoder.decode(blacklist))
        return false;

    WebCore::UserContentInjectedFrames injectedFrames;
    if (!decoder.decodeEnum(injectedFrames))
        return false;

    WebCore::UserStyleLevel level;
    if (!decoder.decodeEnum(level))
        return false;

    userStyleSheet = WebCore::UserStyleSheet(source, url, whitelist, blacklist, injectedFrames, level);
    return true;
}

void ArgumentCoder<WebCore::UserScript>::encode(ArgumentEncoder& encoder, const WebCore::UserScript& userScript)
{
    encoder << userScript.source();
    encoder << userScript.url();
    encoder << userScript.whitelist();
    encoder << userScript.blacklist();
    encoder.encodeEnum(userScript.injectionTime());
    encoder.encodeEnum(userScript.injectedFrames());
}

bool ArgumentCoder<WebCore::UserScript>::decode(ArgumentDecoder& decoder, WebCore::UserScript& userScript)
{
    String source;
    if (!decoder.decode(source))
        return false;

    URL url;
    if (!decoder.decode(url))
        return false;

    Vector<String> whitelist;
    if (!decoder.decode(whitelist))
        return false;

    Vector<String> blacklist;
    if (!decoder.decode(blacklist))
        return false;

    WebCore::UserScriptInjectionTime injectionTime;
    if (!decoder.decodeEnum(injectionTime))
        return false;

    WebCore::UserContentInjectedFrames injectedFrames;
    if (!decoder.decodeEnum(injectedFrames))
        return false;

    userScript = WebCore::UserScript(source, url, whitelist, blacklist, injectionTime, injectedFrames);
    return true;
}

void ArgumentCoder<WebCore::ScrollableAreaParameters>::encode(ArgumentEncoder& encoder, const WebCore::ScrollableAreaParameters& parameters)
{
    encoder.encodeEnum(parameters.horizontalScrollElasticity);
    encoder.encodeEnum(parameters.verticalScrollElasticity);

    encoder.encodeEnum(parameters.horizontalScrollbarMode);
    encoder.encodeEnum(parameters.verticalScrollbarMode);

    encoder << parameters.hasEnabledHorizontalScrollbar;
    encoder << parameters.hasEnabledVerticalScrollbar;
}

bool ArgumentCoder<WebCore::ScrollableAreaParameters>::decode(ArgumentDecoder& decoder, WebCore::ScrollableAreaParameters& params)
{
    if (!decoder.decodeEnum(params.horizontalScrollElasticity))
        return false;
    if (!decoder.decodeEnum(params.verticalScrollElasticity))
        return false;

    if (!decoder.decodeEnum(params.horizontalScrollbarMode))
        return false;
    if (!decoder.decodeEnum(params.verticalScrollbarMode))
        return false;

    if (!decoder.decode(params.hasEnabledHorizontalScrollbar))
        return false;
    if (!decoder.decode(params.hasEnabledVerticalScrollbar))
        return false;
    
    return true;
}

void ArgumentCoder<WebCore::FixedPositionViewportConstraints>::encode(ArgumentEncoder& encoder, const WebCore::FixedPositionViewportConstraints& viewportConstraints)
{
    encoder << viewportConstraints.alignmentOffset();
    encoder << viewportConstraints.anchorEdges();

    encoder << viewportConstraints.viewportRectAtLastLayout();
    encoder << viewportConstraints.layerPositionAtLastLayout();
}

bool ArgumentCoder<WebCore::FixedPositionViewportConstraints>::decode(ArgumentDecoder& decoder, WebCore::FixedPositionViewportConstraints& viewportConstraints)
{
    FloatSize alignmentOffset;
    if (!decoder.decode(alignmentOffset))
        return false;
    
    ViewportConstraints::AnchorEdges anchorEdges;
    if (!decoder.decode(anchorEdges))
        return false;

    FloatRect viewportRectAtLastLayout;
    if (!decoder.decode(viewportRectAtLastLayout))
        return false;

    FloatPoint layerPositionAtLastLayout;
    if (!decoder.decode(layerPositionAtLastLayout))
        return false;

    viewportConstraints = WebCore::FixedPositionViewportConstraints();
    viewportConstraints.setAlignmentOffset(alignmentOffset);
    viewportConstraints.setAnchorEdges(anchorEdges);

    viewportConstraints.setViewportRectAtLastLayout(viewportRectAtLastLayout);
    viewportConstraints.setLayerPositionAtLastLayout(layerPositionAtLastLayout);
    
    return true;
}

void ArgumentCoder<WebCore::StickyPositionViewportConstraints>::encode(ArgumentEncoder& encoder, const WebCore::StickyPositionViewportConstraints& viewportConstraints)
{
    encoder << viewportConstraints.alignmentOffset();
    encoder << viewportConstraints.anchorEdges();

    encoder << viewportConstraints.leftOffset();
    encoder << viewportConstraints.rightOffset();
    encoder << viewportConstraints.topOffset();
    encoder << viewportConstraints.bottomOffset();

    encoder << viewportConstraints.constrainingRectAtLastLayout();
    encoder << viewportConstraints.containingBlockRect();
    encoder << viewportConstraints.stickyBoxRect();

    encoder << viewportConstraints.stickyOffsetAtLastLayout();
    encoder << viewportConstraints.layerPositionAtLastLayout();
}

bool ArgumentCoder<WebCore::StickyPositionViewportConstraints>::decode(ArgumentDecoder& decoder, WebCore::StickyPositionViewportConstraints& viewportConstraints)
{
    FloatSize alignmentOffset;
    if (!decoder.decode(alignmentOffset))
        return false;
    
    ViewportConstraints::AnchorEdges anchorEdges;
    if (!decoder.decode(anchorEdges))
        return false;
    
    float leftOffset;
    if (!decoder.decode(leftOffset))
        return false;

    float rightOffset;
    if (!decoder.decode(rightOffset))
        return false;

    float topOffset;
    if (!decoder.decode(topOffset))
        return false;

    float bottomOffset;
    if (!decoder.decode(bottomOffset))
        return false;
    
    FloatRect constrainingRectAtLastLayout;
    if (!decoder.decode(constrainingRectAtLastLayout))
        return false;

    FloatRect containingBlockRect;
    if (!decoder.decode(containingBlockRect))
        return false;

    FloatRect stickyBoxRect;
    if (!decoder.decode(stickyBoxRect))
        return false;

    FloatSize stickyOffsetAtLastLayout;
    if (!decoder.decode(stickyOffsetAtLastLayout))
        return false;
    
    FloatPoint layerPositionAtLastLayout;
    if (!decoder.decode(layerPositionAtLastLayout))
        return false;
    
    viewportConstraints = WebCore::StickyPositionViewportConstraints();
    viewportConstraints.setAlignmentOffset(alignmentOffset);
    viewportConstraints.setAnchorEdges(anchorEdges);

    viewportConstraints.setLeftOffset(leftOffset);
    viewportConstraints.setRightOffset(rightOffset);
    viewportConstraints.setTopOffset(topOffset);
    viewportConstraints.setBottomOffset(bottomOffset);
    
    viewportConstraints.setConstrainingRectAtLastLayout(constrainingRectAtLastLayout);
    viewportConstraints.setContainingBlockRect(containingBlockRect);
    viewportConstraints.setStickyBoxRect(stickyBoxRect);

    viewportConstraints.setStickyOffsetAtLastLayout(stickyOffsetAtLastLayout);
    viewportConstraints.setLayerPositionAtLastLayout(layerPositionAtLastLayout);

    return true;
}

#if ENABLE(CSS_FILTERS) && !USE(COORDINATED_GRAPHICS)
static void encodeFilterOperation(ArgumentEncoder& encoder, const FilterOperation& filter)
{
    encoder.encodeEnum(filter.type());

    switch (filter.type()) {
    case FilterOperation::REFERENCE: {
        const auto& referenceFilter = static_cast<const ReferenceFilterOperation&>(filter);
        encoder << referenceFilter.url();
        encoder << referenceFilter.fragment();
        break;
    }
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
        encoder << static_cast<const BasicColorMatrixFilterOperation&>(filter).amount();
        break;
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
        encoder << static_cast<const BasicComponentTransferFilterOperation&>(filter).amount();
        break;
    case FilterOperation::BLUR:
        encoder << static_cast<const BlurFilterOperation&>(filter).stdDeviation();
        break;
    case FilterOperation::DROP_SHADOW: {
        const auto& dropShadowFilter = static_cast<const DropShadowFilterOperation&>(filter);
        encoder << dropShadowFilter.location();
        encoder << dropShadowFilter.stdDeviation();
        encoder << dropShadowFilter.color();
        break;
    }
    case FilterOperation::PASSTHROUGH:
    case FilterOperation::NONE:
        break;
    };
}

static bool decodeFilterOperation(ArgumentDecoder& decoder, RefPtr<FilterOperation>& filter)
{
    FilterOperation::OperationType type;
    if (!decoder.decodeEnum(type))
        return false;

    switch (type) {
    case FilterOperation::REFERENCE: {
        String url;
        String fragment;
        if (!decoder.decode(url))
            return false;
        if (!decoder.decode(fragment))
            return false;
        filter = ReferenceFilterOperation::create(url, fragment, type);
        break;
    }
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE: {
        double amount;
        if (!decoder.decode(amount))
            return false;
        filter = BasicColorMatrixFilterOperation::create(amount, type);
        break;
    }
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST: {
        double amount;
        if (!decoder.decode(amount))
            return false;
        filter = BasicComponentTransferFilterOperation::create(amount, type);
        break;
    }
    case FilterOperation::BLUR: {
        Length stdDeviation;
        if (!decoder.decode(stdDeviation))
            return false;
        filter = BlurFilterOperation::create(stdDeviation, type);
        break;
    }
    case FilterOperation::DROP_SHADOW: {
        IntPoint location;
        int stdDeviation;
        Color color;
        if (!decoder.decode(location))
            return false;
        if (!decoder.decode(stdDeviation))
            return false;
        if (!decoder.decode(color))
            return false;
        filter = DropShadowFilterOperation::create(location, stdDeviation, color, type);
        break;
    }
    case FilterOperation::PASSTHROUGH:
    case FilterOperation::NONE:
        break;
    };

    return true;
}


void ArgumentCoder<FilterOperations>::encode(ArgumentEncoder& encoder, const FilterOperations& filters)
{
    encoder << static_cast<uint64_t>(filters.size());

    for (const auto& filter : filters.operations())
        encodeFilterOperation(encoder, *filter);
}

bool ArgumentCoder<FilterOperations>::decode(ArgumentDecoder& decoder, FilterOperations& filters)
{
    uint64_t filterCount;
    if (!decoder.decode(filterCount))
        return false;

    for (uint64_t i = 0; i < filterCount; ++i) {
        RefPtr<FilterOperation> filter;
        if (!decodeFilterOperation(decoder, filter))
            return false;
        filters.operations().append(std::move(filter));
    }

    return true;
}
#endif // ENABLE(CSS_FILTERS) && !USE(COORDINATED_GRAPHICS)

#if ENABLE(INDEXED_DATABASE)
void ArgumentCoder<IDBDatabaseMetadata>::encode(ArgumentEncoder& encoder, const IDBDatabaseMetadata& metadata)
{
    encoder << metadata.name << metadata.id << metadata.version << metadata.maxObjectStoreId << metadata.objectStores;
}

bool ArgumentCoder<IDBDatabaseMetadata>::decode(ArgumentDecoder& decoder, IDBDatabaseMetadata& metadata)
{
    if (!decoder.decode(metadata.name))
        return false;

    if (!decoder.decode(metadata.id))
        return false;

    if (!decoder.decode(metadata.version))
        return false;

    if (!decoder.decode(metadata.maxObjectStoreId))
        return false;

    if (!decoder.decode(metadata.objectStores))
        return false;

    return true;
}

void ArgumentCoder<IDBIndexMetadata>::encode(ArgumentEncoder& encoder, const IDBIndexMetadata& metadata)
{
    encoder << metadata.name << metadata.id << metadata.keyPath << metadata.unique << metadata.multiEntry;
}

bool ArgumentCoder<IDBIndexMetadata>::decode(ArgumentDecoder& decoder, IDBIndexMetadata& metadata)
{
    if (!decoder.decode(metadata.name))
        return false;

    if (!decoder.decode(metadata.id))
        return false;

    if (!decoder.decode(metadata.keyPath))
        return false;

    if (!decoder.decode(metadata.unique))
        return false;

    if (!decoder.decode(metadata.multiEntry))
        return false;

    return true;
}

void ArgumentCoder<IDBGetResult>::encode(ArgumentEncoder& encoder, const IDBGetResult& result)
{
    bool nullData = !result.valueBuffer;
    encoder << nullData;

    if (!nullData)
        encoder << DataReference(reinterpret_cast<const uint8_t*>(result.valueBuffer->data()), result.valueBuffer->size());

    encoder << result.keyData << result.keyPath;
}

bool ArgumentCoder<IDBGetResult>::decode(ArgumentDecoder& decoder, IDBGetResult& result)
{
    bool nullData;
    if (!decoder.decode(nullData))
        return false;

    if (nullData)
        result.valueBuffer = nullptr;
    else {
        DataReference data;
        if (!decoder.decode(data))
            return false;

        result.valueBuffer = SharedBuffer::create(data.data(), data.size());
    }

    if (!decoder.decode(result.keyData))
        return false;

    if (!decoder.decode(result.keyPath))
        return false;

    return true;
}

void ArgumentCoder<IDBKeyData>::encode(ArgumentEncoder& encoder, const IDBKeyData& keyData)
{
    encoder << keyData.isNull;
    if (keyData.isNull)
        return;

    encoder.encodeEnum(keyData.type);

    switch (keyData.type) {
    case IDBKey::InvalidType:
        break;
    case IDBKey::ArrayType:
        encoder << keyData.arrayValue;
        break;
    case IDBKey::StringType:
        encoder << keyData.stringValue;
        break;
    case IDBKey::DateType:
    case IDBKey::NumberType:
        encoder << keyData.numberValue;
        break;
    case IDBKey::MaxType:
    case IDBKey::MinType:
        // MaxType and MinType are only used for comparison to other keys.
        // They should never be sent across the wire.
        ASSERT_NOT_REACHED();
        break;
    }
}

bool ArgumentCoder<IDBKeyData>::decode(ArgumentDecoder& decoder, IDBKeyData& keyData)
{
    if (!decoder.decode(keyData.isNull))
        return false;

    if (keyData.isNull)
        return true;

    if (!decoder.decodeEnum(keyData.type))
        return false;

    switch (keyData.type) {
    case IDBKey::InvalidType:
        break;
    case IDBKey::ArrayType:
        if (!decoder.decode(keyData.arrayValue))
            return false;
        break;
    case IDBKey::StringType:
        if (!decoder.decode(keyData.stringValue))
            return false;
        break;
    case IDBKey::DateType:
    case IDBKey::NumberType:
        if (!decoder.decode(keyData.numberValue))
            return false;
        break;
    case IDBKey::MaxType:
    case IDBKey::MinType:
        // MaxType and MinType are only used for comparison to other keys.
        // They should never be sent across the wire.
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

void ArgumentCoder<IDBKeyPath>::encode(ArgumentEncoder& encoder, const IDBKeyPath& keyPath)
{
    encoder.encodeEnum(keyPath.type());

    switch (keyPath.type()) {
    case IDBKeyPath::NullType:
        break;
    case IDBKeyPath::StringType:
        encoder << keyPath.string();
        break;
    case IDBKeyPath::ArrayType:
        encoder << keyPath.array();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

bool ArgumentCoder<IDBKeyPath>::decode(ArgumentDecoder& decoder, IDBKeyPath& keyPath)
{
    IDBKeyPath::Type type;
    if (!decoder.decodeEnum(type))
        return false;

    switch (type) {
    case IDBKeyPath::NullType:
        keyPath = IDBKeyPath();
        return true;

    case IDBKeyPath::StringType: {
        String string;
        if (!decoder.decode(string))
            return false;

        keyPath = IDBKeyPath(string);
        return true;
    }
    case IDBKeyPath::ArrayType: {
        Vector<String> array;
        if (!decoder.decode(array))
            return false;

        keyPath = IDBKeyPath(array);
        return true;
    }
    default:
        return false;
    }
}

void ArgumentCoder<IDBKeyRangeData>::encode(ArgumentEncoder& encoder, const IDBKeyRangeData& keyRange)
{
    encoder << keyRange.isNull;
    if (keyRange.isNull)
        return;

    encoder << keyRange.upperKey << keyRange.lowerKey << keyRange.upperOpen << keyRange.lowerOpen;
}

bool ArgumentCoder<IDBKeyRangeData>::decode(ArgumentDecoder& decoder, IDBKeyRangeData& keyRange)
{
    if (!decoder.decode(keyRange.isNull))
        return false;

    if (keyRange.isNull)
        return true;

    if (!decoder.decode(keyRange.upperKey))
        return false;

    if (!decoder.decode(keyRange.lowerKey))
        return false;

    if (!decoder.decode(keyRange.upperOpen))
        return false;

    if (!decoder.decode(keyRange.lowerOpen))
        return false;

    return true;
}

void ArgumentCoder<IDBObjectStoreMetadata>::encode(ArgumentEncoder& encoder, const IDBObjectStoreMetadata& metadata)
{
    encoder << metadata.name << metadata.id << metadata.keyPath << metadata.autoIncrement << metadata.maxIndexId << metadata.indexes;
}

bool ArgumentCoder<IDBObjectStoreMetadata>::decode(ArgumentDecoder& decoder, IDBObjectStoreMetadata& metadata)
{
    if (!decoder.decode(metadata.name))
        return false;

    if (!decoder.decode(metadata.id))
        return false;

    if (!decoder.decode(metadata.keyPath))
        return false;

    if (!decoder.decode(metadata.autoIncrement))
        return false;

    if (!decoder.decode(metadata.maxIndexId))
        return false;

    if (!decoder.decode(metadata.indexes))
        return false;

    return true;
}

#endif // ENABLE(INDEXED_DATABASE)

void ArgumentCoder<SessionID>::encode(ArgumentEncoder& encoder, const SessionID& sessionID)
{
    encoder << sessionID.sessionID();
}

bool ArgumentCoder<SessionID>::decode(ArgumentDecoder& decoder, SessionID& sessionID)
{
    uint64_t session;
    if (!decoder.decode(session))
        return false;

    sessionID = SessionID(session);

    return true;
}

} // namespace IPC
