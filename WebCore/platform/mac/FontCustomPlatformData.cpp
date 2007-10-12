/*
 * Copyright (C) 2007 Apple Computer, Inc.
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "FontCustomPlatformData.h"

#include <ApplicationServices/ApplicationServices.h>
#include "SharedBuffer.h"
#include "FontPlatformData.h"

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
    ATSFontDeactivate(m_atsContainer, NULL, kATSOptionFlagsDefault);
    CGFontRelease(m_cgFont);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic)
{
    return FontPlatformData(m_cgFont, (ATSUFontID)m_atsFont, size, bold, italic);
}

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer* buffer)
{
    // Use ATS to activate the font.
    ATSFontContainerRef containerRef = 0;

    // The value "3" means that the font is private and can't be seen by anyone else.
    ATSFontActivateFromMemory((void*)buffer->data(), buffer->size(), 3, kATSFontFormatUnspecified, NULL, kATSOptionFlagsDefault, &containerRef);
    if (!containerRef)
        return 0;
    ItemCount fontCount;
    ATSFontFindFromContainer(containerRef, kATSOptionFlagsDefault, 0, NULL, &fontCount);
    
    // We just support the first font in the list.
    if (fontCount == 0) {
        ATSFontDeactivate(containerRef, NULL, kATSOptionFlagsDefault);
        return 0;
    }
    
    ATSFontRef fontRef = 0;
    ATSFontFindFromContainer(containerRef, kATSOptionFlagsDefault, 1, &fontRef, NULL);
    if (!fontRef) {
        ATSFontDeactivate(containerRef, NULL, kATSOptionFlagsDefault);
        return 0;
    }
    
    CGFontRef cgFontRef = CGFontCreateWithPlatformFont(&fontRef);
    if (!cgFontRef) {
        ATSFontDeactivate(containerRef, NULL, kATSOptionFlagsDefault);
        return 0;
    }

    return new FontCustomPlatformData(containerRef, fontRef, cgFontRef);
}

}
