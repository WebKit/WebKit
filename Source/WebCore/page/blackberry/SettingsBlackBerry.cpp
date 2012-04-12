/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Settings.h"

namespace WebCore {

void Settings::initializeDefaultFontFamilies()
{
    setCursiveFontFamily("Comic Sans MS");
    setFantasyFontFamily("Impact");
    setFixedFontFamily("Courier New");
    setSansSerifFontFamily("Arial");
    setSerifFontFamily("Times New Roman");
    setStandardFontFamily("Times New Roman");

    setStandardFontFamily("Adobe Ming Std L", USCRIPT_TRADITIONAL_HAN);

    setStandardFontFamily("Adobe Heiti Std R", USCRIPT_SIMPLIFIED_HAN);

    setFixedFontFamily("Ryo Gothic PlusN R", USCRIPT_KATAKANA_OR_HIRAGANA);
    setSansSerifFontFamily("Ryo Gothic PlusN R", USCRIPT_KATAKANA_OR_HIRAGANA);
    setSerifFontFamily("Ryo Text PlusN L", USCRIPT_KATAKANA_OR_HIRAGANA);
    setStandardFontFamily("Ryo Gothic PlusN R", USCRIPT_KATAKANA_OR_HIRAGANA);

    setStandardFontFamily("Adobe Gothic Std", USCRIPT_HANGUL);

    setStandardFontFamily("Garuda", USCRIPT_THAI);

    setStandardFontFamily("Tahoma", USCRIPT_ARABIC);

    setStandardFontFamily("Tahoma", USCRIPT_HEBREW);

    setStandardFontFamily("Bengali OTS", USCRIPT_BENGALI);
    setStandardFontFamily("Devanagari OTS", USCRIPT_DEVANAGARI);
    setStandardFontFamily("Gujarati OTS", USCRIPT_GUJARATI);
    setStandardFontFamily("Gurmukhi OTS", USCRIPT_GURMUKHI);
    setStandardFontFamily("Kannada OTS", USCRIPT_KANNADA);
    setStandardFontFamily("Malayalam OTS", USCRIPT_MALAYALAM);
    setStandardFontFamily("Sinhala OTS", USCRIPT_SINHALA);
    setStandardFontFamily("Tamil OTS", USCRIPT_TAMIL);
    setStandardFontFamily("Telugu OTS", USCRIPT_TELUGU);
}


} // namespace WebCore
