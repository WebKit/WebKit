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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "FontCustomPlatformData.h"

#include <wtf/RetainPtr.h>
#include <ApplicationServices/ApplicationServices.h>
#include "SharedBuffer.h"
#include "FontPlatformData.h"

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
    CGFontRelease(m_cgFont);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic)
{
    return FontPlatformData(m_cgFont, size, bold, italic);
}

const void* getData(void* info)
{
    SharedBuffer* buffer = static_cast<SharedBuffer*>(info);
    buffer->ref();
    return (void*)buffer->data();
}

void releaseData(void* info, const void* data)
{
    static_cast<SharedBuffer*>(info)->deref();
}

size_t getBytesWithOffset(void *info, void* buffer, size_t offset, size_t count)
{
    SharedBuffer* sharedBuffer = static_cast<SharedBuffer*>(info);
    size_t availBytes = count;
    if (offset + count > sharedBuffer->size())
        availBytes -= (offset + count) - sharedBuffer->size();
    memcpy(buffer, sharedBuffer->data() + offset, availBytes);
    return availBytes;
}

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer* buffer)
{
    ASSERT_ARG(buffer, buffer);

    // Get CG to create the font.
    CGDataProviderDirectAccessCallbacks callbacks = { &getData, &releaseData, &getBytesWithOffset, NULL };
    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateDirectAccess(buffer, buffer->size(), &callbacks));
    CGFontRef cgFont = CGFontCreateWithDataProvider(dataProvider.get());
    if (!cgFont)
        return 0;
    return new FontCustomPlatformData(cgFont);
}

}
