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
#include <WebCore/Editor.h>
#include <WebCore/FileChooser.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h>
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

using namespace WebCore;
using namespace WebKit;

namespace CoreIPC {

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


void ArgumentCoder<ViewportArguments>::encode(ArgumentEncoder* encoder, const ViewportArguments& viewportArguments)
{
    SimpleArgumentCoder<ViewportArguments>::encode(encoder, viewportArguments);
}

bool ArgumentCoder<ViewportArguments>::decode(ArgumentDecoder* decoder, ViewportArguments& viewportArguments)
{
    return SimpleArgumentCoder<ViewportArguments>::decode(decoder, viewportArguments);
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


void ArgumentCoder<MatrixTransformOperation>::encode(ArgumentEncoder* encoder, const MatrixTransformOperation& operation)
{
    SimpleArgumentCoder<MatrixTransformOperation>::encode(encoder, operation);
}

bool ArgumentCoder<MatrixTransformOperation>::decode(ArgumentDecoder* decoder, MatrixTransformOperation& operation)
{
    return SimpleArgumentCoder<MatrixTransformOperation>::decode(decoder, operation);
}


void ArgumentCoder<Matrix3DTransformOperation>::encode(ArgumentEncoder* encoder, const Matrix3DTransformOperation& operation)
{
    SimpleArgumentCoder<Matrix3DTransformOperation>::encode(encoder, operation);
}

bool ArgumentCoder<Matrix3DTransformOperation>::decode(ArgumentDecoder* decoder, Matrix3DTransformOperation& operation)
{
    return SimpleArgumentCoder<Matrix3DTransformOperation>::decode(decoder, operation);
}


void ArgumentCoder<PerspectiveTransformOperation>::encode(ArgumentEncoder* encoder, const PerspectiveTransformOperation& operation)
{
    SimpleArgumentCoder<PerspectiveTransformOperation>::encode(encoder, operation);
}

bool ArgumentCoder<PerspectiveTransformOperation>::decode(ArgumentDecoder* decoder, PerspectiveTransformOperation& operation)
{
    return SimpleArgumentCoder<PerspectiveTransformOperation>::decode(decoder, operation);
}


void ArgumentCoder<RotateTransformOperation>::encode(ArgumentEncoder* encoder, const RotateTransformOperation& operation)
{
    SimpleArgumentCoder<RotateTransformOperation>::encode(encoder, operation);
}

bool ArgumentCoder<RotateTransformOperation>::decode(ArgumentDecoder* decoder, RotateTransformOperation& operation)
{
    return SimpleArgumentCoder<RotateTransformOperation>::decode(decoder, operation);
}


void ArgumentCoder<ScaleTransformOperation>::encode(ArgumentEncoder* encoder, const ScaleTransformOperation& operation)
{
    SimpleArgumentCoder<ScaleTransformOperation>::encode(encoder, operation);
}

bool ArgumentCoder<ScaleTransformOperation>::decode(ArgumentDecoder* decoder, ScaleTransformOperation& operation)
{
    return SimpleArgumentCoder<ScaleTransformOperation>::decode(decoder, operation);
}


void ArgumentCoder<SkewTransformOperation>::encode(ArgumentEncoder* encoder, const SkewTransformOperation& operation)
{
    SimpleArgumentCoder<SkewTransformOperation>::encode(encoder, operation);
}

bool ArgumentCoder<SkewTransformOperation>::decode(ArgumentDecoder* decoder, SkewTransformOperation& operation)
{
    return SimpleArgumentCoder<SkewTransformOperation>::decode(decoder, operation);
}


void ArgumentCoder<TranslateTransformOperation>::encode(ArgumentEncoder* encoder, const TranslateTransformOperation& operation)
{
    SimpleArgumentCoder<TranslateTransformOperation>::encode(encoder, operation);
}

bool ArgumentCoder<TranslateTransformOperation>::decode(ArgumentDecoder* decoder, TranslateTransformOperation& operation)
{
    return SimpleArgumentCoder<TranslateTransformOperation>::decode(decoder, operation);
}

void ArgumentCoder<RefPtr<TimingFunction> >::encode(ArgumentEncoder* encoder, const RefPtr<TimingFunction>& function)
{
    // We don't want to encode null-references.
    ASSERT(function);

    encoder->encodeEnum(function->type());
    switch (function->type()) {
    case TimingFunction::LinearFunction:
        break;
    case TimingFunction::CubicBezierFunction: {
        CubicBezierTimingFunction* cubicFunction = static_cast<CubicBezierTimingFunction*>(function.get());
        encoder->encodeDouble(cubicFunction->x1());
        encoder->encodeDouble(cubicFunction->y1());
        encoder->encodeDouble(cubicFunction->x2());
        encoder->encodeDouble(cubicFunction->y2());
        break;
    }
    case TimingFunction::StepsFunction: {
        StepsTimingFunction* stepsFunction = static_cast<StepsTimingFunction*>(function.get());
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
    encoder->encode(*static_cast<const T*>(operation.get()));
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
bool decodeOperation(ArgumentDecoder* decoder, RefPtr<TransformOperation>& operation, PassRefPtr<T> newOperation)
{
    if (!decoder->decode(*newOperation.get()))
        return false;

    operation = newOperation.get();
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
        return decodeOperation(decoder, operation, ScaleTransformOperation::create(1.0, 1.0, type));

    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_X:
    case TransformOperation::TRANSLATE_Y:
    case TransformOperation::TRANSLATE_Z:
    case TransformOperation::TRANSLATE_3D:
        return decodeOperation(decoder, operation, TranslateTransformOperation::create(Length(0, WebCore::Fixed), Length(0, WebCore::Fixed), type));

    case TransformOperation::ROTATE:
    case TransformOperation::ROTATE_X:
    case TransformOperation::ROTATE_Y:
    case TransformOperation::ROTATE_3D:
        return decodeOperation(decoder, operation, RotateTransformOperation::create(0.0, type));

    case TransformOperation::SKEW:
    case TransformOperation::SKEW_X:
    case TransformOperation::SKEW_Y:
        return decodeOperation(decoder, operation, SkewTransformOperation::create(0.0, 0.0, type));

    case TransformOperation::MATRIX:
        return decodeOperation(decoder, operation, MatrixTransformOperation::create(TransformationMatrix()));

    case TransformOperation::MATRIX_3D:
        return decodeOperation(decoder, operation, Matrix3DTransformOperation::create(TransformationMatrix()));

    case TransformOperation::PERSPECTIVE:
        return decodeOperation(decoder, operation, PerspectiveTransformOperation::create(Length(0, WebCore::Fixed)));

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

void ArgumentCoder<Animation>::encode(ArgumentEncoder* encoder, const Animation& animation)
{
    encodeBoolAndValue(encoder, animation.isDelaySet(), animation.delay());
    encodeBoolAndEnumValue(encoder, animation.isDirectionSet(), animation.direction());
    encodeBoolAndValue(encoder, animation.isDurationSet(), animation.duration());
    encodeBoolAndValue(encoder, animation.isFillModeSet(), animation.fillMode());
    encodeBoolAndValue(encoder, animation.isIterationCountSet(), animation.iterationCount());
    encodeBoolAndValue(encoder, animation.isNameSet(), animation.name());
    encodeBoolAndEnumValue(encoder, animation.isPlayStateSet(), animation.playState());
    encodeBoolAndValue(encoder, animation.isPropertySet(), animation.property());
    encodeBoolAndValue<RefPtr<TimingFunction> >(encoder, animation.isTimingFunctionSet(), animation.timingFunction());
    encoder->encodeBool(animation.isNoneAnimation());
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

bool ArgumentCoder<Animation>::decode(ArgumentDecoder* decoder, Animation& animation)
{
    bool isDelaySet, isDirectionSet, isDurationSet, isFillModeSet, isIterationCountSet, isNameSet, isPlayStateSet, isPropertySet, isTimingFunctionSet;
    int property, iterationCount, fillMode;
    double duration;
    RefPtr<TimingFunction> timingFunction;
    String name;

    animation.clearAll();

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

    if (isDelaySet)
        animation.setDelay(delay);
    if (isDirectionSet)
        animation.setDirection(direction);
    if (isDurationSet)
        animation.setDuration(duration);
    if (isFillModeSet)
        animation.setFillMode(fillMode);
    if (isIterationCountSet)
        animation.setIterationCount(iterationCount);
    if (isNameSet)
        animation.setName(name);
    if (isPlayStateSet)
        animation.setPlayState(playState);
    if (isPropertySet)
        animation.setProperty(property);
    if (isTimingFunctionSet)
        animation.setTimingFunction(timingFunction);

    return true;
}
#endif

} // namespace CoreIPC
