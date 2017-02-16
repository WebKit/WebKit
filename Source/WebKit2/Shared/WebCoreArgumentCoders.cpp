/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include <WebCore/BlobPart.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Cookie.h>
#include <WebCore/Credential.h>
#include <WebCore/Cursor.h>
#include <WebCore/DatabaseDetails.h>
#include <WebCore/DictationAlternative.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/DragData.h>
#include <WebCore/Editor.h>
#include <WebCore/EventTrackingRegions.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FilterOperation.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/Image.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/Length.h>
#include <WebCore/Path.h>
#include <WebCore/PluginData.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/Region.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/SearchPopupMenu.h>
#include <WebCore/SessionID.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TimingFunction.h>
#include <WebCore/TransformationMatrix.h>
#include <WebCore/URL.h>
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#include "ArgumentCodersMac.h"
#endif

#if PLATFORM(IOS)
#include <WebCore/FloatQuad.h>
#include <WebCore/InspectorOverlay.h>
#include <WebCore/Pasteboard.h>
#include <WebCore/SelectionRect.h>
#include <WebCore/SharedBuffer.h>
#endif // PLATFORM(IOS)

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include <WebCore/MediaPlaybackTargetContext.h>
#endif

#if ENABLE(MEDIA_SESSION)
#include <WebCore/MediaSessionMetadata.h>
#endif

#if ENABLE(MEDIA_STREAM)
#include <WebCore/CaptureDevice.h>
#include <WebCore/MediaConstraintsImpl.h>
#endif

using namespace WebCore;
using namespace WebKit;

namespace IPC {

void ArgumentCoder<MonotonicTime>::encode(Encoder& encoder, const MonotonicTime& time)
{
    encoder << time.secondsSinceEpoch().value();
}

bool ArgumentCoder<MonotonicTime>::decode(Decoder& decoder, MonotonicTime& time)
{
    double value;
    if (!decoder.decode(value))
        return false;

    time = MonotonicTime::fromRawSeconds(value);
    return true;
}

void ArgumentCoder<Seconds>::encode(Encoder& encoder, const Seconds& seconds)
{
    encoder << seconds.value();
}

bool ArgumentCoder<Seconds>::decode(Decoder& decoder, Seconds& seconds)
{
    double value;
    if (!decoder.decode(value))
        return false;

    seconds = Seconds(value);
    return true;
}

void ArgumentCoder<AffineTransform>::encode(Encoder& encoder, const AffineTransform& affineTransform)
{
    SimpleArgumentCoder<AffineTransform>::encode(encoder, affineTransform);
}

bool ArgumentCoder<AffineTransform>::decode(Decoder& decoder, AffineTransform& affineTransform)
{
    return SimpleArgumentCoder<AffineTransform>::decode(decoder, affineTransform);
}

void ArgumentCoder<EventTrackingRegions>::encode(Encoder& encoder, const EventTrackingRegions& eventTrackingRegions)
{
    encoder << eventTrackingRegions.asynchronousDispatchRegion;
    encoder << eventTrackingRegions.eventSpecificSynchronousDispatchRegions;
}

bool ArgumentCoder<EventTrackingRegions>::decode(Decoder& decoder, EventTrackingRegions& eventTrackingRegions)
{
    Region asynchronousDispatchRegion;
    if (!decoder.decode(asynchronousDispatchRegion))
        return false;
    HashMap<String, Region> eventSpecificSynchronousDispatchRegions;
    if (!decoder.decode(eventSpecificSynchronousDispatchRegions))
        return false;
    eventTrackingRegions.asynchronousDispatchRegion = WTFMove(asynchronousDispatchRegion);
    eventTrackingRegions.eventSpecificSynchronousDispatchRegions = WTFMove(eventSpecificSynchronousDispatchRegions);
    return true;
}

void ArgumentCoder<TransformationMatrix>::encode(Encoder& encoder, const TransformationMatrix& transformationMatrix)
{
    encoder << transformationMatrix.m11();
    encoder << transformationMatrix.m12();
    encoder << transformationMatrix.m13();
    encoder << transformationMatrix.m14();

    encoder << transformationMatrix.m21();
    encoder << transformationMatrix.m22();
    encoder << transformationMatrix.m23();
    encoder << transformationMatrix.m24();

    encoder << transformationMatrix.m31();
    encoder << transformationMatrix.m32();
    encoder << transformationMatrix.m33();
    encoder << transformationMatrix.m34();

    encoder << transformationMatrix.m41();
    encoder << transformationMatrix.m42();
    encoder << transformationMatrix.m43();
    encoder << transformationMatrix.m44();
}

bool ArgumentCoder<TransformationMatrix>::decode(Decoder& decoder, TransformationMatrix& transformationMatrix)
{
    double m11;
    if (!decoder.decode(m11))
        return false;
    double m12;
    if (!decoder.decode(m12))
        return false;
    double m13;
    if (!decoder.decode(m13))
        return false;
    double m14;
    if (!decoder.decode(m14))
        return false;

    double m21;
    if (!decoder.decode(m21))
        return false;
    double m22;
    if (!decoder.decode(m22))
        return false;
    double m23;
    if (!decoder.decode(m23))
        return false;
    double m24;
    if (!decoder.decode(m24))
        return false;

    double m31;
    if (!decoder.decode(m31))
        return false;
    double m32;
    if (!decoder.decode(m32))
        return false;
    double m33;
    if (!decoder.decode(m33))
        return false;
    double m34;
    if (!decoder.decode(m34))
        return false;

    double m41;
    if (!decoder.decode(m41))
        return false;
    double m42;
    if (!decoder.decode(m42))
        return false;
    double m43;
    if (!decoder.decode(m43))
        return false;
    double m44;
    if (!decoder.decode(m44))
        return false;

    transformationMatrix.setMatrix(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44);
    return true;
}

void ArgumentCoder<LinearTimingFunction>::encode(Encoder& encoder, const LinearTimingFunction& timingFunction)
{
    encoder.encodeEnum(timingFunction.type());
}

bool ArgumentCoder<LinearTimingFunction>::decode(Decoder&, LinearTimingFunction&)
{
    // Type is decoded by the caller. Nothing else to decode.
    return true;
}

void ArgumentCoder<CubicBezierTimingFunction>::encode(Encoder& encoder, const CubicBezierTimingFunction& timingFunction)
{
    encoder.encodeEnum(timingFunction.type());
    
    encoder << timingFunction.x1();
    encoder << timingFunction.y1();
    encoder << timingFunction.x2();
    encoder << timingFunction.y2();
    
    encoder.encodeEnum(timingFunction.timingFunctionPreset());
}

bool ArgumentCoder<CubicBezierTimingFunction>::decode(Decoder& decoder, CubicBezierTimingFunction& timingFunction)
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

void ArgumentCoder<StepsTimingFunction>::encode(Encoder& encoder, const StepsTimingFunction& timingFunction)
{
    encoder.encodeEnum(timingFunction.type());
    
    encoder << timingFunction.numberOfSteps();
    encoder << timingFunction.stepAtStart();
}

bool ArgumentCoder<StepsTimingFunction>::decode(Decoder& decoder, StepsTimingFunction& timingFunction)
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

void ArgumentCoder<SpringTimingFunction>::encode(Encoder& encoder, const SpringTimingFunction& timingFunction)
{
    encoder.encodeEnum(timingFunction.type());
    
    encoder << timingFunction.mass();
    encoder << timingFunction.stiffness();
    encoder << timingFunction.damping();
    encoder << timingFunction.initialVelocity();
}

bool ArgumentCoder<SpringTimingFunction>::decode(Decoder& decoder, SpringTimingFunction& timingFunction)
{
    // Type is decoded by the caller.
    double mass;
    if (!decoder.decode(mass))
        return false;

    double stiffness;
    if (!decoder.decode(stiffness))
        return false;

    double damping;
    if (!decoder.decode(damping))
        return false;

    double initialVelocity;
    if (!decoder.decode(initialVelocity))
        return false;

    timingFunction.setValues(mass, stiffness, damping, initialVelocity);

    return true;
}

void ArgumentCoder<FloatPoint>::encode(Encoder& encoder, const FloatPoint& floatPoint)
{
    SimpleArgumentCoder<FloatPoint>::encode(encoder, floatPoint);
}

bool ArgumentCoder<FloatPoint>::decode(Decoder& decoder, FloatPoint& floatPoint)
{
    return SimpleArgumentCoder<FloatPoint>::decode(decoder, floatPoint);
}


void ArgumentCoder<FloatPoint3D>::encode(Encoder& encoder, const FloatPoint3D& floatPoint)
{
    SimpleArgumentCoder<FloatPoint3D>::encode(encoder, floatPoint);
}

bool ArgumentCoder<FloatPoint3D>::decode(Decoder& decoder, FloatPoint3D& floatPoint)
{
    return SimpleArgumentCoder<FloatPoint3D>::decode(decoder, floatPoint);
}


void ArgumentCoder<FloatRect>::encode(Encoder& encoder, const FloatRect& floatRect)
{
    SimpleArgumentCoder<FloatRect>::encode(encoder, floatRect);
}

bool ArgumentCoder<FloatRect>::decode(Decoder& decoder, FloatRect& floatRect)
{
    return SimpleArgumentCoder<FloatRect>::decode(decoder, floatRect);
}


void ArgumentCoder<FloatSize>::encode(Encoder& encoder, const FloatSize& floatSize)
{
    SimpleArgumentCoder<FloatSize>::encode(encoder, floatSize);
}

bool ArgumentCoder<FloatSize>::decode(Decoder& decoder, FloatSize& floatSize)
{
    return SimpleArgumentCoder<FloatSize>::decode(decoder, floatSize);
}


void ArgumentCoder<FloatRoundedRect>::encode(Encoder& encoder, const FloatRoundedRect& roundedRect)
{
    SimpleArgumentCoder<FloatRoundedRect>::encode(encoder, roundedRect);
}

bool ArgumentCoder<FloatRoundedRect>::decode(Decoder& decoder, FloatRoundedRect& roundedRect)
{
    return SimpleArgumentCoder<FloatRoundedRect>::decode(decoder, roundedRect);
}

#if PLATFORM(IOS)
void ArgumentCoder<FloatQuad>::encode(Encoder& encoder, const FloatQuad& floatQuad)
{
    SimpleArgumentCoder<FloatQuad>::encode(encoder, floatQuad);
}

bool ArgumentCoder<FloatQuad>::decode(Decoder& decoder, FloatQuad& floatQuad)
{
    return SimpleArgumentCoder<FloatQuad>::decode(decoder, floatQuad);
}

void ArgumentCoder<ViewportArguments>::encode(Encoder& encoder, const ViewportArguments& viewportArguments)
{
    SimpleArgumentCoder<ViewportArguments>::encode(encoder, viewportArguments);
}

bool ArgumentCoder<ViewportArguments>::decode(Decoder& decoder, ViewportArguments& viewportArguments)
{
    return SimpleArgumentCoder<ViewportArguments>::decode(decoder, viewportArguments);
}
#endif // PLATFORM(IOS)


void ArgumentCoder<IntPoint>::encode(Encoder& encoder, const IntPoint& intPoint)
{
    SimpleArgumentCoder<IntPoint>::encode(encoder, intPoint);
}

bool ArgumentCoder<IntPoint>::decode(Decoder& decoder, IntPoint& intPoint)
{
    return SimpleArgumentCoder<IntPoint>::decode(decoder, intPoint);
}


void ArgumentCoder<IntRect>::encode(Encoder& encoder, const IntRect& intRect)
{
    SimpleArgumentCoder<IntRect>::encode(encoder, intRect);
}

bool ArgumentCoder<IntRect>::decode(Decoder& decoder, IntRect& intRect)
{
    return SimpleArgumentCoder<IntRect>::decode(decoder, intRect);
}


void ArgumentCoder<IntSize>::encode(Encoder& encoder, const IntSize& intSize)
{
    SimpleArgumentCoder<IntSize>::encode(encoder, intSize);
}

bool ArgumentCoder<IntSize>::decode(Decoder& decoder, IntSize& intSize)
{
    return SimpleArgumentCoder<IntSize>::decode(decoder, intSize);
}


void ArgumentCoder<LayoutSize>::encode(Encoder& encoder, const LayoutSize& layoutSize)
{
    SimpleArgumentCoder<LayoutSize>::encode(encoder, layoutSize);
}

bool ArgumentCoder<LayoutSize>::decode(Decoder& decoder, LayoutSize& layoutSize)
{
    return SimpleArgumentCoder<LayoutSize>::decode(decoder, layoutSize);
}


void ArgumentCoder<LayoutPoint>::encode(Encoder& encoder, const LayoutPoint& layoutPoint)
{
    SimpleArgumentCoder<LayoutPoint>::encode(encoder, layoutPoint);
}

bool ArgumentCoder<LayoutPoint>::decode(Decoder& decoder, LayoutPoint& layoutPoint)
{
    return SimpleArgumentCoder<LayoutPoint>::decode(decoder, layoutPoint);
}


static void pathEncodeApplierFunction(Encoder& encoder, const PathElement& element)
{
    encoder.encodeEnum(element.type);

    switch (element.type) {
    case PathElementMoveToPoint: // The points member will contain 1 value.
        encoder << element.points[0];
        break;
    case PathElementAddLineToPoint: // The points member will contain 1 value.
        encoder << element.points[0];
        break;
    case PathElementAddQuadCurveToPoint: // The points member will contain 2 values.
        encoder << element.points[0];
        encoder << element.points[1];
        break;
    case PathElementAddCurveToPoint: // The points member will contain 3 values.
        encoder << element.points[0];
        encoder << element.points[1];
        encoder << element.points[2];
        break;
    case PathElementCloseSubpath: // The points member will contain no values.
        break;
    }
}

void ArgumentCoder<Path>::encode(Encoder& encoder, const Path& path)
{
    uint64_t numPoints = 0;
    path.apply([&numPoints](const PathElement&) {
        ++numPoints;
    });

    encoder << numPoints;

    path.apply([&encoder](const PathElement& pathElement) {
        pathEncodeApplierFunction(encoder, pathElement);
    });
}

bool ArgumentCoder<Path>::decode(Decoder& decoder, Path& path)
{
    uint64_t numPoints;
    if (!decoder.decode(numPoints))
        return false;
    
    path.clear();

    for (uint64_t i = 0; i < numPoints; ++i) {
    
        PathElementType elementType;
        if (!decoder.decodeEnum(elementType))
            return false;
        
        switch (elementType) {
        case PathElementMoveToPoint: { // The points member will contain 1 value.
            FloatPoint point;
            if (!decoder.decode(point))
                return false;
            path.moveTo(point);
            break;
        }
        case PathElementAddLineToPoint: { // The points member will contain 1 value.
            FloatPoint point;
            if (!decoder.decode(point))
                return false;
            path.addLineTo(point);
            break;
        }
        case PathElementAddQuadCurveToPoint: { // The points member will contain 2 values.
            FloatPoint controlPoint;
            if (!decoder.decode(controlPoint))
                return false;

            FloatPoint endPoint;
            if (!decoder.decode(endPoint))
                return false;

            path.addQuadCurveTo(controlPoint, endPoint);
            break;
        }
        case PathElementAddCurveToPoint: { // The points member will contain 3 values.
            FloatPoint controlPoint1;
            if (!decoder.decode(controlPoint1))
                return false;

            FloatPoint controlPoint2;
            if (!decoder.decode(controlPoint2))
                return false;

            FloatPoint endPoint;
            if (!decoder.decode(endPoint))
                return false;

            path.addBezierCurveTo(controlPoint1, controlPoint2, endPoint);
            break;
        }
        case PathElementCloseSubpath: // The points member will contain no values.
            path.closeSubpath();
            break;
        }
    }

    return true;
}

void ArgumentCoder<RecentSearch>::encode(Encoder& encoder, const RecentSearch& recentSearch)
{
    encoder << recentSearch.string << recentSearch.time;
}

bool ArgumentCoder<RecentSearch>::decode(Decoder& decoder, RecentSearch& recentSearch)
{
    if (!decoder.decode(recentSearch.string))
        return false;

    if (!decoder.decode(recentSearch.time))
        return false;

    return true;
}

template<> struct ArgumentCoder<Region::Span> {
    static void encode(Encoder&, const Region::Span&);
    static bool decode(Decoder&, Region::Span&);
};

void ArgumentCoder<Region::Span>::encode(Encoder& encoder, const Region::Span& span)
{
    encoder << span.y;
    encoder << (uint64_t)span.segmentIndex;
}

bool ArgumentCoder<Region::Span>::decode(Decoder& decoder, Region::Span& span)
{
    if (!decoder.decode(span.y))
        return false;
    
    uint64_t segmentIndex;
    if (!decoder.decode(segmentIndex))
        return false;
    
    span.segmentIndex = segmentIndex;
    return true;
}

void ArgumentCoder<Region>::encode(Encoder& encoder, const Region& region)
{
    encoder.encode(region.shapeSegments());
    encoder.encode(region.shapeSpans());
}

bool ArgumentCoder<Region>::decode(Decoder& decoder, Region& region)
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

void ArgumentCoder<Length>::encode(Encoder& encoder, const Length& length)
{
    SimpleArgumentCoder<Length>::encode(encoder, length);
}

bool ArgumentCoder<Length>::decode(Decoder& decoder, Length& length)
{
    return SimpleArgumentCoder<Length>::decode(decoder, length);
}


void ArgumentCoder<ViewportAttributes>::encode(Encoder& encoder, const ViewportAttributes& viewportAttributes)
{
    SimpleArgumentCoder<ViewportAttributes>::encode(encoder, viewportAttributes);
}

bool ArgumentCoder<ViewportAttributes>::decode(Decoder& decoder, ViewportAttributes& viewportAttributes)
{
    return SimpleArgumentCoder<ViewportAttributes>::decode(decoder, viewportAttributes);
}


void ArgumentCoder<MimeClassInfo>::encode(Encoder& encoder, const MimeClassInfo& mimeClassInfo)
{
    encoder << mimeClassInfo.type << mimeClassInfo.desc << mimeClassInfo.extensions;
}

bool ArgumentCoder<MimeClassInfo>::decode(Decoder& decoder, MimeClassInfo& mimeClassInfo)
{
    if (!decoder.decode(mimeClassInfo.type))
        return false;
    if (!decoder.decode(mimeClassInfo.desc))
        return false;
    if (!decoder.decode(mimeClassInfo.extensions))
        return false;

    return true;
}


void ArgumentCoder<PluginInfo>::encode(Encoder& encoder, const PluginInfo& pluginInfo)
{
    encoder << pluginInfo.name;
    encoder << pluginInfo.file;
    encoder << pluginInfo.desc;
    encoder << pluginInfo.mimes;
    encoder << pluginInfo.isApplicationPlugin;
    encoder.encodeEnum(pluginInfo.clientLoadPolicy);
#if PLATFORM(MAC)
    encoder << pluginInfo.bundleIdentifier;
    encoder << pluginInfo.versionString;
#endif
}

bool ArgumentCoder<PluginInfo>::decode(Decoder& decoder, PluginInfo& pluginInfo)
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
    if (!decoder.decodeEnum(pluginInfo.clientLoadPolicy))
        return false;
#if PLATFORM(MAC)
    if (!decoder.decode(pluginInfo.bundleIdentifier))
        return false;
    if (!decoder.decode(pluginInfo.versionString))
        return false;
#endif

    return true;
}

void ArgumentCoder<AuthenticationChallenge>::encode(Encoder& encoder, const AuthenticationChallenge& challenge)
{
    encoder << challenge.protectionSpace() << challenge.proposedCredential() << challenge.previousFailureCount() << challenge.failureResponse() << challenge.error();
}

bool ArgumentCoder<AuthenticationChallenge>::decode(Decoder& decoder, AuthenticationChallenge& challenge)
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


void ArgumentCoder<ProtectionSpace>::encode(Encoder& encoder, const ProtectionSpace& space)
{
    if (space.encodingRequiresPlatformData()) {
        encoder << true;
        encodePlatformData(encoder, space);
        return;
    }

    encoder << false;
    encoder << space.host() << space.port() << space.realm();
    encoder.encodeEnum(space.authenticationScheme());
    encoder.encodeEnum(space.serverType());
}

bool ArgumentCoder<ProtectionSpace>::decode(Decoder& decoder, ProtectionSpace& space)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    if (hasPlatformData)
        return decodePlatformData(decoder, space);

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

void ArgumentCoder<Credential>::encode(Encoder& encoder, const Credential& credential)
{
    if (credential.encodingRequiresPlatformData()) {
        encoder << true;
        encodePlatformData(encoder, credential);
        return;
    }

    encoder << false;
    encoder << credential.user() << credential.password();
    encoder.encodeEnum(credential.persistence());
}

bool ArgumentCoder<Credential>::decode(Decoder& decoder, Credential& credential)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    if (hasPlatformData)
        return decodePlatformData(decoder, credential);

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

static void encodeImage(Encoder& encoder, Image& image)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(IntSize(image.size()), ShareableBitmap::SupportsAlpha);
    bitmap->createGraphicsContext()->drawImage(image, IntPoint());

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);

    encoder << handle;
}

static bool decodeImage(Decoder& decoder, RefPtr<Image>& image)
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

static void encodeOptionalImage(Encoder& encoder, Image* image)
{
    bool hasImage = !!image;
    encoder << hasImage;

    if (hasImage)
        encodeImage(encoder, *image);
}

static bool decodeOptionalImage(Decoder& decoder, RefPtr<Image>& image)
{
    image = nullptr;

    bool hasImage;
    if (!decoder.decode(hasImage))
        return false;

    if (!hasImage)
        return true;

    return decodeImage(decoder, image);
}

#if !PLATFORM(IOS)
void ArgumentCoder<Cursor>::encode(Encoder& encoder, const Cursor& cursor)
{
    encoder.encodeEnum(cursor.type());
        
    if (cursor.type() != Cursor::Custom)
        return;

    if (cursor.image()->isNull()) {
        encoder << false; // There is no valid image being encoded.
        return;
    }

    encoder << true;
    encodeImage(encoder, *cursor.image());
    encoder << cursor.hotSpot();
#if ENABLE(MOUSE_CURSOR_SCALE)
    encoder << cursor.imageScaleFactor();
#endif
}

bool ArgumentCoder<Cursor>::decode(Decoder& decoder, Cursor& cursor)
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

#if ENABLE(MOUSE_CURSOR_SCALE)
    float scale;
    if (!decoder.decode(scale))
        return false;

    cursor = Cursor(image.get(), hotSpot, scale);
#else
    cursor = Cursor(image.get(), hotSpot);
#endif
    return true;
}
#endif

void ArgumentCoder<ResourceRequest>::encode(Encoder& encoder, const ResourceRequest& resourceRequest)
{
    encoder << resourceRequest.cachePartition();
    encoder << resourceRequest.hiddenFromInspector();

    if (resourceRequest.encodingRequiresPlatformData()) {
        encoder << true;
        encodePlatformData(encoder, resourceRequest);
        return;
    }
    encoder << false;
    resourceRequest.encodeWithoutPlatformData(encoder);
}

bool ArgumentCoder<ResourceRequest>::decode(Decoder& decoder, ResourceRequest& resourceRequest)
{
    String cachePartition;
    if (!decoder.decode(cachePartition))
        return false;
    resourceRequest.setCachePartition(cachePartition);

    bool isHiddenFromInspector;
    if (!decoder.decode(isHiddenFromInspector))
        return false;
    resourceRequest.setHiddenFromInspector(isHiddenFromInspector);

    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;
    if (hasPlatformData)
        return decodePlatformData(decoder, resourceRequest);

    return resourceRequest.decodeWithoutPlatformData(decoder);
}

void ArgumentCoder<ResourceError>::encode(Encoder& encoder, const ResourceError& resourceError)
{
    encodePlatformData(encoder, resourceError);
}

bool ArgumentCoder<ResourceError>::decode(Decoder& decoder, ResourceError& resourceError)
{
    return decodePlatformData(decoder, resourceError);
}

#if PLATFORM(IOS)

void ArgumentCoder<SelectionRect>::encode(Encoder& encoder, const SelectionRect& selectionRect)
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

bool ArgumentCoder<SelectionRect>::decode(Decoder& decoder, SelectionRect& selectionRect)
{
    IntRect rect;
    if (!decoder.decode(rect))
        return false;
    selectionRect.setRect(rect);

    uint32_t direction;
    if (!decoder.decode(direction))
        return false;
    selectionRect.setDirection((TextDirection)direction);

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

void ArgumentCoder<WindowFeatures>::encode(Encoder& encoder, const WindowFeatures& windowFeatures)
{
    encoder << windowFeatures.x;
    encoder << windowFeatures.y;
    encoder << windowFeatures.width;
    encoder << windowFeatures.height;
    encoder << windowFeatures.menuBarVisible;
    encoder << windowFeatures.statusBarVisible;
    encoder << windowFeatures.toolBarVisible;
    encoder << windowFeatures.locationBarVisible;
    encoder << windowFeatures.scrollbarsVisible;
    encoder << windowFeatures.resizable;
    encoder << windowFeatures.fullscreen;
    encoder << windowFeatures.dialog;
}

bool ArgumentCoder<WindowFeatures>::decode(Decoder& decoder, WindowFeatures& windowFeatures)
{
    if (!decoder.decode(windowFeatures.x))
        return false;
    if (!decoder.decode(windowFeatures.y))
        return false;
    if (!decoder.decode(windowFeatures.width))
        return false;
    if (!decoder.decode(windowFeatures.height))
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


void ArgumentCoder<Color>::encode(Encoder& encoder, const Color& color)
{
    if (color.isExtended()) {
        encoder << true;
        encoder << color.asExtended().red();
        encoder << color.asExtended().green();
        encoder << color.asExtended().blue();
        encoder << color.asExtended().alpha();
        encoder << color.asExtended().colorSpace();
        return;
    }

    encoder << false;

    if (!color.isValid()) {
        encoder << false;
        return;
    }

    encoder << true;
    encoder << color.rgb();
}

bool ArgumentCoder<Color>::decode(Decoder& decoder, Color& color)
{
    bool isExtended;
    if (!decoder.decode(isExtended))
        return false;

    if (isExtended) {
        float red;
        float green;
        float blue;
        float alpha;
        ColorSpace colorSpace;
        if (!decoder.decode(red))
            return false;
        if (!decoder.decode(green))
            return false;
        if (!decoder.decode(blue))
            return false;
        if (!decoder.decode(alpha))
            return false;
        if (!decoder.decode(colorSpace))
            return false;
        color = Color(red, green, blue, alpha, colorSpace);
        return true;
    }

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

#if ENABLE(DRAG_SUPPORT)
void ArgumentCoder<DragData>::encode(Encoder& encoder, const DragData& dragData)
{
    encoder << dragData.clientPosition();
    encoder << dragData.globalPosition();
    encoder.encodeEnum(dragData.draggingSourceOperationMask());
    encoder.encodeEnum(dragData.flags());
#if PLATFORM(COCOA)
    encoder << dragData.pasteboardName();
#endif
#if PLATFORM(MAC)
    encoder << dragData.fileNames();
#endif
}

bool ArgumentCoder<DragData>::decode(Decoder& decoder, DragData& dragData)
{
    IntPoint clientPosition;
    if (!decoder.decode(clientPosition))
        return false;

    IntPoint globalPosition;
    if (!decoder.decode(globalPosition))
        return false;

    DragOperation draggingSourceOperationMask;
    if (!decoder.decodeEnum(draggingSourceOperationMask))
        return false;

    DragApplicationFlags applicationFlags;
    if (!decoder.decodeEnum(applicationFlags))
        return false;

    String pasteboardName;
#if PLATFORM(COCOA)
    if (!decoder.decode(pasteboardName))
        return false;
#endif
    Vector<String> fileNames;
#if PLATFORM(MAC)
    if (!decoder.decode(fileNames))
        return false;
#endif

    dragData = DragData(pasteboardName, clientPosition, globalPosition, draggingSourceOperationMask, applicationFlags);
    dragData.setFileNames(fileNames);

    return true;
}
#endif

void ArgumentCoder<CompositionUnderline>::encode(Encoder& encoder, const CompositionUnderline& underline)
{
    encoder << underline.startOffset;
    encoder << underline.endOffset;
    encoder << underline.thick;
    encoder << underline.color;
}

bool ArgumentCoder<CompositionUnderline>::decode(Decoder& decoder, CompositionUnderline& underline)
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


void ArgumentCoder<Cookie>::encode(Encoder& encoder, const Cookie& cookie)
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

bool ArgumentCoder<Cookie>::decode(Decoder& decoder, Cookie& cookie)
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

void ArgumentCoder<DatabaseDetails>::encode(Encoder& encoder, const DatabaseDetails& details)
{
    encoder << details.name();
    encoder << details.displayName();
    encoder << details.expectedUsage();
    encoder << details.currentUsage();
    encoder << details.creationTime();
    encoder << details.modificationTime();
}
    
bool ArgumentCoder<DatabaseDetails>::decode(Decoder& decoder, DatabaseDetails& details)
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

#if PLATFORM(IOS)

void ArgumentCoder<Highlight>::encode(Encoder& encoder, const Highlight& highlight)
{
    encoder << static_cast<uint32_t>(highlight.type);
    encoder << highlight.usePageCoordinates;
    encoder << highlight.contentColor;
    encoder << highlight.contentOutlineColor;
    encoder << highlight.paddingColor;
    encoder << highlight.borderColor;
    encoder << highlight.marginColor;
    encoder << highlight.quads;
}

bool ArgumentCoder<Highlight>::decode(Decoder& decoder, Highlight& highlight)
{
    uint32_t type;
    if (!decoder.decode(type))
        return false;
    highlight.type = (HighlightType)type;

    if (!decoder.decode(highlight.usePageCoordinates))
        return false;
    if (!decoder.decode(highlight.contentColor))
        return false;
    if (!decoder.decode(highlight.contentOutlineColor))
        return false;
    if (!decoder.decode(highlight.paddingColor))
        return false;
    if (!decoder.decode(highlight.borderColor))
        return false;
    if (!decoder.decode(highlight.marginColor))
        return false;
    if (!decoder.decode(highlight.quads))
        return false;
    return true;
}

static void encodeSharedBuffer(Encoder& encoder, SharedBuffer* buffer)
{
    SharedMemory::Handle handle;
    encoder << (buffer ? static_cast<uint64_t>(buffer->size()): 0);
    if (buffer) {
        RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(buffer->size());
        memcpy(sharedMemoryBuffer->data(), buffer->data(), buffer->size());
        sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
        encoder << handle;
    }
}

static bool decodeSharedBuffer(Decoder& decoder, RefPtr<SharedBuffer>& buffer)
{
    uint64_t bufferSize = 0;
    if (!decoder.decode(bufferSize))
        return false;

    if (bufferSize) {
        SharedMemory::Handle handle;
        if (!decoder.decode(handle))
            return false;

        RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
        buffer = SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryBuffer->data()), bufferSize);
    }

    return true;
}

void ArgumentCoder<PasteboardWebContent>::encode(Encoder& encoder, const PasteboardWebContent& content)
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

bool ArgumentCoder<PasteboardWebContent>::decode(Decoder& decoder, PasteboardWebContent& content)
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

void ArgumentCoder<PasteboardImage>::encode(Encoder& encoder, const PasteboardImage& pasteboardImage)
{
    encodeOptionalImage(encoder, pasteboardImage.image.get());
    encoder << pasteboardImage.url.url;
    encoder << pasteboardImage.url.title;
    encoder << pasteboardImage.resourceMIMEType;
    if (pasteboardImage.resourceData)
        encodeSharedBuffer(encoder, pasteboardImage.resourceData.get());
}

bool ArgumentCoder<PasteboardImage>::decode(Decoder& decoder, PasteboardImage& pasteboardImage)
{
    if (!decodeOptionalImage(decoder, pasteboardImage.image))
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

void ArgumentCoder<DictationAlternative>::encode(Encoder& encoder, const DictationAlternative& dictationAlternative)
{
    encoder << dictationAlternative.rangeStart;
    encoder << dictationAlternative.rangeLength;
    encoder << dictationAlternative.dictationContext;
}

bool ArgumentCoder<DictationAlternative>::decode(Decoder& decoder, DictationAlternative& dictationAlternative)
{
    if (!decoder.decode(dictationAlternative.rangeStart))
        return false;
    if (!decoder.decode(dictationAlternative.rangeLength))
        return false;
    if (!decoder.decode(dictationAlternative.dictationContext))
        return false;
    return true;
}


void ArgumentCoder<FileChooserSettings>::encode(Encoder& encoder, const FileChooserSettings& settings)
{
    encoder << settings.allowsMultipleFiles;
    encoder << settings.acceptMIMETypes;
    encoder << settings.selectedFiles;
#if ENABLE(MEDIA_CAPTURE)
    encoder.encodeEnum(settings.mediaCaptureType);
#endif
}

bool ArgumentCoder<FileChooserSettings>::decode(Decoder& decoder, FileChooserSettings& settings)
{
    if (!decoder.decode(settings.allowsMultipleFiles))
        return false;
    if (!decoder.decode(settings.acceptMIMETypes))
        return false;
    if (!decoder.decode(settings.selectedFiles))
        return false;
#if ENABLE(MEDIA_CAPTURE)
    if (!decoder.decodeEnum(settings.mediaCaptureType))
        return false;
#endif

    return true;
}


void ArgumentCoder<GrammarDetail>::encode(Encoder& encoder, const GrammarDetail& detail)
{
    encoder << detail.location;
    encoder << detail.length;
    encoder << detail.guesses;
    encoder << detail.userDescription;
}

bool ArgumentCoder<GrammarDetail>::decode(Decoder& decoder, GrammarDetail& detail)
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

void ArgumentCoder<TextCheckingRequestData>::encode(Encoder& encoder, const TextCheckingRequestData& request)
{
    encoder << request.sequence();
    encoder << request.text();
    encoder << request.mask();
    encoder.encodeEnum(request.processType());
}

bool ArgumentCoder<TextCheckingRequestData>::decode(Decoder& decoder, TextCheckingRequestData& request)
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

void ArgumentCoder<TextCheckingResult>::encode(Encoder& encoder, const TextCheckingResult& result)
{
    encoder.encodeEnum(result.type);
    encoder << result.location;
    encoder << result.length;
    encoder << result.details;
    encoder << result.replacement;
}

bool ArgumentCoder<TextCheckingResult>::decode(Decoder& decoder, TextCheckingResult& result)
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

void ArgumentCoder<URL>::encode(Encoder& encoder, const URL& result)
{
    encoder << result.string();
}
    
bool ArgumentCoder<URL>::decode(Decoder& decoder, URL& result)
{
    String urlAsString;
    if (!decoder.decode(urlAsString))
        return false;
    result = URL(ParsedURLString, urlAsString);
    return true;
}

void ArgumentCoder<UserStyleSheet>::encode(Encoder& encoder, const UserStyleSheet& userStyleSheet)
{
    encoder << userStyleSheet.source();
    encoder << userStyleSheet.url();
    encoder << userStyleSheet.whitelist();
    encoder << userStyleSheet.blacklist();
    encoder.encodeEnum(userStyleSheet.injectedFrames());
    encoder.encodeEnum(userStyleSheet.level());
}

bool ArgumentCoder<UserStyleSheet>::decode(Decoder& decoder, UserStyleSheet& userStyleSheet)
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

    UserContentInjectedFrames injectedFrames;
    if (!decoder.decodeEnum(injectedFrames))
        return false;

    UserStyleLevel level;
    if (!decoder.decodeEnum(level))
        return false;

    userStyleSheet = UserStyleSheet(source, url, WTFMove(whitelist), WTFMove(blacklist), injectedFrames, level);
    return true;
}

#if ENABLE(MEDIA_SESSION)
void ArgumentCoder<MediaSessionMetadata>::encode(Encoder& encoder, const MediaSessionMetadata& result)
{
    encoder << result.artist();
    encoder << result.album();
    encoder << result.title();
    encoder << result.artworkURL();
}

bool ArgumentCoder<MediaSessionMetadata>::decode(Decoder& decoder, MediaSessionMetadata& result)
{
    String artist, album, title;
    URL artworkURL;
    if (!decoder.decode(artist))
        return false;
    if (!decoder.decode(album))
        return false;
    if (!decoder.decode(title))
        return false;
    if (!decoder.decode(artworkURL))
        return false;
    result = MediaSessionMetadata(title, artist, album, artworkURL);
    return true;
}
#endif

void ArgumentCoder<UserScript>::encode(Encoder& encoder, const UserScript& userScript)
{
    encoder << userScript.source();
    encoder << userScript.url();
    encoder << userScript.whitelist();
    encoder << userScript.blacklist();
    encoder.encodeEnum(userScript.injectionTime());
    encoder.encodeEnum(userScript.injectedFrames());
}

bool ArgumentCoder<UserScript>::decode(Decoder& decoder, UserScript& userScript)
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

    UserScriptInjectionTime injectionTime;
    if (!decoder.decodeEnum(injectionTime))
        return false;

    UserContentInjectedFrames injectedFrames;
    if (!decoder.decodeEnum(injectedFrames))
        return false;

    userScript = UserScript(source, url, WTFMove(whitelist), WTFMove(blacklist), injectionTime, injectedFrames);
    return true;
}

void ArgumentCoder<ScrollableAreaParameters>::encode(Encoder& encoder, const ScrollableAreaParameters& parameters)
{
    encoder.encodeEnum(parameters.horizontalScrollElasticity);
    encoder.encodeEnum(parameters.verticalScrollElasticity);

    encoder.encodeEnum(parameters.horizontalScrollbarMode);
    encoder.encodeEnum(parameters.verticalScrollbarMode);

    encoder << parameters.hasEnabledHorizontalScrollbar;
    encoder << parameters.hasEnabledVerticalScrollbar;
}

bool ArgumentCoder<ScrollableAreaParameters>::decode(Decoder& decoder, ScrollableAreaParameters& params)
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

void ArgumentCoder<FixedPositionViewportConstraints>::encode(Encoder& encoder, const FixedPositionViewportConstraints& viewportConstraints)
{
    encoder << viewportConstraints.alignmentOffset();
    encoder << viewportConstraints.anchorEdges();

    encoder << viewportConstraints.viewportRectAtLastLayout();
    encoder << viewportConstraints.layerPositionAtLastLayout();
}

bool ArgumentCoder<FixedPositionViewportConstraints>::decode(Decoder& decoder, FixedPositionViewportConstraints& viewportConstraints)
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

    viewportConstraints = FixedPositionViewportConstraints();
    viewportConstraints.setAlignmentOffset(alignmentOffset);
    viewportConstraints.setAnchorEdges(anchorEdges);

    viewportConstraints.setViewportRectAtLastLayout(viewportRectAtLastLayout);
    viewportConstraints.setLayerPositionAtLastLayout(layerPositionAtLastLayout);
    
    return true;
}

void ArgumentCoder<StickyPositionViewportConstraints>::encode(Encoder& encoder, const StickyPositionViewportConstraints& viewportConstraints)
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

bool ArgumentCoder<StickyPositionViewportConstraints>::decode(Decoder& decoder, StickyPositionViewportConstraints& viewportConstraints)
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
    
    viewportConstraints = StickyPositionViewportConstraints();
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

#if !USE(COORDINATED_GRAPHICS)
void ArgumentCoder<FilterOperation>::encode(Encoder& encoder, const FilterOperation& filter)
{
    encoder.encodeEnum(filter.type());

    switch (filter.type()) {
    case FilterOperation::NONE:
    case FilterOperation::REFERENCE:
        ASSERT_NOT_REACHED();
        break;
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
        encoder << downcast<BasicColorMatrixFilterOperation>(filter).amount();
        break;
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
        encoder << downcast<BasicComponentTransferFilterOperation>(filter).amount();
        break;
    case FilterOperation::BLUR:
        encoder << downcast<BlurFilterOperation>(filter).stdDeviation();
        break;
    case FilterOperation::DROP_SHADOW: {
        const auto& dropShadowFilter = downcast<DropShadowFilterOperation>(filter);
        encoder << dropShadowFilter.location();
        encoder << dropShadowFilter.stdDeviation();
        encoder << dropShadowFilter.color();
        break;
    }
    case FilterOperation::DEFAULT:
        encoder.encodeEnum(downcast<DefaultFilterOperation>(filter).representedType());
        break;
    case FilterOperation::PASSTHROUGH:
        break;
    }
}

bool decodeFilterOperation(Decoder& decoder, RefPtr<FilterOperation>& filter)
{
    FilterOperation::OperationType type;
    if (!decoder.decodeEnum(type))
        return false;

    switch (type) {
    case FilterOperation::NONE:
    case FilterOperation::REFERENCE:
        ASSERT_NOT_REACHED();
        decoder.markInvalid();
        return false;
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
        filter = BlurFilterOperation::create(stdDeviation);
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
        filter = DropShadowFilterOperation::create(location, stdDeviation, color);
        break;
    }
    case FilterOperation::DEFAULT: {
        FilterOperation::OperationType representedType;
        if (!decoder.decodeEnum(representedType))
            return false;
        filter = DefaultFilterOperation::create(representedType);
        break;
    }
    case FilterOperation::PASSTHROUGH:
        filter = PassthroughFilterOperation::create();
        break;
    }
            
    return true;
}


void ArgumentCoder<FilterOperations>::encode(Encoder& encoder, const FilterOperations& filters)
{
    encoder << static_cast<uint64_t>(filters.size());

    for (const auto& filter : filters.operations())
        encoder << *filter;
}

bool ArgumentCoder<FilterOperations>::decode(Decoder& decoder, FilterOperations& filters)
{
    uint64_t filterCount;
    if (!decoder.decode(filterCount))
        return false;

    for (uint64_t i = 0; i < filterCount; ++i) {
        RefPtr<FilterOperation> filter;
        if (!decodeFilterOperation(decoder, filter))
            return false;
        filters.operations().append(WTFMove(filter));
    }

    return true;
}
#endif // !USE(COORDINATED_GRAPHICS)

void ArgumentCoder<SessionID>::encode(Encoder& encoder, const SessionID& sessionID)
{
    encoder << sessionID.sessionID();
}

bool ArgumentCoder<SessionID>::decode(Decoder& decoder, SessionID& sessionID)
{
    uint64_t session;
    if (!decoder.decode(session))
        return false;

    sessionID = SessionID(session);

    return true;
}

void ArgumentCoder<BlobPart>::encode(Encoder& encoder, const BlobPart& blobPart)
{
    encoder << static_cast<uint32_t>(blobPart.type());
    switch (blobPart.type()) {
    case BlobPart::Data:
        encoder << blobPart.data();
        break;
    case BlobPart::Blob:
        encoder << blobPart.url();
        break;
    }
}

bool ArgumentCoder<BlobPart>::decode(Decoder& decoder, BlobPart& blobPart)
{
    uint32_t type;
    if (!decoder.decode(type))
        return false;

    switch (type) {
    case BlobPart::Data: {
        Vector<uint8_t> data;
        if (!decoder.decode(data))
            return false;
        blobPart = BlobPart(WTFMove(data));
        break;
    }
    case BlobPart::Blob: {
        URL url;
        if (!decoder.decode(url))
            return false;
        blobPart = BlobPart(url);
        break;
    }
    default:
        return false;
    }

    return true;
}

void ArgumentCoder<TextIndicatorData>::encode(Encoder& encoder, const TextIndicatorData& textIndicatorData)
{
    encoder << textIndicatorData.selectionRectInRootViewCoordinates;
    encoder << textIndicatorData.textBoundingRectInRootViewCoordinates;
    encoder << textIndicatorData.textRectsInBoundingRectCoordinates;
    encoder << textIndicatorData.contentImageScaleFactor;
    encoder.encodeEnum(textIndicatorData.presentationTransition);
    encoder << static_cast<uint64_t>(textIndicatorData.options);

    encodeOptionalImage(encoder, textIndicatorData.contentImage.get());
    encodeOptionalImage(encoder, textIndicatorData.contentImageWithHighlight.get());
}

bool ArgumentCoder<TextIndicatorData>::decode(Decoder& decoder, TextIndicatorData& textIndicatorData)
{
    if (!decoder.decode(textIndicatorData.selectionRectInRootViewCoordinates))
        return false;

    if (!decoder.decode(textIndicatorData.textBoundingRectInRootViewCoordinates))
        return false;

    if (!decoder.decode(textIndicatorData.textRectsInBoundingRectCoordinates))
        return false;

    if (!decoder.decode(textIndicatorData.contentImageScaleFactor))
        return false;

    if (!decoder.decodeEnum(textIndicatorData.presentationTransition))
        return false;

    uint64_t options;
    if (!decoder.decode(options))
        return false;
    textIndicatorData.options = static_cast<TextIndicatorOptions>(options);

    if (!decodeOptionalImage(decoder, textIndicatorData.contentImage))
        return false;

    if (!decodeOptionalImage(decoder, textIndicatorData.contentImageWithHighlight))
        return false;

    return true;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void ArgumentCoder<MediaPlaybackTargetContext>::encode(Encoder& encoder, const MediaPlaybackTargetContext& target)
{
    bool hasPlatformData = target.encodingRequiresPlatformData();
    encoder << hasPlatformData;

    int32_t targetType = target.type();
    encoder << targetType;

    if (target.encodingRequiresPlatformData()) {
        encodePlatformData(encoder, target);
        return;
    }

    ASSERT(targetType == MediaPlaybackTargetContext::MockType);
    encoder << target.mockDeviceName();
    encoder << static_cast<int32_t>(target.mockState());
}

bool ArgumentCoder<MediaPlaybackTargetContext>::decode(Decoder& decoder, MediaPlaybackTargetContext& target)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    int32_t targetType;
    if (!decoder.decode(targetType))
        return false;

    if (hasPlatformData)
        return decodePlatformData(decoder, target);

    ASSERT(targetType == MediaPlaybackTargetContext::MockType);

    String mockDeviceName;
    if (!decoder.decode(mockDeviceName))
        return false;

    int32_t mockState;
    if (!decoder.decode(mockState))
        return false;

    target = MediaPlaybackTargetContext(mockDeviceName, static_cast<MediaPlaybackTargetContext::State>(mockState));
    return true;
}
#endif

void ArgumentCoder<DictionaryPopupInfo>::encode(IPC::Encoder& encoder, const DictionaryPopupInfo& info)
{
    encoder << info.origin;
    encoder << info.textIndicator;

#if PLATFORM(COCOA)
    bool hadOptions = info.options;
    encoder << hadOptions;
    if (hadOptions)
        IPC::encode(encoder, info.options.get());

    bool hadAttributedString = info.attributedString;
    encoder << hadAttributedString;
    if (hadAttributedString)
        IPC::encode(encoder, info.attributedString.get());
#endif
}

bool ArgumentCoder<DictionaryPopupInfo>::decode(IPC::Decoder& decoder, DictionaryPopupInfo& result)
{
    if (!decoder.decode(result.origin))
        return false;

    if (!decoder.decode(result.textIndicator))
        return false;

#if PLATFORM(COCOA)
    bool hadOptions;
    if (!decoder.decode(hadOptions))
        return false;
    if (hadOptions) {
        if (!IPC::decode(decoder, result.options))
            return false;
    } else
        result.options = nullptr;

    bool hadAttributedString;
    if (!decoder.decode(hadAttributedString))
        return false;
    if (hadAttributedString) {
        if (!IPC::decode(decoder, result.attributedString))
            return false;
    } else
        result.attributedString = nullptr;
#endif
    return true;
}

void ArgumentCoder<ExceptionDetails>::encode(IPC::Encoder& encoder, const ExceptionDetails& info)
{
    encoder << info.message;
    encoder << info.lineNumber;
    encoder << info.columnNumber;
    encoder << info.sourceURL;
}

bool ArgumentCoder<ExceptionDetails>::decode(IPC::Decoder& decoder, ExceptionDetails& result)
{
    if (!decoder.decode(result.message))
        return false;

    if (!decoder.decode(result.lineNumber))
        return false;

    if (!decoder.decode(result.columnNumber))
        return false;

    if (!decoder.decode(result.sourceURL))
        return false;

    return true;
}

void ArgumentCoder<ResourceLoadStatistics>::encode(Encoder& encoder, const WebCore::ResourceLoadStatistics& statistics)
{
    encoder << statistics.highLevelDomain;
    
    // User interaction
    encoder << statistics.hadUserInteraction;
    encoder << statistics.mostRecentUserInteraction;
    encoder << statistics.grandfathered;
    
    // Top frame stats
    encoder << statistics.topFrameHasBeenNavigatedToBefore;
    encoder << statistics.topFrameHasBeenRedirectedTo;
    encoder << statistics.topFrameHasBeenRedirectedFrom;
    encoder << statistics.topFrameInitialLoadCount;
    encoder << statistics.topFrameHasBeenNavigatedTo;
    encoder << statistics.topFrameHasBeenNavigatedFrom;
    
    // Subframe stats
    encoder << statistics.subframeHasBeenLoadedBefore;
    encoder << statistics.subframeHasBeenRedirectedTo;
    encoder << statistics.subframeHasBeenRedirectedFrom;
    encoder << statistics.subframeSubResourceCount;
    encoder << statistics.subframeUnderTopFrameOrigins;
    encoder << statistics.subframeUniqueRedirectsTo;
    encoder << statistics.subframeHasBeenNavigatedTo;
    encoder << statistics.subframeHasBeenNavigatedFrom;
    
    // Subresource stats
    encoder << statistics.subresourceHasBeenRedirectedFrom;
    encoder << statistics.subresourceHasBeenRedirectedTo;
    encoder << statistics.subresourceHasBeenSubresourceCount;
    encoder << statistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited;
    encoder << statistics.subresourceUnderTopFrameOrigins;
    encoder << statistics.subresourceUniqueRedirectsTo;
    
    // Prevalent Resource
    encoder << statistics.redirectedToOtherPrevalentResourceOrigins;
    encoder << statistics.isPrevalentResource;
    encoder << statistics.dataRecordsRemoved;
}

bool ArgumentCoder<ResourceLoadStatistics>::decode(Decoder& decoder, WebCore::ResourceLoadStatistics& statistics)
{
    if (!decoder.decode(statistics.highLevelDomain))
        return false;
    
    // User interaction
    if (!decoder.decode(statistics.hadUserInteraction))
        return false;

    if (!decoder.decode(statistics.mostRecentUserInteraction))
        return false;

    if (!decoder.decode(statistics.grandfathered))
        return false;
    
    // Top frame stats
    if (!decoder.decode(statistics.topFrameHasBeenNavigatedToBefore))
        return false;
    
    if (!decoder.decode(statistics.topFrameHasBeenRedirectedTo))
        return false;
    
    if (!decoder.decode(statistics.topFrameHasBeenRedirectedFrom))
        return false;
    
    if (!decoder.decode(statistics.topFrameInitialLoadCount))
        return false;
    
    if (!decoder.decode(statistics.topFrameHasBeenNavigatedTo))
        return false;
    
    if (!decoder.decode(statistics.topFrameHasBeenNavigatedFrom))
        return false;
    
    // Subframe stats
    if (!decoder.decode(statistics.subframeHasBeenLoadedBefore))
        return false;
    
    if (!decoder.decode(statistics.subframeHasBeenRedirectedTo))
        return false;
    
    if (!decoder.decode(statistics.subframeHasBeenRedirectedFrom))
        return false;
    
    if (!decoder.decode(statistics.subframeSubResourceCount))
        return false;
    
    if (!decoder.decode(statistics.subframeUnderTopFrameOrigins))
        return false;

    if (!decoder.decode(statistics.subframeUniqueRedirectsTo))
        return false;
    
    if (!decoder.decode(statistics.subframeHasBeenNavigatedTo))
        return false;
    
    if (!decoder.decode(statistics.subframeHasBeenNavigatedFrom))
        return false;
    
    // Subresource stats
    if (!decoder.decode(statistics.subresourceHasBeenRedirectedFrom))
        return false;
    
    if (!decoder.decode(statistics.subresourceHasBeenRedirectedTo))
        return false;
    
    if (!decoder.decode(statistics.subresourceHasBeenSubresourceCount))
        return false;
    
    if (!decoder.decode(statistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited))
        return false;
    
    if (!decoder.decode(statistics.subresourceUnderTopFrameOrigins))
        return false;

    if (!decoder.decode(statistics.subresourceUniqueRedirectsTo))
        return false;
    
    // Prevalent Resource
    if (!decoder.decode(statistics.redirectedToOtherPrevalentResourceOrigins))
        return false;
    
    if (!decoder.decode(statistics.isPrevalentResource))
        return false;

    if (!decoder.decode(statistics.dataRecordsRemoved))
        return false;

    return true;
}

#if ENABLE(MEDIA_STREAM)
void ArgumentCoder<MediaConstraintsData>::encode(Encoder& encoder, const WebCore::MediaConstraintsData& constraint)
{
    encoder << constraint.mandatoryConstraints;

    auto& advancedConstraints = constraint.advancedConstraints;
    encoder << static_cast<uint64_t>(advancedConstraints.size());
    for (const auto& advancedConstraint : advancedConstraints)
        encoder << advancedConstraint;

    encoder << constraint.isValid;
}

bool ArgumentCoder<MediaConstraintsData>::decode(Decoder& decoder, WebCore::MediaConstraintsData& constraints)
{
    MediaTrackConstraintSetMap mandatoryConstraints;
    if (!decoder.decode(mandatoryConstraints))
        return false;

    uint64_t advancedCount;
    if (!decoder.decode(advancedCount))
        return false;

    Vector<MediaTrackConstraintSetMap> advancedConstraints;
    advancedConstraints.reserveInitialCapacity(advancedCount);
    for (size_t i = 0; i < advancedCount; ++i) {
        MediaTrackConstraintSetMap map;
        if (!decoder.decode(map))
            return false;

        advancedConstraints.uncheckedAppend(WTFMove(map));
    }

    bool isValid;
    if (!decoder.decode(isValid))
        return false;

    constraints.mandatoryConstraints = WTFMove(mandatoryConstraints);
    constraints.advancedConstraints = WTFMove(advancedConstraints);
    constraints.isValid = isValid;

    return true;
}

void ArgumentCoder<CaptureDevice>::encode(Encoder& encoder, const WebCore::CaptureDevice& device)
{
    encoder << device.persistentId();
    encoder << device.label();
    encoder << device.groupId();
    encoder << device.enabled();
    encoder.encodeEnum(device.type());
}

bool ArgumentCoder<CaptureDevice>::decode(Decoder& decoder, WebCore::CaptureDevice& device)
{
    String persistentId;
    if (!decoder.decode(persistentId))
        return false;

    String label;
    if (!decoder.decode(label))
        return false;

    String groupId;
    if (!decoder.decode(groupId))
        return false;

    bool enabled;
    if (!decoder.decode(enabled))
        return false;

    CaptureDevice::DeviceType type;
    if (!decoder.decodeEnum(type))
        return false;

    device.setPersistentId(persistentId);
    device.setLabel(label);
    device.setGroupId(groupId);
    device.setType(type);
    device.setEnabled(enabled);

    return true;
}
#endif

#if ENABLE(INDEXED_DATABASE)
void ArgumentCoder<IDBKeyPath>::encode(Encoder& encoder, const IDBKeyPath& keyPath)
{
    bool isString = WTF::holds_alternative<String>(keyPath);
    encoder << isString;
    if (isString)
        encoder << WTF::get<String>(keyPath);
    else
        encoder << WTF::get<Vector<String>>(keyPath);
}

bool ArgumentCoder<IDBKeyPath>::decode(Decoder& decoder, IDBKeyPath& keyPath)
{
    bool isString;
    if (!decoder.decode(isString))
        return false;
    if (isString) {
        String string;
        if (!decoder.decode(string))
            return false;
        keyPath = string;
    } else {
        Vector<String> vector;
        if (!decoder.decode(vector))
            return false;
        keyPath = vector;
    }
    return true;
}
#endif

#if ENABLE(CSS_SCROLL_SNAP)

void ArgumentCoder<ScrollOffsetRange<float>>::encode(Encoder& encoder, const ScrollOffsetRange<float>& range)
{
    encoder << range.start;
    encoder << range.end;
}

bool ArgumentCoder<ScrollOffsetRange<float>>::decode(Decoder& decoder, ScrollOffsetRange<float>& range)
{
    float start;
    if (!decoder.decode(start))
        return false;

    float end;
    if (!decoder.decode(end))
        return false;

    range.start = start;
    range.end = end;
    return true;
}

#endif

} // namespace IPC
