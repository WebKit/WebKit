/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if ENABLE(VIDEO) && PLATFORM(IOS)

#import "InbandTextTrackPrivateAVFIOS.h"

#import "BlockExceptions.h"
#import "FloatConversion.h"
#import "InbandTextTrackPrivate.h"
#import "InbandTextTrackPrivateAVF.h"
#import "Logging.h"

using namespace WebCore;

namespace WebCore {

InbandTextTrackPrivateAVFIOS::InbandTextTrackPrivateAVFIOS(AVFInbandTrackParent* parent, int internalID, const String& name, const String& language, const String& type)
    : InbandTextTrackPrivateAVF(parent)
    , m_name(name)
    , m_language(language)
    , m_type(type)
    , m_internalID(internalID)
{
}

InbandTextTrackPrivateAVFIOS::~InbandTextTrackPrivateAVFIOS()
{
}

InbandTextTrackPrivate::Kind InbandTextTrackPrivateAVFIOS::kind() const
{
    if ([m_type isEqualToString:@"captions"])
        return Captions;
    if ([m_type isEqualToString:@"subtitles"])
        return Subtitles;

    return Captions;
}

} // namespace WebCore

#endif
