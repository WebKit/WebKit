/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
#include "Pasteboard.h"

#include "CommonAtomStrings.h"
#include "DragData.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "SelectionData.h"

namespace WebCore {

std::unique_ptr<Pasteboard> Pasteboard::createForGlobalSelection(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTFMove(context), "PRIMARY"_s);
}


bool Pasteboard::canSmartReplace()
{
    if (m_selectionData)
        return m_selectionData->canSmartReplace();
    return platformStrategies()->pasteboardStrategy()->types(m_name).contains("application/vnd.webkitgtk.smartpaste"_s);
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy, std::optional<size_t>)
{
    text.text = platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name, "text/plain"_s);
}

} // namespace WebCore
