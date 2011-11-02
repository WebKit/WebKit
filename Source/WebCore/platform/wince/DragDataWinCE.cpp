/*
 *  Copyright (C) 2007-2008 Torch Mobile, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "DragData.h"

#include "DocumentFragment.h"
#include "PlatformString.h"
#include "Range.h"

namespace WebCore {

bool DragData::containsURL(Frame*, FilenameConversionPolicy filenamePolicy) const
{
    return false;
}

const DragDataMap& DragData::dragDataMap()
{
    return m_dragDataMap;
}

String DragData::asURL(Frame*, FilenameConversionPolicy filenamePolicy, String* title) const
{
    return String();
}

bool DragData::containsFiles() const
{
    return false;
}

unsigned DragData::numberOfFiles() const
{
    return 0;
}

void DragData::asFilenames(Vector<String>&) const
{
}

bool DragData::containsPlainText() const
{
    return false;
}

String DragData::asPlainText(Frame*) const
{
    return String();
}

bool DragData::containsColor() const
{
    return false;
}

bool DragData::canSmartReplace() const
{
    return false;
}

bool DragData::containsCompatibleContent() const
{
    return false;
}

PassRefPtr<DocumentFragment> DragData::asFragment(Frame* frame, PassRefPtr<Range>, bool, bool&) const
{
     return 0;
}

Color DragData::asColor() const
{
    return Color();
}

} // namespace WebCore
