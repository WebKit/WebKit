/*
 *  Copyright (C) 2016 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DragData.h"

#include "SelectionData.h"

namespace WebCore {

bool DragData::canSmartReplace() const
{
    return false;
}

bool DragData::containsColor() const
{
    return false;
}

static bool allowsFilenameAccess(const DragData& drag)
{
    // A drag that started in this application is web content.
    return !drag.flags().contains(DragApplicationFlags::IsSource);
}

bool DragData::containsFiles() const
{
    return !m_disallowFileAccess && allowsFilenameAccess(*this) && m_platformDragData->hasFilenames();
}

unsigned DragData::numberOfFiles() const
{
    if (m_disallowFileAccess || !allowsFilenameAccess(*this))
        return 0;
    return m_platformDragData->filenames().size();
}

Vector<String> DragData::asFilenames() const
{
    if (m_disallowFileAccess || !allowsFilenameAccess(*this))
        return { };
    return m_platformDragData->filenames();
}

bool DragData::containsPlainText() const
{
    return m_platformDragData->hasText();
}

String DragData::asPlainText() const
{
    return m_platformDragData->text();
}

Color DragData::asColor() const
{
    return Color();
}

bool DragData::containsCompatibleContent(DraggingPurpose) const
{
    return containsPlainText() || containsURL() || m_platformDragData->hasMarkup() || containsColor() || containsFiles();
}

bool DragData::containsURL(FilenameConversionPolicy filenamePolicy) const
{
    return !asURL(filenamePolicy).isEmpty();
}

String DragData::asURL(FilenameConversionPolicy filenamePolicy, String* title) const
{
    if (!m_platformDragData->hasURL())
        return String();
    if (filenamePolicy != ConvertFilenames) {
        if (m_platformDragData->url().protocolIsFile())
            return { };
    }

    if (title)
        *title = m_platformDragData->urlLabel();
    return m_platformDragData->url().string();
}

bool DragData::shouldMatchStyleOnDrop() const
{
    return false;
}

}
