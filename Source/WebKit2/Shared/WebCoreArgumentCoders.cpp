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

#if PLATFORM(QT)
#include <WebCore/Animation.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/IdentityTransformOperation.h>
#include <WebCore/Matrix3DTransformOperation.h>
#include <WebCore/MatrixTransformOperation.h>
#include <WebCore/PerspectiveTransformOperation.h>
#include <WebCore/RotateTransformOperation.h>
#include <WebCore/ScaleTransformOperation.h>
#include <WebCore/SkewTransformOperation.h>
#include <WebCore/TimingFunction.h>
#include <WebCore/TransformOperation.h>
#include <WebCore/TransformOperations.h>
#include <WebCore/TranslateTransformOperation.h>
#endif

#if USE(UI_SIDE_COMPOSITING) && ENABLE(CSS_FILTERS)
#include <WebCore/FilterOperations.h>
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

#if PLATFORM(QT)

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


void ArgumentCoder<RefPtr<MatrixTransformOperation> >::encode(ArgumentEncoder* encoder, const MatrixTransformOperation* operation)
{
    ArgumentCoder<WebCore::TransformationMatrix>::encode(encoder, operation->matrix());
}

bool ArgumentCoder<RefPtr<MatrixTransformOperation> >::decode(ArgumentDecoder* decoder, RefPtr<MatrixTransformOperation>& operation)
{
    TransformationMatrix matrix;
    if (!ArgumentCoder<WebCore::TransformationMatrix>::decode(decoder, matrix))
        return false;

    operation = MatrixTransformOperation::create(matrix);
    return true;
}

void ArgumentCoder<RefPtr<Matrix3DTransformOperation> >::encode(ArgumentEncoder* encoder, const Matrix3DTransformOperation* operation)
{
    ArgumentCoder<TransformationMatrix>::encode(encoder, operation->matrix());
}

bool ArgumentCoder<RefPtr<Matrix3DTransformOperation> >::decode(ArgumentDecoder* decoder, RefPtr<Matrix3DTransformOperation>& operation)
{

    TransformationMatrix matrix;
    if (!ArgumentCoder<WebCore::TransformationMatrix>::decode(decoder, matrix))
        return false;

    operation = Matrix3DTransformOperation::create(matrix);
    return true;
}

void ArgumentCoder<RefPtr<PerspectiveTransformOperation> >::encode(ArgumentEncoder* encoder, const PerspectiveTransformOperation* operation)
{
    ArgumentCoder<Length>::encode(encoder, operation->perspective());
}

bool ArgumentCoder<RefPtr<PerspectiveTransformOperation> >::decode(ArgumentDecoder* decoder, RefPtr<PerspectiveTransformOperation>& operation)
{
    Length perspective;
    if (!ArgumentCoder<Length>::decode(decoder, perspective))
        return false;

    operation = PerspectiveTransformOperation::create(perspective);
    return true;
}

void ArgumentCoder<RefPtr<RotateTransformOperation> >::encode(ArgumentEncoder* encoder, const RotateTransformOperation* operation)
{
    const TransformOperation* transformOperation = operation;
    encoder->encodeEnum(transformOperation->getOperationType());
    encoder->encode(operation->x());
    encoder->encode(operation->y());
    encoder->encode(operation->z());
    encoder->encode(operation->angle());
}

bool ArgumentCoder<RefPtr<RotateTransformOperation> >::decode(ArgumentDecoder* decoder, RefPtr<RotateTransformOperation>& operation)
{
    TransformOperation::OperationType operationType;
    double x;
    double y;
    double z;
    double angle;

    if (!decoder->decodeEnum(operationType))
        return false;
    if (!decoder->decode(x))
        return false;
    if (!decoder->decode(y))
        return false;
    if (!decoder->decode(z))
        return false;
    if (!decoder->decode(angle))
        return false;

    operation = RotateTransformOperation::create(x, y, z, angle, operationType);
    return true;
}


void ArgumentCoder<RefPtr<ScaleTransformOperation> >::encode(ArgumentEncoder* encoder, const ScaleTransformOperation* operation)
{
    const TransformOperation* transformOperation = operation;
    encoder->encodeEnum(transformOperation->getOperationType());
    encoder->encode(operation->x());
    encoder->encode(operation->y());
    encoder->encode(operation->z());
}

bool ArgumentCoder<RefPtr<ScaleTransformOperation> >::decode(ArgumentDecoder* decoder, RefPtr<ScaleTransformOperation>& operation)
{
    TransformOperation::OperationType operationType;
    double x;
    double y;
    double z;

    if (!decoder->decodeEnum(operationType))
        return false;
    if (!decoder->decode(x))
        return false;
    if (!decoder->decode(y))
        return false;
    if (!decoder->decode(z))
        return false;

    operation = ScaleTransformOperation::create(x, y, z, operationType);
    return true;
}

void ArgumentCoder<RefPtr<SkewTransformOperation> >::encode(ArgumentEncoder* encoder, const SkewTransformOperation* operation)
{
    const TransformOperation* transformOperation = operation;
    encoder->encodeEnum(transformOperation->getOperationType());
    encoder->encode(operation->angleX());
    encoder->encode(operation->angleY());
}

bool ArgumentCoder<RefPtr<SkewTransformOperation> >::decode(ArgumentDecoder* decoder, RefPtr<SkewTransformOperation>& operation)
{
    TransformOperation::OperationType operationType;
    double angleX;
    double angleY;

    if (!decoder->decodeEnum(operationType))
        return false;
    if (!decoder->decode(angleX))
        return false;
    if (!decoder->decode(angleY))
        return false;

    operation = SkewTransformOperation::create(angleX, angleY, operationType);
    return true;
}

void ArgumentCoder<RefPtr<TranslateTransformOperation> >::encode(ArgumentEncoder* encoder, const TranslateTransformOperation* operation)
{
    const TransformOperation* transformOperation = operation;
    encoder->encodeEnum(transformOperation->getOperationType());
    ArgumentCoder<Length>::encode(encoder, operation->x());
    ArgumentCoder<Length>::encode(encoder, operation->y());
    ArgumentCoder<Length>::encode(encoder, operation->z());
}

bool ArgumentCoder<RefPtr<TranslateTransformOperation> >::decode(ArgumentDecoder* decoder, RefPtr<TranslateTransformOperation>& operation)
{
    TransformOperation::OperationType operationType;
    Length x;
    Length y;
    Length z;

    if (!decoder->decodeEnum(operationType))
        return false;
    if (!ArgumentCoder<Length>::decode(decoder, x))
        return false;
    if (!ArgumentCoder<Length>::decode(decoder, y))
        return false;
    if (!ArgumentCoder<Length>::decode(decoder, z))
        return false;

    operation = TranslateTransformOperation::create(x, y, z, operationType);
    return true;
}

void ArgumentCoder<RefPtr<TimingFunction> >::encode(ArgumentEncoder* encoder, const RefPtr<TimingFunction>& function)
{
    encode(encoder, function.get());
}

void ArgumentCoder<RefPtr<TimingFunction> >::encode(ArgumentEncoder* encoder, const WebCore::TimingFunction* function)
{
    if (!function) {
        encoder->encodeEnum(WebCore::TimingFunction::LinearFunction);
        return;
    }

    encoder->encodeEnum(function->type());
    switch (function->type()) {
    case TimingFunction::LinearFunction:
        break;
    case TimingFunction::CubicBezierFunction: {
        const WebCore::CubicBezierTimingFunction* cubicFunction = static_cast<const WebCore::CubicBezierTimingFunction*>(function);
        encoder->encodeDouble(cubicFunction->x1());
        encoder->encodeDouble(cubicFunction->y1());
        encoder->encodeDouble(cubicFunction->x2());
        encoder->encodeDouble(cubicFunction->y2());
        break;
    }
    case TimingFunction::StepsFunction: {
        const WebCore::StepsTimingFunction* stepsFunction = static_cast<const WebCore::StepsTimingFunction*>(function);
        encoder->encodeInt32(stepsFunction->numberOfSteps());
        encoder->encodeBool(stepsFunction->stepAtStart());
        break;
    }
    }
}

bool ArgumentCoder<RefPtr<TimingFunction> >::decode(ArgumentDecoder* decoder, RefPtr<TimingFunction>& function)
{
    TimingFunction::TimingFunctionType type;
    if (!decoder->decodeEnum(type))
        return false;

    switch (type) {
    case TimingFunction::LinearFunction:
        function = LinearTimingFunction::create();
        return true;

    case TimingFunction::CubicBezierFunction: {
        double x1, y1, x2, y2;
        if (!decoder->decodeDouble(x1))
            return false;
        if (!decoder->decodeDouble(y1))
            return false;
        if (!decoder->decodeDouble(x2))
            return false;
        if (!decoder->decodeDouble(y2))
            return false;
        function = CubicBezierTimingFunction::create(x1, y1, x2, y2);
        return true;
    }

    case TimingFunction::StepsFunction: {
        int numSteps;
        bool stepAtStart;
        if (!decoder->decodeInt32(numSteps))
            return false;
        if (!decoder->decodeBool(stepAtStart))
            return false;

        function = StepsTimingFunction::create(numSteps, stepAtStart);
        return true;
    }

    }

    return false;
}

template<typename T>
void encodeOperation(ArgumentEncoder* encoder, const RefPtr<TransformOperation>& operation)
{
    ArgumentCoder<RefPtr<T> >::encode(encoder, static_cast<const T*>(operation.get()));
}

void ArgumentCoder<RefPtr<TransformOperation> >::encode(ArgumentEncoder* encoder, const RefPtr<TransformOperation>& operation)
{
    // We don't want to encode null-references.
    ASSERT(operation);

    encoder->encodeEnum(operation->getOperationType());
    switch (operation->getOperationType()) {
    case TransformOperation::SCALE:
    case TransformOperation::SCALE_X:
    case TransformOperation::SCALE_Y:
    case TransformOperation::SCALE_Z:
    case TransformOperation::SCALE_3D:
        encodeOperation<ScaleTransformOperation>(encoder, operation);
        return;

    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_X:
    case TransformOperation::TRANSLATE_Y:
    case TransformOperation::TRANSLATE_Z:
    case TransformOperation::TRANSLATE_3D:
        encodeOperation<TranslateTransformOperation>(encoder, operation);
        return;

    case TransformOperation::ROTATE:
    case TransformOperation::ROTATE_X:
    case TransformOperation::ROTATE_Y:
    case TransformOperation::ROTATE_3D:
        encodeOperation<RotateTransformOperation>(encoder, operation);
        return;

    case TransformOperation::SKEW:
    case TransformOperation::SKEW_X:
    case TransformOperation::SKEW_Y:
        encodeOperation<SkewTransformOperation>(encoder, operation);
        return;

    case TransformOperation::MATRIX:
        encodeOperation<MatrixTransformOperation>(encoder, operation);
        return;

    case TransformOperation::MATRIX_3D:
        encodeOperation<Matrix3DTransformOperation>(encoder, operation);
        return;

    case TransformOperation::PERSPECTIVE:
        encodeOperation<PerspectiveTransformOperation>(encoder, operation);
        return;

    case TransformOperation::IDENTITY:
    case TransformOperation::NONE:
        return;
    }
}

template<typename T>
bool decodeOperation(ArgumentDecoder* decoder, RefPtr<TransformOperation>& operation)
{
    RefPtr<T> newOperation;
    if (!decoder->decode(newOperation))
        return false;

    operation = newOperation;
    return true;
}

bool ArgumentCoder<RefPtr<TransformOperation> >::decode(ArgumentDecoder* decoder, RefPtr<TransformOperation>& operation)
{
    TransformOperation::OperationType type;
    if (!decoder->decodeEnum(type))
        return false;

    switch (type) {
    case TransformOperation::SCALE:
    case TransformOperation::SCALE_X:
    case TransformOperation::SCALE_Y:
    case TransformOperation::SCALE_Z:
    case TransformOperation::SCALE_3D:
        return decodeOperation<ScaleTransformOperation>(decoder, operation);

    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_X:
    case TransformOperation::TRANSLATE_Y:
    case TransformOperation::TRANSLATE_Z:
    case TransformOperation::TRANSLATE_3D:
        return decodeOperation<TranslateTransformOperation>(decoder, operation);

    case TransformOperation::ROTATE:
    case TransformOperation::ROTATE_X:
    case TransformOperation::ROTATE_Y:
    case TransformOperation::ROTATE_3D:
        return decodeOperation<RotateTransformOperation>(decoder, operation);

    case TransformOperation::SKEW:
    case TransformOperation::SKEW_X:
    case TransformOperation::SKEW_Y:
        return decodeOperation<SkewTransformOperation>(decoder, operation);

    case TransformOperation::MATRIX:
        return decodeOperation<MatrixTransformOperation>(decoder, operation);

    case TransformOperation::MATRIX_3D:
        return decodeOperation<Matrix3DTransformOperation>(decoder, operation);

    case TransformOperation::PERSPECTIVE:
        return decodeOperation<PerspectiveTransformOperation>(decoder, operation);

    case TransformOperation::IDENTITY:
    case TransformOperation::NONE:
        operation = IdentityTransformOperation::create();
        return true;
    }

    return false;
}

void ArgumentCoder<TransformOperations>::encode(ArgumentEncoder* encoder, const TransformOperations& operations)
{
    Vector<RefPtr<TransformOperation> > operationsVector = operations.operations();
    int size = operationsVector.size();
    encoder->encodeInt32(size);
    for (int i = 0; i < size; ++i)
        ArgumentCoder<RefPtr<TransformOperation> >::encode(encoder, operationsVector[i]);
}

bool ArgumentCoder<TransformOperations>::decode(ArgumentDecoder* decoder, TransformOperations& operations)
{
    int size;
    if (!decoder->decodeInt32(size))
        return false;

    Vector<RefPtr<TransformOperation> >& operationVector = operations.operations();
    operationVector.clear();
    operationVector.resize(size);
    for (int i = 0; i < size; ++i) {
        RefPtr<TransformOperation> operation;
        if (!ArgumentCoder<RefPtr<TransformOperation> >::decode(decoder, operation))
            return false;
        operationVector[i] = operation;
    }

    return true;
}

template<typename T>
static void encodeBoolAndValue(ArgumentEncoder* encoder, bool isSet, const T& value)
{
    encoder->encodeBool(isSet);
    if (isSet)
        encoder->encode(value);
}

template<typename T>
static void encodeBoolAndEnumValue(ArgumentEncoder* encoder, bool isSet, T value)
{
    encoder->encodeBool(isSet);
    if (isSet)
        encoder->encodeEnum(value);
}

void ArgumentCoder<RefPtr<Animation> >::encode(ArgumentEncoder* encoder, const RefPtr<Animation>& animation)
{
    encodeBoolAndValue(encoder, animation->isDelaySet(), animation->delay());
    encodeBoolAndEnumValue(encoder, animation->isDirectionSet(), animation->direction());
    encodeBoolAndValue(encoder, animation->isDurationSet(), animation->duration());
    encodeBoolAndValue(encoder, animation->isFillModeSet(), animation->fillMode());
    encodeBoolAndValue(encoder, animation->isIterationCountSet(), animation->iterationCount());
    encodeBoolAndValue(encoder, animation->isNameSet(), animation->name());
    encodeBoolAndEnumValue(encoder, animation->isPlayStateSet(), animation->playState());
    encodeBoolAndValue(encoder, animation->isPropertySet(), static_cast<int>(animation->property()));
    encodeBoolAndValue<RefPtr<TimingFunction> >(encoder, animation->isTimingFunctionSet(), animation->timingFunction());
    encoder->encodeBool(animation->isNoneAnimation());
}


template<typename T>
static bool decodeBoolAndValue(ArgumentDecoder* decoder, bool& isSet, T& value)
{
    if (!decoder->decodeBool(isSet))
        return false;
    if (!isSet)
        return true;

    return decoder->decode(value);
}

template<typename T>
static bool decodeBoolAndEnumValue(ArgumentDecoder* decoder, bool& isSet, T& value)
{
    if (!decoder->decodeBool(isSet))
        return false;
    if (!isSet)
        return true;

    return decoder->decodeEnum(value);
}

bool ArgumentCoder<RefPtr<Animation> >::decode(ArgumentDecoder* decoder, RefPtr<Animation>& animation)
{
    bool isDelaySet, isDirectionSet, isDurationSet, isFillModeSet, isIterationCountSet, isNameSet, isPlayStateSet, isPropertySet, isTimingFunctionSet;
    int property, iterationCount, fillMode;
    double duration;
    RefPtr<TimingFunction> timingFunction;
    String name;

    double delay;
    if (!decodeBoolAndValue(decoder, isDelaySet, delay))
        return false;

    Animation::AnimationDirection direction = Animation::AnimationDirectionNormal;
    if (!decodeBoolAndEnumValue(decoder, isDirectionSet, direction))
        return false;
    if (!decodeBoolAndValue(decoder, isDurationSet, duration))
        return false;
    if (!decodeBoolAndValue(decoder, isFillModeSet, fillMode))
        return false;
    if (!decodeBoolAndValue(decoder, isIterationCountSet, iterationCount))
        return false;
    if (!decodeBoolAndValue(decoder, isNameSet, name))
        return false;

    EAnimPlayState playState = AnimPlayStatePlaying;
    if (!decodeBoolAndEnumValue(decoder, isPlayStateSet, playState))
        return false;
    if (!decodeBoolAndValue(decoder, isPropertySet, property))
        return false;
    if (!decodeBoolAndValue<RefPtr<TimingFunction> >(decoder, isTimingFunctionSet, timingFunction))
        return false;

    animation = Animation::create();
    animation->clearAll();

    if (isDelaySet)
        animation->setDelay(delay);
    if (isDirectionSet)
        animation->setDirection(direction);
    if (isDurationSet)
        animation->setDuration(duration);
    if (isFillModeSet)
        animation->setFillMode(fillMode);
    if (isIterationCountSet)
        animation->setIterationCount(iterationCount);
    if (isNameSet)
        animation->setName(name);
    if (isPlayStateSet)
        animation->setPlayState(playState);
    if (isPropertySet)
        animation->setProperty(static_cast<CSSPropertyID>(property));
    if (isTimingFunctionSet)
        animation->setTimingFunction(timingFunction);

    return true;
}

#if USE(ACCELERATED_COMPOSITING)

void ArgumentCoder<KeyframeValueList>::encode(ArgumentEncoder* encoder, const WebCore::KeyframeValueList& keyframes)
{
    encoder->encodeUInt32(keyframes.size());
    encoder->encodeInt32(keyframes.property());
    for (size_t i = 0; i < keyframes.size(); ++i) {
        const WebCore::AnimationValue* value = keyframes.at(i);
        encoder->encodeFloat(value->keyTime());
        ArgumentCoder<RefPtr<WebCore::TimingFunction> >::encode(encoder, value->timingFunction());
        switch (keyframes.property()) {
        case WebCore::AnimatedPropertyOpacity: {
            const WebCore::FloatAnimationValue* floatValue = static_cast<const WebCore::FloatAnimationValue*>(value);
            encoder->encodeFloat(floatValue->value());
            break;
        }
        case WebCore::AnimatedPropertyWebkitTransform: {
            const WebCore::TransformAnimationValue* transformValue = static_cast<const WebCore::TransformAnimationValue*>(value);
            ArgumentCoder<WebCore::TransformOperations>::encode(encoder, *transformValue->value());
            break;
        }
        default:
            break;
        }
    }
}

bool ArgumentCoder<KeyframeValueList>::decode(ArgumentDecoder* decoder, WebCore::KeyframeValueList& keyframes)
{
    uint32_t size;
    int32_t property;
    if (!decoder->decodeUInt32(size))
        return false;
    if (!decoder->decodeInt32(property))
        return false;

    keyframes = WebCore::KeyframeValueList(WebCore::AnimatedPropertyID(property));

    for (size_t i = 0; i < size; ++i) {
        float keyTime;
   
        RefPtr<WebCore::TimingFunction> timingFunction;
        if (!decoder->decodeFloat(keyTime))
            return false;
        if (!ArgumentCoder<RefPtr<WebCore::TimingFunction> >::decode(decoder, timingFunction))
            return false;

        switch (property) {
        case WebCore::AnimatedPropertyOpacity: {
            float value;
            if (!decoder->decodeFloat(value))
                return false;
            keyframes.insert(new WebCore::FloatAnimationValue(keyTime, value, timingFunction));
            break;
        }
        case WebCore::AnimatedPropertyWebkitTransform: {
            WebCore::TransformOperations value;
            if (!ArgumentCoder<WebCore::TransformOperations>::decode(decoder, value))
                return false;
            keyframes.insert(new WebCore::TransformAnimationValue(keyTime, &value, timingFunction));
            break;
        }
        default:
            break;
        }
    }

    return true;
}

#endif

#if USE(UI_SIDE_COMPOSITING) && ENABLE(CSS_FILTERS)
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
