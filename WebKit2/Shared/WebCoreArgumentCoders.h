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
#include <WebCore/PluginData.h>
#include <WebCore/ResourceRequest.h>
#include <limits>
#include <wtf/text/WTFString.h>

namespace CoreIPC {

template<> struct ArgumentCoder<WebCore::IntPoint> : SimpleArgumentCoder<WebCore::IntPoint> { };
template<> struct ArgumentCoder<WebCore::IntSize> : SimpleArgumentCoder<WebCore::IntSize> { };
template<> struct ArgumentCoder<WebCore::IntRect> : SimpleArgumentCoder<WebCore::IntRect> { };

template<> struct ArgumentCoder<WTF::String> {
    static void encode(ArgumentEncoder* encoder, const WTF::String& string)
    {
        // Special case the null string.
        if (string.isNull()) {
            encoder->encodeUInt32(std::numeric_limits<uint32_t>::max());
            return;
        }

        uint32_t length = string.length();
        encoder->encode(length);
        encoder->encodeBytes(reinterpret_cast<const uint8_t*>(string.characters()), length * sizeof(UChar));
    }
    
    static bool decode(ArgumentDecoder* decoder, WTF::String& s)
    {
        uint32_t length;
        if (!decoder->decode(length))
            return false;

        if (length == std::numeric_limits<uint32_t>::max()) {
            // This is the null string.
            s = String();
            return true;
        }

        // Before allocating the string, make sure that the decoder buffer is big enough.
        if (!decoder->bufferIsLargeEnoughToContain<UChar>(length)) {
            decoder->markInvalid();
            return false;
        }
        
        UChar* buffer;
        WTF::String string = WTF::String::createUninitialized(length, buffer);
        if (!decoder->decodeBytes(reinterpret_cast<uint8_t*>(buffer), length * sizeof(UChar)))
            return false;
        
        s = string;
        return true;
    }
};

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

} // namespace CoreIPC

#endif // WebCoreArgumentCoders_h
