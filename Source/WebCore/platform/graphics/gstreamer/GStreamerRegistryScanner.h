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
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
class ContentType;

class GStreamerRegistryScanner {
    friend NeverDestroyed<GStreamerRegistryScanner>;
public:
    static GStreamerRegistryScanner& singleton();

    enum Configuration {
        Decoding = 0,
        Encoding
    };

    const HashSet<String, ASCIICaseInsensitiveHash>& mimeTypeSet(Configuration);
    bool isContainerTypeSupported(Configuration, String containerType) const;

    struct RegistryLookupResult {
        bool isSupported;
        bool isUsingHardware;

        operator bool() const { return isSupported; }
    };
    RegistryLookupResult isDecodingSupported(MediaConfiguration& mediaConfiguration) const { return isConfigurationSupported(Configuration::Decoding, mediaConfiguration); };
    RegistryLookupResult isEncodingSupported(MediaConfiguration& mediaConfiguration) const { return isConfigurationSupported(Configuration::Encoding, mediaConfiguration); }

    bool isCodecSupported(Configuration, String codec, bool usingHardware = false) const;
    MediaPlayerEnums::SupportsType isContentTypeSupported(Configuration, const ContentType&, const Vector<ContentType>& contentTypesRequiringHardwareSupport) const;
    bool areAllCodecsSupported(Configuration, const Vector<String>& codecs, bool shouldCheckForHardwareUse = false) const;

protected:
    GStreamerRegistryScanner(bool isMediaSource = false);
    ~GStreamerRegistryScanner();

    void initializeDecoders();
    void initializeEncoders();

    RegistryLookupResult isConfigurationSupported(Configuration, MediaConfiguration&) const;

    enum ElementType {
        AudioDecoder = 0,
        VideoDecoder,
        Demuxer
    };

    struct GstCapsWebKitMapping {
        ElementType elementType;
        const char* capsString;
        Vector<AtomString> webkitMimeTypes;
        Vector<AtomString> webkitCodecPatterns;
    };
    void fillMimeTypeSetFromCapsMapping(Vector<GstCapsWebKitMapping>&);

    RegistryLookupResult hasElementForMediaType(GList* elementFactories, const char* capsString, bool shouldCheckHardwareClassifier = false, Optional<Vector<String>> blackList = WTF::nullopt) const;

    bool isAVC1CodecSupported(Configuration, const String& codec, bool shouldCheckForHardwareUse) const;

private:
    const char* configurationNameForLogging(Configuration) const;

    bool m_isMediaSource;
    GList* m_audioDecoderFactories;
    GList* m_audioParserFactories;
    GList* m_videoDecoderFactories;
    GList* m_videoParserFactories;
    GList* m_demuxerFactories;
    GList* m_audioEncoderFactories;
    GList* m_videoEncoderFactories;
    GList* m_muxerFactories;
    HashSet<String, ASCIICaseInsensitiveHash> m_decoderMimeTypeSet;
    HashMap<AtomString, bool> m_decoderCodecMap;
    HashSet<String, ASCIICaseInsensitiveHash> m_encoderMimeTypeSet;
    HashMap<AtomString, bool> m_encoderCodecMap;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
