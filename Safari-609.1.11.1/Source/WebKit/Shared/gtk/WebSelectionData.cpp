/*
 *  Copyright (C) 2016 Red Hat Inc.
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
#include "WebSelectionData.h"

#include "ArgumentCodersGtk.h"
#include "Decoder.h"
#include "Encoder.h"
#include <wtf/RetainPtr.h>

namespace WebKit {

WebSelectionData::WebSelectionData()
    : selectionData(WebCore::SelectionData::create())
{
}

WebSelectionData::WebSelectionData(const WebCore::SelectionData& data)
    : selectionData(const_cast<WebCore::SelectionData&>(data))
{
}

WebSelectionData::WebSelectionData(Ref<WebCore::SelectionData>&& data)
    : selectionData(WTFMove(data))
{
}

void WebSelectionData::encode(IPC::Encoder& encoder) const
{
    encoder << selectionData.get();
}

bool WebSelectionData::decode(IPC::Decoder& decoder, WebSelectionData& selection)
{
    return decoder.decode(selection.selectionData.get());
}

} // namespace WebKit
