/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
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
#include "DragData.h"

#include "Color.h"
#include "DocumentFragment.h"
#include "NotImplemented.h"
#include "PlatformString.h"

#include <wtf/Vector.h>

namespace WebCore {

bool DragData::canSmartReplace() const
{
    notImplemented();
    return false;
}

bool DragData::containsColor() const
{
    notImplemented();
    return false;
}

bool DragData::containsCompatibleContent() const
{
    notImplemented();
    return false;
}

bool DragData::containsFiles() const
{
    notImplemented();
    return false;
}

bool DragData::containsPlainText() const
{
    notImplemented();
    return false;
}

bool DragData::containsURL(Frame*, FilenameConversionPolicy filenamePolicy) const
{
    notImplemented();
    return false;
}

void DragData::asFilenames(WTF::Vector<String, 0u>& result) const
{
    // FIXME: remove explicit 0 size in result template once this is implemented
    notImplemented();
}

Color DragData::asColor() const
{
    notImplemented();
    return Color();
}

String DragData::asPlainText(Frame*) const
{
    notImplemented();
    return String();
}

String DragData::asURL(Frame*, FilenameConversionPolicy filenamePolicy, String* title) const
{
    notImplemented();
    return String();
}

WTF::PassRefPtr<DocumentFragment> DragData::asFragment(Frame*, PassRefPtr<Range> context,
                                            bool allowPlainText, bool& chosePlainText) const
{
    notImplemented();
    return 0;
}

} // namespace WebCore
