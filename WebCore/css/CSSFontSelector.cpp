/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CSSFontSelector.h"

#include "AtomicString.h"
#include "CSSFontFace.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFaceSource.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "DocLoader.h"
#include "Document.h"
#include "FontCache.h"
#include "FontFamilyValue.h"
#include "Frame.h"
#include "RenderObject.h"
#include "Settings.h"

namespace WebCore {

CSSFontSelector::CSSFontSelector(Document* document)
: m_document(document)
{
    ASSERT(m_document);
}

CSSFontSelector::~CSSFontSelector()
{}

bool CSSFontSelector::isEmpty() const
{
    return m_fonts.isEmpty();
}

DocLoader* CSSFontSelector::docLoader() const
{
    return m_document->docLoader();
}

static String hashForFont(const String& familyName, bool bold, bool italic)
{
    String familyHash(familyName);
    if (bold)
        familyHash += "-webkit-bold";
    if (italic)
        familyHash += "-webkit-italic";
    return AtomicString(familyHash);
}

void CSSFontSelector::addFontFaceRule(const CSSFontFaceRule* fontFaceRule)
{
    // Obtain the font-family property and the src property.  Both must be defined.
    const CSSMutableStyleDeclaration* style = fontFaceRule->style();
    RefPtr<CSSValue> fontFamily = style->getPropertyCSSValue(CSS_PROP_FONT_FAMILY);
    RefPtr<CSSValue> src = style->getPropertyCSSValue(CSS_PROP_SRC);    
    if (!fontFamily || !src || !fontFamily->isValueList() || !src->isValueList())
        return;

    CSSValueList* familyList = static_cast<CSSValueList*>(fontFamily.get());
    if (!familyList->length())
        return;
        
    CSSValueList* srcList = static_cast<CSSValueList*>(src.get());
    if (!srcList->length())
        return;

    // Create a FontDescription for this font and set up bold/italic info properly.
    FontDescription fontDescription;
    RefPtr<CSSValue> fontWeight = style->getPropertyCSSValue(CSS_PROP_FONT_WEIGHT);
    RefPtr<CSSValue> fontStyle = style->getPropertyCSSValue(CSS_PROP_FONT_STYLE);
    fontDescription.setItalic(fontStyle.get() && static_cast<CSSPrimitiveValue*>(fontStyle.get())->getIdent() != CSS_VAL_NORMAL);
    if (fontWeight) {
        // FIXME: Need to support weights for real, since we're effectively limiting the number of supported weights to two.
        // This behavior could also result in the "last kinda bold variant" described winning even if it isn't the best match for bold.
        switch (static_cast<CSSPrimitiveValue*>(fontWeight.get())->getIdent()) {
            case CSS_VAL_BOLD:
            case CSS_VAL_BOLDER:
            case CSS_VAL_600:
            case CSS_VAL_700:
            case CSS_VAL_800:
            case CSS_VAL_900:
                fontDescription.setWeight(cBoldWeight);
            default:
                break;
        }
    }
    
    // Each item in the src property's list is a single CSSFontFaceSource. Put them all into a CSSFontFace.
    CSSFontFace* fontFace = new CSSFontFace(this);
    
    int i;
    int srcLength = srcList->length();
    bool foundLocal = false;
    for (i = 0; i < srcLength; i++) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        CSSFontFaceSrcValue* item = static_cast<CSSFontFaceSrcValue*>(srcList->item(i));
        CSSFontFaceSource* source = 0;
        if (!item->isLocal()) {
            if (item->isSupportedFormat()) {
                CachedFont* cachedFont = m_document->docLoader()->requestFont(item->resource());
                if (cachedFont)
                    source = new CSSFontFaceSource(item->resource(), cachedFont);
            }
        } else {
            // Test the validity of the local font now.  We don't want to include this font if it does not exist
            // on the system.  If it *does* exist on the system, then we don't need to look any further.
            String family = item->resource();
            if (FontCache::fontExists(fontDescription, family)) {
                source = new CSSFontFaceSource(family);
                foundLocal = true;
            }
        }

        if (source)
            fontFace->addSource(source);
        
        // We can just break if we see a local font that is valid.
        if (foundLocal)
            break;
    }
    
    if (!fontFace->isValid()) {
        delete fontFace;
        return;
    }

    // Hash under every single family name.
    int familyLength = familyList->length();
    for (i = 0; i < familyLength; i++) {
        CSSPrimitiveValue* item = static_cast<CSSPrimitiveValue*>(familyList->item(i));
        String familyName;
        if (item->primitiveType() == CSSPrimitiveValue::CSS_STRING)
            familyName = static_cast<FontFamilyValue*>(item)->fontName();
        else if (item->primitiveType() == CSSPrimitiveValue::CSS_IDENT) {
            // We need to use the raw text for all the generic family types, since @font-face is a way of actually
            // defining what font to use for those types.
            String familyName;
            switch (item->getIdent()) {
                case CSS_VAL_SERIF:
                    familyName = "-webkit-serif";
                    break;
                case CSS_VAL_SANS_SERIF:
                    familyName = "-webkit-sans-serif";
                    break;
                case CSS_VAL_CURSIVE:
                    familyName = "-webkit-cursive";
                    break;
                case CSS_VAL_FANTASY:
                    familyName = "-webkit-fantasy";
                    break;
                case CSS_VAL_MONOSPACE:
                    familyName = "-webkit-monospace";
                    break;
                default:
                    break;
            }
        }
        
        if (!familyName.isEmpty())
            m_fonts.set(hashForFont(familyName.lower(), fontDescription.bold(), fontDescription.italic()), fontFace);
    }
}

void CSSFontSelector::fontLoaded(CSSFontFace*)
{
    if (m_document->inPageCache())
        return;
    m_document->recalcStyle(Document::Force);
    m_document->renderer()->setNeedsLayoutAndPrefWidthsRecalc();
}

FontData* CSSFontSelector::getFontData(const FontDescription& fontDescription, const AtomicString& familyName)
{
    if (m_fonts.isEmpty())
        return 0;
    
    bool bold = fontDescription.bold();
    bool italic = fontDescription.italic();
    
    bool syntheticBold = false;
    bool syntheticItalic = false;

    String family = familyName.domString().lower();
    
    RefPtr<CSSFontFace> face = m_fonts.get(hashForFont(family, bold, italic));
    // If we don't find a face, and if bold/italic are set, we should try other variants.
    // Bold/italic should try bold first, then italic, then normal (on the assumption that we are better at synthesizing italic than we are
    // at synthesizing bold).
    if (!face) {
        if (bold && italic) {
            syntheticItalic = true;
            face = m_fonts.get(hashForFont(family, bold, false));
            if (!face) {
                syntheticBold = true;
                face = m_fonts.get(hashForFont(family, false, italic));
            }
        }
        
        // Bold should try normal.
        // Italic should try normal.
        if (!face && (bold || italic)) {
            syntheticBold = bold;
            syntheticItalic = italic;
            face = m_fonts.get(hashForFont(family, false, false));
        }
    }
    
    // If no face was found, then return 0 and let the OS come up with its best match for the name.
    if (!face) {
        // If we were handed a generic family, but there was no match, go ahead and return the correct font based off our
        // settings.
        const Settings* settings = m_document->frame()->settings();
        AtomicString genericFamily;
        if (familyName == "-webkit-serif")
            genericFamily = settings->serifFontFamily();
        else if (familyName == "-webkit-sans-serif")
            genericFamily = settings->sansSerifFontFamily();
        else if (familyName == "-webkit-cursive")
            genericFamily = settings->cursiveFontFamily();
        else if (familyName == "-webkit-fantasy")
            genericFamily = settings->cursiveFontFamily();
        else if (familyName == "-webkit-monospace")
            genericFamily = settings->fixedFontFamily();
        
        if (!genericFamily.isEmpty())
            return FontCache::getCachedFontData(FontCache::getCachedFontPlatformData(fontDescription, genericFamily));
        return 0;
    }

    // We have a face.  Ask it for a font data.  If it cannot produce one, it will fail, and the OS will take over.
    return face->getFontData(fontDescription, syntheticBold, syntheticItalic);
}

}
