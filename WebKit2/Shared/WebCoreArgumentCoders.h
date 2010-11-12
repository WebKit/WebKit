/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include <WebCore/Cursor.h>
#include <WebCore/FloatRect.h>
#include <WebCore/IntRect.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/PluginData.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <limits>

namespace CoreIPC {

template<> struct ArgumentCoder<WebCore::IntPoint> : SimpleArgumentCoder<WebCore::IntPoint> { };
template<> struct ArgumentCoder<WebCore::IntSize> : SimpleArgumentCoder<WebCore::IntSize> { };
template<> struct ArgumentCoder<WebCore::IntRect> : SimpleArgumentCoder<WebCore::IntRect> { };
template<> struct ArgumentCoder<WebCore::ViewportArguments> : SimpleArgumentCoder<WebCore::ViewportArguments> { };

template<> struct ArgumentCoder<WebCore::FloatPoint> : SimpleArgumentCoder<WebCore::FloatPoint> { };
template<> struct ArgumentCoder<WebCore::FloatSize> : SimpleArgumentCoder<WebCore::FloatSize> { };
template<> struct ArgumentCoder<WebCore::FloatRect> : SimpleArgumentCoder<WebCore::FloatRect> { };

template<> struct ArgumentCoder<WebCore::MimeClassInfo> {
    static void encode(ArgumentEncoder* encoder, const WebCore::MimeClassInfo& mimeClassInfo)
    {
        encoder->encode(mimeClassInfo.type);
        encoder->encode(mimeClassInfo.desc);
        encoder->encode(mimeClassInfo.extensions);
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::MimeClassInfo& mimeClassInfo)
    {
        if (!decoder->decode(mimeClassInfo.type))
            return false;
        if (!decoder->decode(mimeClassInfo.desc))
            return false;
        if (!decoder->decode(mimeClassInfo.extensions))
            return false;

        return true;
    }
};
    
template<> struct ArgumentCoder<WebCore::PluginInfo> {
    static void encode(ArgumentEncoder* encoder, const WebCore::PluginInfo& pluginInfo)
    {
        encoder->encode(pluginInfo.name);
        encoder->encode(pluginInfo.file);
        encoder->encode(pluginInfo.desc);
        encoder->encode(pluginInfo.mimes);
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::PluginInfo& pluginInfo)
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
};

template<> struct ArgumentCoder<WebCore::HTTPHeaderMap> {
    static void encode(ArgumentEncoder* encoder, const WebCore::HTTPHeaderMap& headerMap)
    {
        encoder->encode(static_cast<const HashMap<AtomicString, String, CaseFoldingHash>&>(headerMap));
    }

    static bool decode(ArgumentDecoder* decoder, WebCore::HTTPHeaderMap& headerMap)
    {
        return decoder->decode(static_cast<HashMap<AtomicString, String, CaseFoldingHash>&>(headerMap));
    }
};

#if USE(LAZY_NATIVE_CURSOR)
template<> struct ArgumentCoder<WebCore::Cursor> {
    static void encode(ArgumentEncoder* encoder, const WebCore::Cursor& cursor)
    {
        // FIXME: Support custom cursors.
        if (cursor.type() == WebCore::Cursor::Custom)
            encoder->encode(static_cast<uint32_t>(WebCore::Cursor::Pointer));
        else
            encoder->encode(static_cast<uint32_t>(cursor.type()));
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::Cursor& cursor)
    {
        uint32_t typeInt;
        if (!decoder->decode(typeInt))
            return false;

        WebCore::Cursor::Type type = static_cast<WebCore::Cursor::Type>(typeInt);
        ASSERT(type != WebCore::Cursor::Custom);

        cursor = WebCore::Cursor::fromType(type);
        return true;
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

template<> struct ArgumentCoder<WebCore::ResourceError> {
    static void encode(ArgumentEncoder* encoder, const WebCore::ResourceError& resourceError)
    {
        encoder->encode(CoreIPC::In(resourceError.domain(), resourceError.errorCode(), resourceError.failingURL(), resourceError.localizedDescription()));
    }
    
    static bool decode(ArgumentDecoder* decoder, WebCore::ResourceError& resourceError)
    {
        String domain;
        int errorCode;
        String failingURL;
        String localizedDescription;
        if (!decoder->decode(CoreIPC::Out(domain, errorCode, failingURL, localizedDescription)))
            return false;
        resourceError = WebCore::ResourceError(domain, errorCode, failingURL, localizedDescription);
        return true;
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
    
} // namespace CoreIPC

#endif // WebCoreArgumentCoders_h
