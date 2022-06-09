/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "BorderData.h"

#include "OutlineValue.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, const BorderValue& borderValue)
{
    ts << borderValue.width() << " " << borderValue.style() << " " << borderValue.color();
    return ts;
}

TextStream& operator<<(TextStream& ts, const OutlineValue& outlineValue)
{
    ts << static_cast<const BorderValue&>(outlineValue);
    ts.dumpProperty("outline-offset", outlineValue.offset());
    return ts;
}

void BorderData::dump(TextStream& ts, DumpStyleValues behavior) const
{
    if (behavior == DumpStyleValues::All || left() != BorderValue())
        ts.dumpProperty("left", left());
    if (behavior == DumpStyleValues::All || right() != BorderValue())
        ts.dumpProperty("right", right());
    if (behavior == DumpStyleValues::All || top() != BorderValue())
        ts.dumpProperty("top", top());
    if (behavior == DumpStyleValues::All || bottom() != BorderValue())
        ts.dumpProperty("bottom", bottom());

    ts.dumpProperty("image", image());

    if (behavior == DumpStyleValues::All || !topLeftRadius().isZero())
        ts.dumpProperty("top-left", topLeftRadius());
    if (behavior == DumpStyleValues::All || !topRightRadius().isZero())
        ts.dumpProperty("top-right", topRightRadius());
    if (behavior == DumpStyleValues::All || !bottomLeftRadius().isZero())
        ts.dumpProperty("bottom-left", bottomLeftRadius());
    if (behavior == DumpStyleValues::All || !bottomRightRadius().isZero())
        ts.dumpProperty("bottom-right", bottomRightRadius());
}

TextStream& operator<<(TextStream& ts, const BorderData& borderData)
{
    borderData.dump(ts);
    return ts;
}

} // namespace WebCore
