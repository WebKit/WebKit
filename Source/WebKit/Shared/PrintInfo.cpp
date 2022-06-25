/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "PrintInfo.h"

#include "WebCoreArgumentCoders.h"

#if PLATFORM(GTK)
#include "ArgumentCodersGtk.h"
#endif

namespace WebKit {

void PrintInfo::encode(IPC::Encoder& encoder) const
{
    encoder << pageSetupScaleFactor;
    encoder << availablePaperWidth;
    encoder << availablePaperHeight;
    encoder << margin;

#if PLATFORM(GTK)
    IPC::encode(encoder, printSettings.get());
    IPC::encode(encoder, pageSetup.get());
    encoder << printMode;
#endif

#if PLATFORM(IOS_FAMILY)
    encoder << snapshotFirstPage;
#endif
}

bool PrintInfo::decode(IPC::Decoder& decoder, PrintInfo& info)
{
    if (!decoder.decode(info.pageSetupScaleFactor))
        return false;
    if (!decoder.decode(info.availablePaperWidth))
        return false;
    if (!decoder.decode(info.availablePaperHeight))
        return false;
    if (!decoder.decode(info.margin))
        return false;

#if PLATFORM(GTK)
    if (!IPC::decode(decoder, info.printSettings))
        return false;
    if (!IPC::decode(decoder, info.pageSetup))
        return false;
    if (!decoder.decode(info.printMode))
        return false;
#endif

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(info.snapshotFirstPage))
        return false;
#endif

    return true;
}

}
