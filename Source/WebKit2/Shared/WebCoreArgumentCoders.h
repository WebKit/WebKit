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
    static void encode(ArgumentEncoder*, const WebCore::Credential&);
    static bool decode(ArgumentDecoder*, WebCore::Credential&);
};

#if USE(LAZY_NATIVE_CURSOR)
template<> struct ArgumentCoder<WebCore::Cursor> {
    static void encode(ArgumentEncoder*, const WebCore::Cursor&);
    static bool decode(ArgumentDecoder*, WebCore::Cursor&);
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
    static void encode(ArgumentEncoder*, const WebCore::WindowFeatures&);
    static bool decode(ArgumentDecoder*, WebCore::WindowFeatures&);
};

template<> struct ArgumentCoder<WebCore::Color> {
    static void encode(ArgumentEncoder*, const WebCore::Color&);
    static bool decode(ArgumentDecoder*, WebCore::Color&);
};

#if PLATFORM(MAC)
template<> struct ArgumentCoder<WebCore::KeypressCommand> {
    static void encode(ArgumentEncoder*, const WebCore::KeypressCommand&);
    static bool decode(ArgumentDecoder*, WebCore::KeypressCommand&);
};
#endif

template<> struct ArgumentCoder<WebCore::CompositionUnderline> {
    static void encode(ArgumentEncoder*, const WebCore::CompositionUnderline&);
    static bool decode(ArgumentDecoder*, WebCore::CompositionUnderline&);
};

template<> struct ArgumentCoder<WebCore::DatabaseDetails> {
    static void encode(ArgumentEncoder*, const WebCore::DatabaseDetails&);
    static bool decode(ArgumentDecoder*, WebCore::DatabaseDetails&);
};

template<> struct ArgumentCoder<WebCore::GrammarDetail> {
    static void encode(ArgumentEncoder*, const WebCore::GrammarDetail&);
    static bool decode(ArgumentDecoder*, WebCore::GrammarDetail&);
};

template<> struct ArgumentCoder<WebCore::TextCheckingResult> {
    static void encode(ArgumentEncoder*, const WebCore::TextCheckingResult&);
    static bool decode(ArgumentDecoder*, WebCore::TextCheckingResult&);
};

template<> struct ArgumentCoder<RefPtr<WebCore::TimingFunction> > {
    static void encode(ArgumentEncoder*, const RefPtr<WebCore::TimingFunction>&);
    static bool decode(ArgumentDecoder*, RefPtr<WebCore::TimingFunction>&);
};

template<> struct ArgumentCoder<RefPtr<WebCore::TransformOperation> > {
    static void encode(ArgumentEncoder*, const RefPtr<WebCore::TransformOperation>&);
    static bool decode(ArgumentDecoder*, RefPtr<WebCore::TransformOperation>&);
};

template<> struct ArgumentCoder<WebCore::TransformOperations> {
    static void encode(ArgumentEncoder*, const WebCore::TransformOperations&);
    static bool decode(ArgumentDecoder*, WebCore::TransformOperations&);
};

template<> struct ArgumentCoder<WebCore::Animation> {
    static void encode(ArgumentEncoder*, const WebCore::Animation&);
    static bool decode(ArgumentDecoder*, WebCore::Animation&);
};

} // namespace CoreIPC

#endif // WebCoreArgumentCoders_h
