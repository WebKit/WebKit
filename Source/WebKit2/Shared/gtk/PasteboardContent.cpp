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
#include "PasteboardContent.h"

#include "ArgumentCodersGtk.h"
#include "DataObjectGtk.h"
#include "Decoder.h"
#include "Encoder.h"
#include <wtf/RetainPtr.h>

namespace WebKit {

PasteboardContent::PasteboardContent()
    : dataObject(WebCore::DataObjectGtk::create())
{
}

PasteboardContent::PasteboardContent(const WebCore::DataObjectGtk& data)
    : dataObject(const_cast<WebCore::DataObjectGtk&>(data))
{
}

PasteboardContent::PasteboardContent(Ref<WebCore::DataObjectGtk>&& data)
    : dataObject(WTFMove(data))
{
}

void PasteboardContent::encode(IPC::Encoder& encoder) const
{
    encoder << dataObject.get();
}

bool PasteboardContent::decode(IPC::Decoder& decoder, PasteboardContent& pasteboardContent)
{
    return decoder.decode(pasteboardContent.dataObject.get());
}

} // namespace WebKit
