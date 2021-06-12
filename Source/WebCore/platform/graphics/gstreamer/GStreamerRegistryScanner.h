/*
 * Copyright (C) 2019 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(GSTREAMER)

#include "MediaConfiguration.h"

#include "MediaPlayerEnums.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
class ContentType;

class GStreamerRegistryScanner {
public:
    static GStreamerRegistryScanner& singleton();
    static void getSupportedDecodingTypes(HashSet<String, ASCIICaseInsensitiveHash>&);

    explicit GStreamerRegistryScanner(bool isMediaSource = false);
    ~GStreamerRegistryScanner() = default;

    enum Configuration {
        Decoding = 0,
        Encoding
    };

    const HashSet<String, ASCIICaseInsensitiveHash>& mimeTypeSet(Configuration) const;
    bool isContainerTypeSupported(Configuration, const String& containerType) const;

    struct RegistryLookupResult {
        bool isSupported { false };
        bool isUsingHardware { false };

        operator bool() const { return isSupported; }
    };
    RegistryLookupResult isDecodingSupported(MediaConfiguration& mediaConfiguration) const { return isConfigurationSupported(Configuration::Decoding, mediaConfiguration); };
    RegistryLookupResult isEncodingSupported(MediaConfiguration& mediaConfiguration) const { return isConfigurationSupported(Configuration::Encoding, mediaConfiguration); }

    bool isCodecSupported(Configuration, const String& codec, bool usingHardware = false) const;
    MediaPlayerEnums::SupportsType isContentTypeSupported(Configuration, const ContentType&, const Vector<ContentType>& contentTypesRequiringHardwareSupport) const;
    bool areAllCodecsSupported(Configuration, const Vector<String>& codecs, bool shouldCheckForHardwareUse = false) const;

protected:
    struct ElementFactories {
        enum class Type {
            AudioParser  = 1 << 0,
            AudioDecoder = 1 << 1,
            VideoParser  = 1 << 2,
            VideoDecoder = 1 << 3,
            Demuxer      = 1 << 4,
            AudioEncoder = 1 << 5,
            VideoEncoder = 1 << 6,
            Muxer        = 1 << 7,
            All          = (1 << 8) - 1
        };

        explicit ElementFactories(OptionSet<Type>);
        ~ElementFactories();

        static const char* elementFactoryTypeToString(Type);
        GList* factory(Type) const;

        enum class CheckHardwareClassifier { No, Yes };
        RegistryLookupResult hasElementForMediaType(Type, const char* capsString, CheckHardwareClassifier = CheckHardwareClassifier::No, std::optional<Vector<String>> disallowedList = std::nullopt) const;

        GList* audioDecoderFactories { nullptr };
        GList* audioParserFactories { nullptr };
        GList* videoDecoderFactories { nullptr };
        GList* videoParserFactories { nullptr };
        GList* demuxerFactories { nullptr };
        GList* audioEncoderFactories { nullptr };
        GList* videoEncoderFactories { nullptr };
        GList* muxerFactories { nullptr };
    };

    void initializeDecoders(const ElementFactories&);
    void initializeEncoders(const ElementFactories&);

    RegistryLookupResult isConfigurationSupported(Configuration, const MediaConfiguration&) const;

    struct GstCapsWebKitMapping {
        ElementFactories::Type elementType;
        const char* capsString;
        Vector<AtomString> webkitMimeTypes;
        Vector<AtomString> webkitCodecPatterns;
    };
    void fillMimeTypeSetFromCapsMapping(const ElementFactories&, const Vector<GstCapsWebKitMapping>&);

    bool isAVC1CodecSupported(Configuration, const String& codec, bool shouldCheckForHardwareUse) const;

private:
    const char* configurationNameForLogging(Configuration) const;

    bool m_isMediaSource { false };
    HashSet<String, ASCIICaseInsensitiveHash> m_decoderMimeTypeSet;
    HashMap<AtomString, bool> m_decoderCodecMap;
    HashSet<String, ASCIICaseInsensitiveHash> m_encoderMimeTypeSet;
    HashMap<AtomString, bool> m_encoderCodecMap;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
