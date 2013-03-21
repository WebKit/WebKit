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
#include "FontPlatformDataHarfBuzz.h"

#include "FontCache.h"
#include "HarfBuzzFace.h"
#include "NotImplemented.h"
#include "SkAdvancedTypefaceMetrics.h"
#include "SkPaint.h"
#include "SkTypeface.h"

#include <public/linux/WebFontInfo.h>
#include <public/linux/WebFontRenderStyle.h>
#include <public/linux/WebSandboxSupport.h>
#include <public/Platform.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static SkPaint::Hinting skiaHinting = SkPaint::kNormal_Hinting;
static bool useSkiaAutoHint = true;
static bool useSkiaBitmaps = true;
static bool useSkiaAntiAlias = true;
static bool useSkiaSubpixelRendering = false;
static bool useSkiaSubpixelPositioning = false;

void FontPlatformData::setHinting(SkPaint::Hinting hinting)
{
    skiaHinting = hinting;
}

void FontPlatformData::setAutoHint(bool useAutoHint)
{
    useSkiaAutoHint = useAutoHint;
}

void FontPlatformData::setUseBitmaps(bool useBitmaps)
{
    useSkiaBitmaps = useBitmaps;
}

void FontPlatformData::setAntiAlias(bool useAntiAlias)
{
    useSkiaAntiAlias = useAntiAlias;
}

void FontPlatformData::setSubpixelRendering(bool useSubpixelRendering)
{
    useSkiaSubpixelRendering = useSubpixelRendering;
}

void FontPlatformData::setSubpixelPositioning(bool useSubpixelPositioning)
{
    useSkiaSubpixelPositioning = useSubpixelPositioning;
}

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : m_typeface(hashTableDeletedFontValue())
    , m_textSize(0)
    , m_emSizeInFontUnits(0)
    , m_fakeBold(false)
    , m_fakeItalic(false)
    , m_orientation(Horizontal)
{
}

FontPlatformData::FontPlatformData()
    : m_typeface(0)
    , m_textSize(0)
    , m_emSizeInFontUnits(0)
    , m_fakeBold(false)
    , m_fakeItalic(false)
    , m_orientation(Horizontal)
{
}

FontPlatformData::FontPlatformData(float textSize, bool fakeBold, bool fakeItalic)
    : m_typeface(0)
    , m_textSize(textSize)
    , m_emSizeInFontUnits(0)
    , m_fakeBold(fakeBold)
    , m_fakeItalic(fakeItalic)
    , m_orientation(Horizontal)
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& src)
    : m_typeface(src.m_typeface)
    , m_family(src.m_family)
    , m_textSize(src.m_textSize)
    , m_emSizeInFontUnits(src.m_emSizeInFontUnits)
    , m_fakeBold(src.m_fakeBold)
    , m_fakeItalic(src.m_fakeItalic)
    , m_orientation(src.m_orientation)
    , m_style(src.m_style)
    , m_harfBuzzFace(src.m_harfBuzzFace)
{
    SkSafeRef(m_typeface);
}

FontPlatformData::FontPlatformData(SkTypeface* tf, const char* family, float textSize, bool fakeBold, bool fakeItalic, FontOrientation orientation)
    : m_typeface(tf)
    , m_family(family)
    , m_textSize(textSize)
    , m_emSizeInFontUnits(0)
    , m_fakeBold(fakeBold)
    , m_fakeItalic(fakeItalic)
    , m_orientation(orientation)
{
    SkSafeRef(m_typeface);
    querySystemForRenderStyle();
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, float textSize)
    : m_typeface(src.m_typeface)
    , m_family(src.m_family)
    , m_textSize(textSize)
    , m_emSizeInFontUnits(src.m_emSizeInFontUnits)
    , m_fakeBold(src.m_fakeBold)
    , m_fakeItalic(src.m_fakeItalic)
    , m_orientation(src.m_orientation)
    , m_harfBuzzFace(src.m_harfBuzzFace)
{
    SkSafeRef(m_typeface);
    querySystemForRenderStyle();
}

FontPlatformData::~FontPlatformData()
{
    SkSafeUnref(m_typeface);
}

int FontPlatformData::emSizeInFontUnits() const
{
    if (m_emSizeInFontUnits)
        return m_emSizeInFontUnits;

    m_emSizeInFontUnits = m_typeface->getUnitsPerEm();
    return m_emSizeInFontUnits;
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& src)
{
    SkRefCnt_SafeAssign(m_typeface, src.m_typeface);

    m_family = src.m_family;
    m_textSize = src.m_textSize;
    m_fakeBold = src.m_fakeBold;
    m_fakeItalic = src.m_fakeItalic;
    m_harfBuzzFace = src.m_harfBuzzFace;
    m_orientation = src.m_orientation;
    m_style = src.m_style;
    m_emSizeInFontUnits = src.m_emSizeInFontUnits;

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
    paint->setAntiAlias(m_style.useAntiAlias);
    paint->setHinting(static_cast<SkPaint::Hinting>(m_style.hintStyle));
    paint->setEmbeddedBitmapText(m_style.useBitmaps);
    paint->setAutohinted(m_style.useAutoHint);
    paint->setSubpixelText(m_style.useSubpixelPositioning);
    if (m_style.useAntiAlias)
        paint->setLCDRenderText(m_style.useSubpixelRendering);

    const float ts = m_textSize >= 0 ? m_textSize : 12;
    paint->setTextSize(SkFloatToScalar(ts));
    paint->setTypeface(m_typeface);
    paint->setFakeBoldText(m_fakeBold);
    paint->setTextSkewX(m_fakeItalic ? -SK_Scalar1 / 4 : 0);
}

SkFontID FontPlatformData::uniqueID() const
{
    return m_typeface->uniqueID();
}

bool FontPlatformData::operator==(const FontPlatformData& a) const
{
    // If either of the typeface pointers are invalid (either 0 or the
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
        && m_orientation == a.m_orientation
        && m_style == a.m_style;
}

unsigned FontPlatformData::hash() const
{
    unsigned h = SkTypeface::UniqueID(m_typeface);
    h ^= 0x01010101 * ((static_cast<int>(m_orientation) << 2) | (static_cast<int>(m_fakeBold) << 1) | static_cast<int>(m_fakeItalic));

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

HarfBuzzFace* FontPlatformData::harfBuzzFace() const
{
    if (!m_harfBuzzFace)
        m_harfBuzzFace = HarfBuzzFace::create(const_cast<FontPlatformData*>(this), uniqueID());

    return m_harfBuzzFace.get();
}

void FontPlatformData::getRenderStyleForStrike(const char* font, int sizeAndStyle)
{
    WebKit::WebFontRenderStyle style;

#if OS(ANDROID)
    style.setDefaults();
#else
    if (!font || !*font)
        style.setDefaults(); // It's probably a webfont. Take the system defaults.
    else if (WebKit::Platform::current()->sandboxSupport())
        WebKit::Platform::current()->sandboxSupport()->getRenderStyleForStrike(font, sizeAndStyle, &style);
    else
        WebKit::WebFontInfo::renderStyleForStrike(font, sizeAndStyle, &style);
#endif

    style.toFontRenderStyle(&m_style);
}

void FontPlatformData::querySystemForRenderStyle()
{
    getRenderStyleForStrike(m_family.data(), (((int)m_textSize) << 2) | (m_typeface->style() & 3));

    // Fix FontRenderStyle::NoPreference to actual styles.
    if (m_style.useAntiAlias == FontRenderStyle::NoPreference)
         m_style.useAntiAlias = useSkiaAntiAlias;

    if (!m_style.useHinting)
        m_style.hintStyle = SkPaint::kNo_Hinting;
    else if (m_style.useHinting == FontRenderStyle::NoPreference)
        m_style.hintStyle = skiaHinting;

    if (m_style.useBitmaps == FontRenderStyle::NoPreference)
        m_style.useBitmaps = useSkiaBitmaps;
    if (m_style.useAutoHint == FontRenderStyle::NoPreference)
        m_style.useAutoHint = useSkiaAutoHint;
    if (m_style.useSubpixelPositioning == FontRenderStyle::NoPreference)
        m_style.useSubpixelPositioning = useSkiaSubpixelPositioning;
    if (m_style.useAntiAlias == FontRenderStyle::NoPreference)
        m_style.useAntiAlias = useSkiaAntiAlias;
    if (m_style.useSubpixelRendering == FontRenderStyle::NoPreference)
        m_style.useSubpixelRendering = useSkiaSubpixelRendering;
}

#if ENABLE(OPENTYPE_VERTICAL)
static SkFontTableTag reverseByteOrder(uint32_t tableTag)
{
    return (tableTag >> 24) | ((tableTag >> 8) & 0xff00) | ((tableTag & 0xff00) << 8) | ((tableTag & 0xff) << 24);
}

PassRefPtr<OpenTypeVerticalData> FontPlatformData::verticalData() const
{
    return fontCache()->getVerticalData(uniqueID(), *this);
}

PassRefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    RefPtr<SharedBuffer> buffer;

    SkFontTableTag tag = reverseByteOrder(table);
    const size_t tableSize = m_typeface->getTableSize(tag);
    if (tableSize) {
        Vector<char> tableBuffer(tableSize);
        m_typeface->getTableData(tag, 0, tableSize, &tableBuffer[0]);
        buffer = SharedBuffer::adoptVector(tableBuffer);
    }
    return buffer.release();
}
#endif

} // namespace WebCore
