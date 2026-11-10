/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

struct TypeExtensionPair {
    ASCIILiteral type;
    ASCIILiteral extension;
};

WEBCORE_EXPORT const std::initializer_list<TypeExtensionPair>& commonMediaTypes();

struct MIMETypeRegistryThreadGlobalData {
    WTF_MAKE_TZONE_ALLOCATED(MIMETypeRegistryThreadGlobalData);
    WTF_MAKE_NONCOPYABLE(MIMETypeRegistryThreadGlobalData);
public:
    MIMETypeRegistryThreadGlobalData(HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash>&& supportedImageMIMETypesForEncoding)
        : m_supportedImageMIMETypesForEncoding(WTF::move(supportedImageMIMETypesForEncoding))
    { }

    const HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash>& supportedImageMIMETypesForEncoding() const LIFETIME_BOUND { return m_supportedImageMIMETypesForEncoding; }

private:
    HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash> m_supportedImageMIMETypesForEncoding;
};

class MIMETypeRegistry {
public:
    WEBCORE_EXPORT static RefPtr<StringImpl> mimeTypeForExtension(StringView);
    WEBCORE_EXPORT static Vector<RefPtr<StringImpl>> extensionsForMIMEType(StringImpl* type);
    WEBCORE_EXPORT static RefPtr<StringImpl> preferredExtensionForMIMEType(StringImpl* type);
    WEBCORE_EXPORT static RefPtr<StringImpl> mediaMIMETypeForExtension(StringView extension);

    WEBCORE_EXPORT static RefPtr<StringImpl> mimeTypeForPath(StringView);

    static std::unique_ptr<MIMETypeRegistryThreadGlobalData> createMIMETypeRegistryThreadGlobalData();

    // Check to see if a MIME type is suitable for being loaded inline as an
    // image (e.g., <img> tags).
    WEBCORE_EXPORT static bool isSupportedImageMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being loaded as an image, including SVG and Video (where supported).
    WEBCORE_EXPORT static bool isSupportedImageVideoOrSVGMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being encoded.
    WEBCORE_EXPORT static bool isSupportedImageMIMETypeForEncoding(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being loaded as a JavaScript or JSON resource.
    NODELETE WEBCORE_EXPORT static bool isSupportedJavaScriptMIMEType(StringView mimeType);
    WEBCORE_EXPORT static bool isSupportedJSONMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being loaded as a WebAssembly module.
    WEBCORE_EXPORT static bool isSupportedWebAssemblyMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being loaded as a style sheet.
    static bool isSupportedStyleSheetMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being loaded as a font.
    static bool isSupportedFontMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is a text media playlist type, such as an m3u8.
    static bool isTextMediaPlaylistMIMEType(StringImpl* mimeType);

    // Check to see if a non-image MIME type is suitable for being loaded as a
    // document in a frame. Does not include supported JavaScript and JSON MIME types.
    WEBCORE_EXPORT static bool isSupportedNonImageMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being loaded using <video> and <audio>.
    WEBCORE_EXPORT static bool isSupportedMediaMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being loaded using <track>>.
    WEBCORE_EXPORT static bool isSupportedTextTrackMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is a plugin implemented by the browser.
    NODELETE static bool isApplicationPluginMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is one of the common PDF/PS types.
    NODELETE WEBCORE_EXPORT static bool isPDFMIMEType(StringImpl* mimeType);

    NODELETE WEBCORE_EXPORT static bool isUSDMIMEType(StringImpl* mimeType);

    NODELETE WEBCORE_EXPORT static bool isGLTFMIMEType(StringImpl* mimeType);

    NODELETE WEBCORE_EXPORT static bool isSupportedModelMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is suitable for being shown inside a page.
    // Returns true if any of isSupportedImageMIMEType(), isSupportedNonImageMIMEType(),
    // isSupportedMediaMIMEType(), isSupportedJavaScriptMIMEType(), isSupportedJSONMIMEType(),
    // returns true or if the given MIME type begins with "text/" and
    // isUnsupportedTextMIMEType() returns false.
    WEBCORE_EXPORT static bool canShowMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is one where an XML document should be created
    // rather than an HTML document.
    WEBCORE_EXPORT static bool isXMLMIMEType(StringImpl* mimeType);

    // Check to see if a MIME type is for an XML external entity resource.
    WEBCORE_EXPORT static bool isXMLEntityMIMEType(StringView mimeType);

    // Used in page load algorithm to decide whether to display as a text
    // document in a frame. Not a good idea to use elsewhere, because that code
    // makes this test is after many other tests are done on the MIME type.
    WEBCORE_EXPORT static bool isTextMIMEType(StringImpl* mimeType);

    WEBCORE_EXPORT static FixedVector<ASCIILiteral> supportedImageMIMETypes();
    NODELETE static HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash>& additionalSupportedImageMIMETypes();
    WEBCORE_EXPORT static HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash>& supportedNonImageMIMETypes();
    WEBCORE_EXPORT static const HashSet<RefPtr<StringImpl>>& supportedMediaMIMETypes();
    WEBCORE_EXPORT static FixedVector<ASCIILiteral> pdfMIMETypes();
    WEBCORE_EXPORT static FixedVector<ASCIILiteral> unsupportedTextMIMETypes();
    WEBCORE_EXPORT static FixedVector<ASCIILiteral> usdMIMETypes();
    WEBCORE_EXPORT static FixedVector<ASCIILiteral> gltfMIMETypes();

    WEBCORE_EXPORT static RefPtr<StringImpl> appendFileExtensionIfNecessary(StringImpl* filename, StringImpl* mimeType);
    WEBCORE_EXPORT static RefPtr<StringImpl> correctExtensionForMIMEType(StringImpl* filename, StringImpl* mimeType);

    WEBCORE_EXPORT static RefPtr<StringImpl> preferredImageMIMETypeForEncoding(const Vector<RefPtr<StringImpl>>& mimeTypes, const Vector<RefPtr<StringImpl>>& extensions);
    WEBCORE_EXPORT static bool containsImageMIMETypeForEncoding(const Vector<RefPtr<StringImpl>>& mimeTypes, const Vector<RefPtr<StringImpl>>& extensions);
    WEBCORE_EXPORT static Vector<RefPtr<StringImpl>> allowedMIMETypes(const Vector<RefPtr<StringImpl>>& mimeTypes, const Vector<RefPtr<StringImpl>>& extensions);
    WEBCORE_EXPORT static Vector<RefPtr<StringImpl>> allowedFileExtensions(const Vector<RefPtr<StringImpl>>& mimeTypes, const Vector<RefPtr<StringImpl>>& extensions);
    WEBCORE_EXPORT static bool isJPEGMIMEType(StringImpl* mimeType);
    WEBCORE_EXPORT static bool isWebArchiveMIMEType(StringImpl* mimeType);
private:
    // Check to see if the MIME type is not suitable for being loaded as a text
    // document in a frame. Only valid for MIME types begining with "text/".
    NODELETE static bool isUnsupportedTextMIMEType(StringImpl* mimeType);
};

WEBCORE_EXPORT const RefPtr<StringImpl>& defaultMIMEType();

} // namespace WebCore
