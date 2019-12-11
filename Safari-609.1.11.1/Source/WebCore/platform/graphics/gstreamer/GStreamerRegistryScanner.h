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

    HashSet<String, ASCIICaseInsensitiveHash>& mimeTypeSet() { return m_mimeTypeSet; }

    bool isContainerTypeSupported(String containerType) const { return m_mimeTypeSet.contains(containerType); }

    struct RegistryLookupResult {
        bool isSupported;
        bool isUsingHardware;

        operator bool() const { return isSupported; }
    };
    RegistryLookupResult isDecodingSupported(MediaConfiguration&) const;

    bool isCodecSupported(String codec, bool usingHardware = false) const;
    bool areAllCodecsSupported(const Vector<String>& codecs, bool shouldCheckForHardwareUse = false) const;

protected:
    GStreamerRegistryScanner(bool isMediaSource = false);
    ~GStreamerRegistryScanner();

    void initialize();

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

    RegistryLookupResult hasElementForMediaType(GList* elementFactories, const char* capsString, bool shouldCheckHardwareClassifier = false) const;

    bool isAVC1CodecSupported(const String& codec, bool shouldCheckForHardwareUse) const;

private:
    bool m_isMediaSource;
    GList* m_audioDecoderFactories;
    GList* m_audioParserFactories;
    GList* m_videoDecoderFactories;
    GList* m_videoParserFactories;
    GList* m_demuxerFactories;
    HashSet<String, ASCIICaseInsensitiveHash> m_mimeTypeSet;
    HashMap<AtomString, bool> m_codecMap;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
