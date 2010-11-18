/*
 * Copyright (c) 2006, 2007, 2008, Google Inc. All rights reserved.
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
#include "FontPlatformData.h"

#include "ChromiumBridge.h"
#include "HarfbuzzSkia.h"
#include "NotImplemented.h"
#include "PlatformString.h"

#include "SkPaint.h"
#include "SkTypeface.h"

#include <wtf/text/StringImpl.h> 

namespace WebCore {

static SkPaint::Hinting skiaHinting = SkPaint::kNormal_Hinting;
static bool isSkiaAntiAlias = true;
static bool isSkiaSubpixelGlyphs = false;

void FontPlatformData::setHinting(SkPaint::Hinting hinting)
{
    skiaHinting = hinting;
}

void FontPlatformData::setAntiAlias(bool isAntiAlias)
{
    isSkiaAntiAlias = isAntiAlias;
}

void FontPlatformData::setSubpixelGlyphs(bool isSubpixelGlyphs)
{
    isSkiaSubpixelGlyphs = isSubpixelGlyphs;
}

FontPlatformData::RefCountedHarfbuzzFace::~RefCountedHarfbuzzFace()
{
    HB_FreeFace(m_harfbuzzFace);
}

FontPlatformData::FontPlatformData(const FontPlatformData& src)
    : m_typeface(src.m_typeface)
    , m_family(src.m_family)
    , m_textSize(src.m_textSize)
    , m_fakeBold(src.m_fakeBold)
    , m_fakeItalic(src.m_fakeItalic)
    , m_style(src.m_style)
    , m_harfbuzzFace(src.m_harfbuzzFace)
{
    SkSafeRef(m_typeface);
}

FontPlatformData::FontPlatformData(SkTypeface* tf, const char* family, float textSize, bool fakeBold, bool fakeItalic)
    : m_typeface(tf)
    , m_family(family)
    , m_textSize(textSize)
    , m_fakeBold(fakeBold)
    , m_fakeItalic(fakeItalic)
{
    SkSafeRef(m_typeface);
    querySystemForRenderStyle();
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, float textSize)
    : m_typeface(src.m_typeface)
    , m_family(src.m_family)
    , m_textSize(textSize)
    , m_fakeBold(src.m_fakeBold)
    , m_fakeItalic(src.m_fakeItalic)
    , m_harfbuzzFace(src.m_harfbuzzFace)
{
    SkSafeRef(m_typeface);
    querySystemForRenderStyle();
}

FontPlatformData::~FontPlatformData()
{
    SkSafeUnref(m_typeface);
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& src)
{
    SkRefCnt_SafeAssign(m_typeface, src.m_typeface);

    m_family = src.m_family;
    m_textSize = src.m_textSize;
    m_fakeBold = src.m_fakeBold;
    m_fakeItalic = src.m_fakeItalic;
    m_harfbuzzFace = src.m_harfbuzzFace;
    m_style = src.m_style;

    return *this;
}

#ifndef NDEBUG
String FontPlatformData::description() const
{
    return String();
}
#endif

void FontPlatformData::setupPaint(SkPaint* paint) const
{
    const float ts = m_textSize >= 0 ? m_textSize : 12;

    paint->setAntiAlias(m_style.useAntiAlias == FontRenderStyle::NoPreference ? isSkiaAntiAlias : m_style.useAntiAlias);
    switch (m_style.useHinting) {
    case FontRenderStyle::NoPreference:
        paint->setHinting(skiaHinting);
        break;
    case 0:
        paint->setHinting(SkPaint::kNo_Hinting);
        break;
    default:
        paint->setHinting(static_cast<SkPaint::Hinting>(m_style.hintStyle));
        break;
    }

    paint->setEmbeddedBitmapText(m_style.useBitmaps);
    paint->setTextSize(SkFloatToScalar(ts));
    paint->setTypeface(m_typeface);
    paint->setFakeBoldText(m_fakeBold);
    paint->setTextSkewX(m_fakeItalic ? -SK_Scalar1 / 4 : 0);
    paint->setAutohinted(m_style.useAutoHint);

    if (m_style.useAntiAlias == 1 || (m_style.useAntiAlias == FontRenderStyle::NoPreference && isSkiaAntiAlias))
        paint->setLCDRenderText(m_style.useSubpixel == FontRenderStyle::NoPreference ? isSkiaSubpixelGlyphs : m_style.useSubpixel);
}

SkFontID FontPlatformData::uniqueID() const
{
    return m_typeface->uniqueID();
}

bool FontPlatformData::operator==(const FontPlatformData& a) const
{
    // If either of the typeface pointers are invalid (either NULL or the
    // special deleted value) then we test for pointer equality. Otherwise, we
    // call SkTypeface::Equal on the valid pointers.
    bool typefacesEqual;
    if (m_typeface == hashTableDeletedFontValue()
        || a.m_typeface == hashTableDeletedFontValue()
        || !m_typeface
        || !a.m_typeface)
        typefacesEqual = m_typeface == a.m_typeface;
    else
        typefacesEqual = SkTypeface::Equal(m_typeface, a.m_typeface);

    return typefacesEqual 
        && m_textSize == a.m_textSize
        && m_fakeBold == a.m_fakeBold
        && m_fakeItalic == a.m_fakeItalic
        && m_style == a.m_style;
}

unsigned FontPlatformData::hash() const
{
    unsigned h = SkTypeface::UniqueID(m_typeface);
    h ^= 0x01010101 * ((static_cast<int>(m_fakeBold) << 1) | static_cast<int>(m_fakeItalic));

    // This memcpy is to avoid a reinterpret_cast that breaks strict-aliasing
    // rules. Memcpy is generally optimized enough so that performance doesn't
    // matter here.
    uint32_t textSizeBytes;
    memcpy(&textSizeBytes, &m_textSize, sizeof(uint32_t));
    h ^= textSizeBytes;

    return h;
}

bool FontPlatformData::isFixedPitch() const
{
    notImplemented();
    return false;
}

HB_FaceRec_* FontPlatformData::harfbuzzFace() const
{
    if (!m_harfbuzzFace)
        m_harfbuzzFace = RefCountedHarfbuzzFace::create(HB_NewFace(const_cast<FontPlatformData*>(this), harfbuzzSkiaGetTable));

    return m_harfbuzzFace->face();
}

void FontPlatformData::querySystemForRenderStyle()
{
    if (!m_family.length()) {
        // We don't have a family for this. Probably because it's a webfont. We
        // set all the values to 'no preference' and take the defaults passed
        // in from XSETTINGS.
        m_style.useBitmaps = FontRenderStyle::NoPreference;
        m_style.useAutoHint = FontRenderStyle::NoPreference;
        m_style.useHinting = FontRenderStyle::NoPreference;
        m_style.useAntiAlias = FontRenderStyle::NoPreference;
        m_style.useSubpixel = FontRenderStyle::NoPreference;
        return;
    }

    ChromiumBridge::getRenderStyleForStrike(m_family.data(), (((int)m_textSize) << 2) | (m_typeface->style() & 3), &m_style);
}

}  // namespace WebCore
