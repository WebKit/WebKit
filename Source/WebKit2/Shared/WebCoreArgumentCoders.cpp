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
#include <WebCore/TextCheckerClient.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/text/StringHash.h>

#if USE(COORDINATED_GRAPHICS)
#include <WebCore/Animation.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/Length.h>
#include <WebCore/TransformationMatrix.h>

#if ENABLE(CSS_FILTERS)
#include <WebCore/FilterOperations.h>
#endif
#endif

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

#if USE(COORDINATED_GRAPHICS)
void ArgumentCoder<FloatPoint3D>::encode(ArgumentEncoder* encoder, const FloatPoint3D& floatPoint3D)
{
    SimpleArgumentCoder<FloatPoint3D>::encode(encoder, floatPoint3D);
}

bool ArgumentCoder<FloatPoint3D>::decode(ArgumentDecoder* decoder, FloatPoint3D& floatPoint3D)
{
    return SimpleArgumentCoder<FloatPoint3D>::decode(decoder, floatPoint3D);
}

void ArgumentCoder<Length>::encode(ArgumentEncoder* encoder, const Length& length)
{
    SimpleArgumentCoder<Length>::encode(encoder, length);
}

bool ArgumentCoder<Length>::decode(ArgumentDecoder* decoder, Length& length)
{
    return SimpleArgumentCoder<Length>::decode(decoder, length);
}

void ArgumentCoder<TransformationMatrix>::encode(ArgumentEncoder* encoder, const TransformationMatrix& transformationMatrix)
{
    SimpleArgumentCoder<TransformationMatrix>::encode(encoder, transformationMatrix);
}

bool ArgumentCoder<TransformationMatrix>::decode(ArgumentDecoder* decoder, TransformationMatrix& transformationMatrix)
{
    return SimpleArgumentCoder<TransformationMatrix>::decode(decoder, transformationMatrix);
}

#if ENABLE(CSS_FILTERS)
void ArgumentCoder<WebCore::FilterOperations>::encode(ArgumentEncoder* encoder, const WebCore::FilterOperations& filters)
{
    encoder->encodeUInt32(filters.size());
    for (size_t i = 0; i < filters.size(); ++i) {
        const FilterOperation* filter = filters.at(i);
        FilterOperation::OperationType type = filter->getOperationType();
        encoder->encodeEnum(type);
        switch (type) {
        case FilterOperation::GRAYSCALE:
        case FilterOperation::SEPIA:
        case FilterOperation::SATURATE:
        case FilterOperation::HUE_ROTATE:
            encoder->encodeDouble(static_cast<const BasicColorMatrixFilterOperation*>(filter)->amount());
            break;
        case FilterOperation::INVERT:
        case FilterOperation::BRIGHTNESS:
        case FilterOperation::CONTRAST:
        case FilterOperation::OPACITY:
            encoder->encodeDouble(static_cast<const BasicComponentTransferFilterOperation*>(filter)->amount());
            break;
        case FilterOperation::BLUR:
            ArgumentCoder<Length>::encode(encoder, static_cast<const BlurFilterOperation*>(filter)->stdDeviation());
            break;
        case FilterOperation::DROP_SHADOW: {
            const DropShadowFilterOperation* shadow = static_cast<const DropShadowFilterOperation*>(filter);
            ArgumentCoder<IntPoint>::encode(encoder, shadow->location());
            encoder->encodeInt32(shadow->stdDeviation());
            ArgumentCoder<Color>::encode(encoder, shadow->color());
            break;
        }
        default:
            break;
        }
    }
}

bool ArgumentCoder<WebCore::FilterOperations>::decode(ArgumentDecoder* decoder, WebCore::FilterOperations& filters)
{
    uint32_t size;
    if (!decoder->decodeUInt32(size))
        return false;

    Vector<RefPtr<FilterOperation> >& operations = filters.operations();

    for (size_t i = 0; i < size; ++i) {
        FilterOperation::OperationType type;
        RefPtr<FilterOperation> filter;
        if (!decoder->decodeEnum(type))
            return false;

        switch (type) {
        case FilterOperation::GRAYSCALE:
        case FilterOperation::SEPIA:
        case FilterOperation::SATURATE:
        case FilterOperation::HUE_ROTATE: {
            double value;
            if (!decoder->decodeDouble(value))
                return false;
            filter = BasicColorMatrixFilterOperation::create(value, type);
            break;
        }
        case FilterOperation::INVERT:
        case FilterOperation::BRIGHTNESS:
        case FilterOperation::CONTRAST:
        case FilterOperation::OPACITY: {
            double value;
            if (!decoder->decodeDouble(value))
                return false;
            filter = BasicComponentTransferFilterOperation::create(value, type);
            break;
        }
        case FilterOperation::BLUR: {
            Length length;
            if (!ArgumentCoder<Length>::decode(decoder, length))
                return false;
            filter = BlurFilterOperation::create(length, type);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            IntPoint location;
            int32_t stdDeviation;
            Color color;
            if (!ArgumentCoder<IntPoint>::decode(decoder, location))
                return false;
            if (!decoder->decodeInt32(stdDeviation))
                return false;
            if (!ArgumentCoder<Color>::decode(decoder, color))
                return false;
            filter = DropShadowFilterOperation::create(location, stdDeviation, color, type);
            break;
        }
        default:
            break;
        }

        if (filter)
            operations.append(filter);
    }

    return true;
}
#endif

#endif

} // namespace CoreIPC
