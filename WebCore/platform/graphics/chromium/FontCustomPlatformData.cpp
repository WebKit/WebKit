/*
 * Copyright (C) 2007 Apple Computer, Inc.
 * Copyright (c) 2007, 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontCustomPlatformData.h"

#if PLATFORM(WIN_OS)
#include "Base64.h"
#include "ChromiumBridge.h"
#include "OpenTypeUtilities.h"
#endif

#include "FontPlatformData.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"

#if PLATFORM(WIN_OS)
#include <objbase.h>
#include <t2embapi.h>
#pragma comment(lib, "t2embed")
#endif

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
#if PLATFORM(WIN_OS)
    if (m_fontReference) {
        if (m_name.isNull()) {
            ULONG status;
            TTDeleteEmbeddedFont(m_fontReference, 0, &status);
        } else
            RemoveFontMemResourceEx(m_fontReference);
    }
#endif
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic, FontRenderingMode mode)
{
#if PLATFORM(WIN_OS)
    ASSERT(m_fontReference);

    LOGFONT logFont;
    if (m_name.isNull())
        TTGetNewFontName(&m_fontReference, logFont.lfFaceName, LF_FACESIZE, 0, 0);
    else {
        // m_name comes from createUniqueFontName, which, in turn, gets
        // it from base64-encoded uuid (128-bit). So, m_name
        // can never be longer than LF_FACESIZE (32).
        if (m_name.length() + 1 >= LF_FACESIZE) {
            ASSERT_NOT_REACHED();
            return FontPlatformData();
        }
        memcpy(logFont.lfFaceName, m_name.charactersWithNullTermination(),
               sizeof(logFont.lfFaceName[0]) * (1 + m_name.length()));
    }

    // FIXME: almost identical to FillLogFont in FontCacheWin.cpp.
    // Need to refactor. 
    logFont.lfHeight = -size;
    logFont.lfWidth = 0;
    logFont.lfEscapement = 0;
    logFont.lfOrientation = 0;
    logFont.lfUnderline = false;
    logFont.lfStrikeOut = false;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    logFont.lfQuality = ChromiumBridge::layoutTestMode() ?
                        NONANTIALIASED_QUALITY :
                        DEFAULT_QUALITY; // Honor user's desktop settings.
    logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    logFont.lfItalic = italic;
    logFont.lfWeight = bold ? 700 : 400;

    HFONT hfont = CreateFontIndirect(&logFont);
    return FontPlatformData(hfont, size);
#else
    notImplemented();
    return FontPlatformData();
#endif
}

#if PLATFORM(WIN_OS)
// FIXME: EOTStream class and static functions in this #if block are
// duplicated from platform/graphics/win/FontCustomPlatformData.cpp
// and need to be shared.

// Streams the concatenation of a header and font data.
class EOTStream {
public:
    EOTStream(const EOTHeader& eotHeader, const SharedBuffer* fontData, size_t overlayDst, size_t overlaySrc, size_t overlayLength)
        : m_eotHeader(eotHeader)
        , m_fontData(fontData)
        , m_overlayDst(overlayDst)
        , m_overlaySrc(overlaySrc)
        , m_overlayLength(overlayLength)
        , m_offset(0)
        , m_inHeader(true)
    {
    }

    size_t read(void* buffer, size_t count);

private:
    const EOTHeader& m_eotHeader;
    const SharedBuffer* m_fontData;
    size_t m_overlayDst;
    size_t m_overlaySrc;
    size_t m_overlayLength;
    size_t m_offset;
    bool m_inHeader;
};

size_t EOTStream::read(void* buffer, size_t count)
{
    size_t bytesToRead = count;
    if (m_inHeader) {
        size_t bytesFromHeader = std::min(m_eotHeader.size() - m_offset, count);
        memcpy(buffer, m_eotHeader.data() + m_offset, bytesFromHeader);
        m_offset += bytesFromHeader;
        bytesToRead -= bytesFromHeader;
        if (m_offset == m_eotHeader.size()) {
            m_inHeader = false;
            m_offset = 0;
        }
    }
    if (bytesToRead && !m_inHeader) {
        size_t bytesFromData = std::min(m_fontData->size() - m_offset, bytesToRead);
        memcpy(buffer, m_fontData->data() + m_offset, bytesFromData);
        if (m_offset < m_overlayDst + m_overlayLength && m_offset + bytesFromData >= m_overlayDst) {
            size_t dstOffset = std::max<int>(m_overlayDst - m_offset, 0);
            size_t srcOffset = std::max<int>(0, m_offset - m_overlayDst);
            size_t bytesToCopy = std::min(bytesFromData - dstOffset, m_overlayLength - srcOffset);
            memcpy(reinterpret_cast<char*>(buffer) + dstOffset, m_fontData->data() + m_overlaySrc + srcOffset, bytesToCopy);
        }
        m_offset += bytesFromData;
        bytesToRead -= bytesFromData;
    }
    return count - bytesToRead;
}

static unsigned long WINAPIV readEmbedProc(void* stream, void* buffer, unsigned long length)
{
    return static_cast<EOTStream*>(stream)->read(buffer, length);
}

// Creates a unique and unpredictable font name, in order to avoid collisions and to
// not allow access from CSS.
static String createUniqueFontName()
{
    Vector<char> fontUuid(sizeof(GUID));
    CoCreateGuid(reinterpret_cast<GUID*>(fontUuid.data()));

    Vector<char> fontNameVector;
    base64Encode(fontUuid, fontNameVector);
    ASSERT(fontNameVector.size() < LF_FACESIZE);
    return String(fontNameVector.data(), fontNameVector.size());
}
#endif

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer* buffer)
{
    ASSERT_ARG(buffer, buffer);

#if PLATFORM(WIN_OS)
    // Introduce the font to GDI. AddFontMemResourceEx cannot be used, because it will pollute the process's
    // font namespace (Windows has no API for creating an HFONT from data without exposing the font to the
    // entire process first). TTLoadEmbeddedFont lets us override the font family name, so using a unique name
    // we avoid namespace collisions.

    String fontName = createUniqueFontName();

    // TTLoadEmbeddedFont works only with Embedded OpenType (.eot) data,
    // so we need to create an EOT header and prepend it to the font data.
    EOTHeader eotHeader;
    size_t overlayDst;
    size_t overlaySrc;
    size_t overlayLength;

    if (!getEOTHeader(buffer, eotHeader, overlayDst, overlaySrc, overlayLength))
        return 0;

    HANDLE fontReference;
    ULONG privStatus;
    ULONG status;
    EOTStream eotStream(eotHeader, buffer, overlayDst, overlaySrc, overlayLength);

    LONG loadEmbeddedFontResult = TTLoadEmbeddedFont(&fontReference, TTLOAD_PRIVATE, &privStatus, LICENSE_PREVIEWPRINT, &status, readEmbedProc, &eotStream, const_cast<LPWSTR>(fontName.charactersWithNullTermination()), 0, 0);
    if (loadEmbeddedFontResult == E_NONE)
        fontName = String();
    else {
        fontReference = renameAndActivateFont(buffer, fontName);
        if (!fontReference)
            return 0;
    }

    return new FontCustomPlatformData(fontReference, fontName);
#else
    notImplemented();;
    return 0;
#endif
}

}
