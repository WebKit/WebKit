/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef WebCoreArgumentCoders_h
#define WebCoreArgumentCoders_h

#include "ArgumentCoders.h"
#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "Arguments.h"
#include "ShareableBitmap.h"
#include <WebCore/Animation.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/BitmapImage.h>
#include <WebCore/Credential.h>
#include <WebCore/Cursor.h>
#include <WebCore/DatabaseDetails.h>
#include <WebCore/Editor.h>
#include <WebCore/EditorClient.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/FloatRect.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/IdentityTransformOperation.h>
#include <WebCore/IntRect.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Length.h>
#include <WebCore/Matrix3DTransformOperation.h>
#include <WebCore/MatrixTransformOperation.h>
#include <WebCore/PerspectiveTransformOperation.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RotateTransformOperation.h>
#include <WebCore/ScaleTransformOperation.h>
#include <WebCore/SkewTransformOperation.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TimingFunction.h>
#include <WebCore/TransformOperation.h>
#include <WebCore/TransformOperations.h>
#include <WebCore/TransformationMatrix.h>
#include <WebCore/TranslateTransformOperation.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <limits>

namespace WebCore {
    class HTTPHeaderMap;
    struct MimeClassInfo;
    struct PluginInfo;
}

namespace CoreIPC {

template<> struct ArgumentCoder<WebCore::IntPoint> : SimpleArgumentCoder<WebCore::IntPoint> { };
template<> struct ArgumentCoder<WebCore::IntSize> : SimpleArgumentCoder<WebCore::IntSize> { };
template<> struct ArgumentCoder<WebCore::IntRect> : SimpleArgumentCoder<WebCore::IntRect> { };
template<> struct ArgumentCoder<WebCore::ViewportArguments> : SimpleArgumentCoder<WebCore::ViewportArguments> { };

template<> struct ArgumentCoder<WebCore::FloatPoint> : SimpleArgumentCoder<WebCore::FloatPoint> { };
template<> struct ArgumentCoder<WebCore::FloatPoint3D> : SimpleArgumentCoder<WebCore::FloatPoint3D> { };
template<> struct ArgumentCoder<WebCore::FloatSize> : SimpleArgumentCoder<WebCore::FloatSize> { };
template<> struct ArgumentCoder<WebCore::FloatRect> : SimpleArgumentCoder<WebCore::FloatRect> { };
template<> struct ArgumentCoder<WebCore::Length> : SimpleArgumentCoder<WebCore::Length> { };
template<> struct ArgumentCoder<WebCore::TransformationMatrix> : SimpleArgumentCoder<WebCore::TransformationMatrix> { };

template<> struct ArgumentCoder<WebCore::MatrixTransformOperation> : SimpleArgumentCoder<WebCore::MatrixTransformOperation> { };
template<> struct ArgumentCoder<WebCore::Matrix3DTransformOperation> : SimpleArgumentCoder<WebCore::Matrix3DTransformOperation> { };
template<> struct ArgumentCoder<WebCore::PerspectiveTransformOperation> : SimpleArgumentCoder<WebCore::PerspectiveTransformOperation> { };
template<> struct ArgumentCoder<WebCore::RotateTransformOperation> : SimpleArgumentCoder<WebCore::RotateTransformOperation> { };
template<> struct ArgumentCoder<WebCore::ScaleTransformOperation> : SimpleArgumentCoder<WebCore::ScaleTransformOperation> { };
template<> struct ArgumentCoder<WebCore::SkewTransformOperation> : SimpleArgumentCoder<WebCore::SkewTransformOperation> { };
template<> struct ArgumentCoder<WebCore::TranslateTransformOperation> : SimpleArgumentCoder<WebCore::TranslateTransformOperation> { };

template<> struct ArgumentCoder<WebCore::MimeClassInfo> {
    static void encode(ArgumentEncoder*, const WebCore::MimeClassInfo&);
    static bool decode(ArgumentDecoder*, WebCore::MimeClassInfo&);
};

template<> struct ArgumentCoder<WebCore::PluginInfo> {
    static void encode(ArgumentEncoder*, const WebCore::PluginInfo&);
    static bool decode(ArgumentDecoder*, WebCore::PluginInfo&);
};

template<> struct ArgumentCoder<WebCore::HTTPHeaderMap> {
    static void encode(ArgumentEncoder*, const WebCore::HTTPHeaderMap&);
    static bool decode(ArgumentDecoder*, WebCore::HTTPHeaderMap&);
};

template<> struct ArgumentCoder<WebCore::AuthenticationChallenge> {
    static void encode(ArgumentEncoder*, const WebCore::AuthenticationChallenge&);
    static bool decode(ArgumentDecoder*, WebCore::AuthenticationChallenge&);
};

template<> struct ArgumentCoder<WebCore::ProtectionSpace> {
    static void encode(ArgumentEncoder*, const WebCore::ProtectionSpace&);
    static bool decode(ArgumentDecoder*, WebCore::ProtectionSpace&);
};

template<> struct ArgumentCoder<WebCore::Credential> {
    static void encode(ArgumentEncoder* encoder, const WebCore::Credential& credential)
    {
        encoder->encode(CoreIPC::In(credential.user(), credential.password(), static_cast<uint32_t>(credential.persistence())));
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::Credential& credential)
    {
        String user;
        String password;
        int persistence;
        if (!decoder->decode(CoreIPC::Out(user, password, persistence)))
            return false;
        
        credential = WebCore::Credential(user, password, static_cast<WebCore::CredentialPersistence>(persistence));
        return true;
    }
};

#if USE(LAZY_NATIVE_CURSOR)

void encodeImage(ArgumentEncoder*, WebCore::Image*);
bool decodeImage(ArgumentDecoder*, RefPtr<WebCore::Image>&);
RefPtr<WebCore::Image> createImage(WebKit::ShareableBitmap*);

template<> struct ArgumentCoder<WebCore::Cursor> {
    static void encode(ArgumentEncoder* encoder, const WebCore::Cursor& cursor)
    {
        WebCore::Cursor::Type type = cursor.type();
#if !USE(CG)
        // FIXME: Currently we only have the createImage function implemented for CG.
        // Once we implement it for other platforms we can remove this conditional,
        // and the other conditionals below and in WebCoreArgumentCoders.cpp.
        if (type == WebCore::Cursor::Custom)
            type = WebCore::Cursor::Pointer;
#endif
        encoder->encode(static_cast<uint32_t>(type));
#if USE(CG)
        if (type != WebCore::Cursor::Custom)
            return;

        encodeImage(encoder, cursor.image());
        encoder->encode(cursor.hotSpot());
#endif
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::Cursor& cursor)
    {
        uint32_t typeInt;
        if (!decoder->decode(typeInt))
            return false;
        if (typeInt > WebCore::Cursor::Custom)
            return false;
        WebCore::Cursor::Type type = static_cast<WebCore::Cursor::Type>(typeInt);

        if (type != WebCore::Cursor::Custom) {
            cursor = WebCore::Cursor::fromType(type);
            return true;
        }

#if !USE(CG)
        return false;
#else
        RefPtr<WebCore::Image> image;
        if (!decodeImage(decoder, image))
            return false;
        WebCore::IntPoint hotSpot;
        if (!decoder->decode(hotSpot))
            return false;
        if (!image->rect().contains(WebCore::IntRect(hotSpot, WebCore::IntSize())))
            return false;

        cursor = WebCore::Cursor(image.get(), hotSpot);
        return true;
#endif
    }
};

#endif

// These two functions are implemented in a platform specific manner.
void encodeResourceRequest(ArgumentEncoder*, const WebCore::ResourceRequest&);
bool decodeResourceRequest(ArgumentDecoder*, WebCore::ResourceRequest&);

template<> struct ArgumentCoder<WebCore::ResourceRequest> {
    static void encode(ArgumentEncoder* encoder, const WebCore::ResourceRequest& resourceRequest)
    {
        encodeResourceRequest(encoder, resourceRequest);
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::ResourceRequest& resourceRequest)
    {
        return decodeResourceRequest(decoder, resourceRequest);
    }
};

// These two functions are implemented in a platform specific manner.
void encodeResourceResponse(ArgumentEncoder*, const WebCore::ResourceResponse&);
bool decodeResourceResponse(ArgumentDecoder*, WebCore::ResourceResponse&);

template<> struct ArgumentCoder<WebCore::ResourceResponse> {
    static void encode(ArgumentEncoder* encoder, const WebCore::ResourceResponse& resourceResponse)
    {
        encodeResourceResponse(encoder, resourceResponse);
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::ResourceResponse& resourceResponse)
    {
        return decodeResourceResponse(decoder, resourceResponse);
    }
};

// These two functions are implemented in a platform specific manner.
void encodeResourceError(ArgumentEncoder*, const WebCore::ResourceError&);
bool decodeResourceError(ArgumentDecoder*, WebCore::ResourceError&);

template<> struct ArgumentCoder<WebCore::ResourceError> {
    static void encode(ArgumentEncoder* encoder, const WebCore::ResourceError& resourceError)
    {
        encodeResourceError(encoder, resourceError);
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::ResourceError& resourceError)
    {
        return decodeResourceError(decoder, resourceError);
    }
};

template<> struct ArgumentCoder<WebCore::WindowFeatures> {
    static void encode(ArgumentEncoder* encoder, const WebCore::WindowFeatures& windowFeatures)
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
    
    static bool decode(ArgumentDecoder* decoder, WebCore::WindowFeatures& windowFeatures)
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
};

template<> struct ArgumentCoder<WebCore::Color> {
    static void encode(ArgumentEncoder* encoder, const WebCore::Color& color)
    {
        if (!color.isValid()) {
            encoder->encodeBool(false);
            return;
        }

        encoder->encodeBool(true);
        encoder->encode(color.rgb());
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::Color& color)
    {
        bool isValid;
        if (!decoder->decode(isValid))
            return false;

        if (!isValid) {
            color = WebCore::Color();
            return true;
        }

        WebCore::RGBA32 rgba;
        if (!decoder->decode(rgba))
            return false;

        color = WebCore::Color(rgba);
        return true;
    }
};

#if PLATFORM(MAC)
template<> struct ArgumentCoder<WebCore::KeypressCommand> {
    static void encode(ArgumentEncoder* encoder, const WebCore::KeypressCommand& keypressCommand)
    {
        encoder->encode(CoreIPC::In(keypressCommand.commandName, keypressCommand.text));
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::KeypressCommand& keypressCommand)
    {
        return decoder->decode(CoreIPC::Out(keypressCommand.commandName, keypressCommand.text));
    }
};
#endif

template<> struct ArgumentCoder<WebCore::CompositionUnderline> {
    static void encode(ArgumentEncoder* encoder, const WebCore::CompositionUnderline& underline)
    {
        encoder->encode(CoreIPC::In(underline.startOffset, underline.endOffset, underline.thick, underline.color));
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::CompositionUnderline& underline)
    {
        return decoder->decode(CoreIPC::Out(underline.startOffset, underline.endOffset, underline.thick, underline.color));
    }
};

template<> struct ArgumentCoder<WebCore::DatabaseDetails> {
    static void encode(ArgumentEncoder* encoder, const WebCore::DatabaseDetails& details)
    {
        encoder->encode(CoreIPC::In(details.name(), details.displayName(), details.expectedUsage(), details.currentUsage()));
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::DatabaseDetails& details)
    {
        String name;
        String displayName;
        uint64_t expectedUsage;
        uint64_t currentUsage;
        if (!decoder->decode(CoreIPC::Out(name, displayName, expectedUsage, currentUsage)))
            return false;
        
        details = WebCore::DatabaseDetails(name, displayName, expectedUsage, currentUsage);
        return true;
    }
};

template<> struct ArgumentCoder<WebCore::GrammarDetail> {
    static void encode(ArgumentEncoder* encoder, const WebCore::GrammarDetail& detail)
    {
        encoder->encodeInt32(detail.location);
        encoder->encodeInt32(detail.length);
        encoder->encode(detail.guesses);
        encoder->encode(detail.userDescription);
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::GrammarDetail& detail)
    {
        if (!decoder->decodeInt32(detail.location))
            return false;
        if (!decoder->decodeInt32(detail.length))
            return false;
        if (!decoder->decode(detail.guesses))
            return false;
        if (!decoder->decode(detail.userDescription))
            return false;

        return true;
    }
};

template<> struct ArgumentCoder<WebCore::TextCheckingResult> {
    static void encode(ArgumentEncoder* encoder, const WebCore::TextCheckingResult& result)
    {
        encoder->encodeEnum(result.type);
        encoder->encodeInt32(result.location);
        encoder->encodeInt32(result.length);
        encoder->encode(result.details);
        encoder->encode(result.replacement);
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::TextCheckingResult& result)
    {
        if (!decoder->decodeEnum(result.type))
            return false;
        if (!decoder->decodeInt32(result.location))
            return false;
        if (!decoder->decodeInt32(result.length))
            return false;
        if (!decoder->decode(result.details))
            return false;
        if (!decoder->decode(result.replacement))
            return false;
        return true;
    }
};

template<> struct ArgumentCoder<RefPtr<WebCore::TimingFunction> > {
    static void encode(ArgumentEncoder* encoder, const RefPtr<WebCore::TimingFunction>& function)
    {
        // We don't want to encode null-references.
        ASSERT(function);

        WebCore::TimingFunction::TimingFunctionType type = function->type();
        encoder->encodeInt32(type);
        switch (type) {
        case WebCore::TimingFunction::LinearFunction:
            break;
        case WebCore::TimingFunction::CubicBezierFunction: {
            WebCore::CubicBezierTimingFunction* cubicFunction = static_cast<WebCore::CubicBezierTimingFunction*>(function.get());
            encoder->encodeDouble(cubicFunction->x1());
            encoder->encodeDouble(cubicFunction->y1());
            encoder->encodeDouble(cubicFunction->x2());
            encoder->encodeDouble(cubicFunction->y2());
            break;
        }
        case WebCore::TimingFunction::StepsFunction: {
            WebCore::StepsTimingFunction* stepsFunction = static_cast<WebCore::StepsTimingFunction*>(function.get());
            encoder->encodeInt32(stepsFunction->numberOfSteps());
            encoder->encodeBool(stepsFunction->stepAtStart());
            break;
        }
        }
    }

    static bool decode(ArgumentDecoder* decoder, RefPtr<WebCore::TimingFunction>& function)
    {
        WebCore::TimingFunction::TimingFunctionType type;
        int typeInt;
        if (!decoder->decodeInt32(typeInt))
            return false;

        type = static_cast<WebCore::TimingFunction::TimingFunctionType>(typeInt);
        switch (type) {
        case WebCore::TimingFunction::LinearFunction:
            function = WebCore::LinearTimingFunction::create();
            return true;

        case WebCore::TimingFunction::CubicBezierFunction: {
            double x1, y1, x2, y2;
            if (!decoder->decodeDouble(x1))
                return false;
            if (!decoder->decodeDouble(y1))
                return false;
            if (!decoder->decodeDouble(x2))
                return false;
            if (!decoder->decodeDouble(y2))
                return false;
            function = WebCore::CubicBezierTimingFunction::create(x1, y1, x2, y2);
            return true;
        }

        case WebCore::TimingFunction::StepsFunction: {
            int numSteps;
            bool stepAtStart;
            if (!decoder->decodeInt32(numSteps))
                return false;
            if (!decoder->decodeBool(stepAtStart))
                return false;

            function = WebCore::StepsTimingFunction::create(numSteps, stepAtStart);
            return true;
        }

        }

        return false;
    }
};

template<> struct ArgumentCoder<RefPtr<WebCore::TransformOperation> > {
    template<class T>
    static bool decodeOperation(ArgumentDecoder* decoder, RefPtr<WebCore::TransformOperation>& operation, PassRefPtr<T> newOperation)
    {
        if (!ArgumentCoder<T>::decode(decoder, *newOperation.get()))
            return false;
        operation = newOperation.get();
        return true;
    }

    template<class T>
    static void encodeOperation(ArgumentEncoder* encoder, const WebCore::TransformOperation* operation)
    {
        ArgumentCoder<T>::encode(encoder, *static_cast<const T*>(operation));
    }

    static void encode(ArgumentEncoder* encoder, const RefPtr<WebCore::TransformOperation>& operation)
    {
        // We don't want to encode null-references.
        ASSERT(operation);

        WebCore::TransformOperation::OperationType type = operation->getOperationType();
        encoder->encodeInt32(type);
        switch (type) {
        case WebCore::TransformOperation::SCALE:
        case WebCore::TransformOperation::SCALE_X:
        case WebCore::TransformOperation::SCALE_Y:
        case WebCore::TransformOperation::SCALE_Z:
        case WebCore::TransformOperation::SCALE_3D:
            encodeOperation<WebCore::ScaleTransformOperation>(encoder, operation.get());
            return;

        case WebCore::TransformOperation::TRANSLATE:
        case WebCore::TransformOperation::TRANSLATE_X:
        case WebCore::TransformOperation::TRANSLATE_Y:
        case WebCore::TransformOperation::TRANSLATE_Z:
        case WebCore::TransformOperation::TRANSLATE_3D:
            encodeOperation<WebCore::TranslateTransformOperation>(encoder, operation.get());
            return;

        case WebCore::TransformOperation::ROTATE:
        case WebCore::TransformOperation::ROTATE_X:
        case WebCore::TransformOperation::ROTATE_Y:
        case WebCore::TransformOperation::ROTATE_3D:
            encodeOperation<WebCore::RotateTransformOperation>(encoder, operation.get());
            return;

        case WebCore::TransformOperation::SKEW:
        case WebCore::TransformOperation::SKEW_X:
        case WebCore::TransformOperation::SKEW_Y:
            encodeOperation<WebCore::SkewTransformOperation>(encoder, operation.get());
            return;

        case WebCore::TransformOperation::MATRIX:
            encodeOperation<WebCore::MatrixTransformOperation>(encoder, operation.get());
            return;

        case WebCore::TransformOperation::MATRIX_3D:
            encodeOperation<WebCore::Matrix3DTransformOperation>(encoder, operation.get());
            return;

        case WebCore::TransformOperation::PERSPECTIVE:
            encodeOperation<WebCore::PerspectiveTransformOperation>(encoder, operation.get());
            return;

        case WebCore::TransformOperation::IDENTITY:
        case WebCore::TransformOperation::NONE:
            return;
        }
    }

    static bool decode(ArgumentDecoder* decoder, RefPtr<WebCore::TransformOperation>& operation)
    {
        WebCore::TransformOperation::OperationType type;
        int typeInt;
        if (!decoder->decodeInt32(typeInt))
            return false;
        type = static_cast<WebCore::TransformOperation::OperationType>(typeInt);
        switch (type) {
        case WebCore::TransformOperation::SCALE:
        case WebCore::TransformOperation::SCALE_X:
        case WebCore::TransformOperation::SCALE_Y:
        case WebCore::TransformOperation::SCALE_Z:
        case WebCore::TransformOperation::SCALE_3D:
            return decodeOperation<WebCore::ScaleTransformOperation>(decoder, operation, WebCore::ScaleTransformOperation::create(1.0, 1.0, type));

        case WebCore::TransformOperation::TRANSLATE:
        case WebCore::TransformOperation::TRANSLATE_X:
        case WebCore::TransformOperation::TRANSLATE_Y:
        case WebCore::TransformOperation::TRANSLATE_Z:
        case WebCore::TransformOperation::TRANSLATE_3D:
            return decodeOperation<WebCore::TranslateTransformOperation>(decoder, operation, WebCore::TranslateTransformOperation::create(WebCore::Length(0, WebCore::Fixed), WebCore::Length(0, WebCore::Fixed), type));

        case WebCore::TransformOperation::ROTATE:
        case WebCore::TransformOperation::ROTATE_X:
        case WebCore::TransformOperation::ROTATE_Y:
        case WebCore::TransformOperation::ROTATE_3D:
            return decodeOperation<WebCore::RotateTransformOperation>(decoder, operation, WebCore::RotateTransformOperation::create(0.0, type));

        case WebCore::TransformOperation::SKEW:
        case WebCore::TransformOperation::SKEW_X:
        case WebCore::TransformOperation::SKEW_Y:
            return decodeOperation<WebCore::SkewTransformOperation>(decoder, operation, WebCore::SkewTransformOperation::create(0.0, 0.0, type));

        case WebCore::TransformOperation::MATRIX:
            return decodeOperation<WebCore::MatrixTransformOperation>(decoder, operation, WebCore::MatrixTransformOperation::create(WebCore::TransformationMatrix()));

        case WebCore::TransformOperation::MATRIX_3D:
            return decodeOperation<WebCore::Matrix3DTransformOperation>(decoder, operation, WebCore::Matrix3DTransformOperation::create(WebCore::TransformationMatrix()));

        case WebCore::TransformOperation::PERSPECTIVE:
            return decodeOperation<WebCore::PerspectiveTransformOperation>(decoder, operation, WebCore::PerspectiveTransformOperation::create(WebCore::Length(0, WebCore::Fixed)));

        case WebCore::TransformOperation::IDENTITY:
        case WebCore::TransformOperation::NONE:
            operation = WebCore::IdentityTransformOperation::create();
            return true;
        }

        return false;
    }
};

template<> struct ArgumentCoder<WebCore::TransformOperations> {
    static void encode(ArgumentEncoder* encoder, const WebCore::TransformOperations& operations)
    {
        WTF::Vector<RefPtr<WebCore::TransformOperation> > operationsVector = operations.operations();
        int size = operationsVector.size();
        encoder->encodeInt32(size);
        for (int i = 0; i < size; ++i)
            ArgumentCoder<RefPtr<WebCore::TransformOperation> >::encode(encoder, operationsVector[i]);
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::TransformOperations& operations)
    {
        int size;
        if (!decoder->decodeInt32(size))
            return false;

        WTF::Vector<RefPtr<WebCore::TransformOperation> >& operationVector = operations.operations();
        operationVector.clear();
        operationVector.resize(size);
        for (int i = 0; i < size; ++i) {
            RefPtr<WebCore::TransformOperation> operation;
            if (!ArgumentCoder<RefPtr<WebCore::TransformOperation> >::decode(decoder, operation))
                return false;
            operationVector[i] = operation;
        }

        return true;
    }
};


template<> struct ArgumentCoder<WebCore::Animation> {
    static bool encodeBoolAndReturnValue(ArgumentEncoder* encoder, bool value)
    {
        encoder->encodeBool(value);
        return value;
    }

    template<typename T>
    static void encodeBoolAndValue(ArgumentEncoder* encoder, bool isSet, const T& value)
    {
        if (encodeBoolAndReturnValue(encoder, isSet))
            encoder->encode<T>(value);
    }

    static bool decodeBoolAndValue(ArgumentDecoder* decoder, bool& isSet, double& value)
    {
        if (!decoder->decodeBool(isSet))
            return false;
        if (!isSet)
            return true;

        return decoder->decodeDouble(value);
    }

    template<class T>
    static bool decodeBoolAndValue(ArgumentDecoder* decoder, bool& isSet, T& value)
    {
        if (!decoder->decodeBool(isSet))
            return false;
        if (!isSet)
            return true;

        return ArgumentCoder<T>::decode(decoder, value);
    }

    static bool decodeBoolAndValue(ArgumentDecoder* decoder, bool& isSet, int& value)
    {
        if (!decoder->decodeBool(isSet))
            return false;
        if (!isSet)
            return true;

        return decoder->decodeInt32(value);
    }

    static void encode(ArgumentEncoder* encoder, const WebCore::Animation& animation)
    {
        encodeBoolAndValue(encoder, animation.isDelaySet(), animation.delay());
        encodeBoolAndValue<int>(encoder, animation.isDirectionSet(), animation.direction());
        encodeBoolAndValue(encoder, animation.isDurationSet(), animation.duration());
        encodeBoolAndValue(encoder, animation.isFillModeSet(), animation.fillMode());
        encodeBoolAndValue(encoder, animation.isIterationCountSet(), animation.iterationCount());

        if (encodeBoolAndReturnValue(encoder, animation.isNameSet()))
            ArgumentCoder<String>::encode(encoder, animation.name());

        encodeBoolAndValue<int>(encoder, animation.isPlayStateSet(), animation.playState());
        encodeBoolAndValue(encoder, animation.isPropertySet(), animation.property());

        if (encodeBoolAndReturnValue(encoder, animation.isTimingFunctionSet()))
            ArgumentCoder<RefPtr<WebCore::TimingFunction> >::encode(encoder, animation.timingFunction());
        encoder->encodeBool(animation.isNoneAnimation());
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::Animation& animation)
    {
        bool isDelaySet, isDirectionSet, isDurationSet, isFillModeSet, isIterationCountSet, isNameSet, isPlayStateSet, isPropertySet, isTimingFunctionSet;
        int property, iterationCount, direction, fillMode, playState;
        double delay, duration;
        RefPtr<WebCore::TimingFunction> timingFunction;
        String name;

        animation.clearAll();

        if (!decodeBoolAndValue(decoder, isDelaySet, delay))
            return false;
        if (!decodeBoolAndValue(decoder, isDirectionSet, direction))
            return false;
        if (!decodeBoolAndValue(decoder, isDurationSet, duration))
            return false;
        if (!decodeBoolAndValue(decoder, isFillModeSet, fillMode))
            return false;
        if (!decodeBoolAndValue(decoder, isIterationCountSet, iterationCount))
            return false;
        if (!decodeBoolAndValue<String>(decoder, isNameSet, name))
            return false;
        if (!decodeBoolAndValue(decoder, isPlayStateSet, playState))
            return false;
        if (!decodeBoolAndValue(decoder, isPropertySet, property))
            return false;
        if (!decodeBoolAndValue<RefPtr<WebCore::TimingFunction> >(decoder, isTimingFunctionSet, timingFunction))
            return false;

        if (isDelaySet)
            animation.setDelay(delay);
        if (isDirectionSet)
            animation.setDirection(static_cast<WebCore::Animation::AnimationDirection>(direction));
        if (isDurationSet)
            animation.setDuration(duration);
        if (isFillModeSet)
            animation.setFillMode(fillMode);
        if (isIterationCountSet)
            animation.setIterationCount(iterationCount);
        if (isNameSet)
            animation.setName(name);
        if (isPlayStateSet)
            animation.setPlayState(static_cast<WebCore::EAnimPlayState>(playState));
        if (isPropertySet)
            animation.setProperty(property);
        if (isTimingFunctionSet)
            animation.setTimingFunction(timingFunction);

        return true;
    }
};

} // namespace CoreIPC

#endif // WebCoreArgumentCoders_h
