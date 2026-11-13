/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/DisplayListItems.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class SourceBrush;
class SourceImage;

namespace DisplayList {

class DisplayList;

class ResourceCache {
    WTF_MAKE_NONCOPYABLE(ResourceCache);
public:
    WEBCORE_EXPORT ResourceCache();
    WEBCORE_EXPORT ~ResourceCache();

    WEBCORE_EXPORT void appendFont(Ref<const FontBase>&&);
    WEBCORE_EXPORT bool cacheNativeImage(RenderingResourceIdentifier, Ref<NativeImage>&&);
    WEBCORE_EXPORT bool cacheImageBuffer(RenderingResourceIdentifier, Ref<ImageBuffer>&&);
    WEBCORE_EXPORT void appendDisplayList(Ref<const DisplayList>&&);
    WEBCORE_EXPORT bool cachePDFDocument(RenderingResourceIdentifier, Ref<PDFDocumentData>&&);

    bool resolveItem(Item&) const;

private:
    bool resolve(ClipToNativeImage&) const;
    bool resolve(DrawDisplayList&) const;
    bool resolve(DrawFilteredImageBuffer&) const;
    bool resolve(DrawGlyphs&) const;
    bool resolve(DrawImageBuffer&) const;
    bool resolve(DrawNativeImage&) const;
    bool resolve(DrawPDFPage&) const;
    bool resolve(DrawPatternImageBuffer&) const;
    bool resolve(DrawPatternNativeImage&) const;
    bool resolve(SetState&) const;
    template<typename T> bool resolve(T&) const { return true; }

    bool resolveNativeImage(auto& item) const;
    bool resolveImageBuffer(auto& item) const;
    bool resolvePattern(const SourceBrush&) const;
    std::optional<SourceImage> sourceImage(RenderingResourceIdentifier) const;

    Vector<Ref<const FontBase>> m_fonts;
    HashMap<RenderingResourceIdentifier, Ref<NativeImage>> m_nativeImages;
    HashMap<RenderingResourceIdentifier, Ref<ImageBuffer>> m_imageBuffers;
    Vector<Ref<const DisplayList>> m_displayLists;
    HashMap<RenderingResourceIdentifier, Ref<PDFDocumentData>> m_pdfDocuments;
};

} // namespace DisplayList
} // namespace WebCore
