/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "MediaTimeQTKit.h"

#if PLATFORM(MAC)

#import "SoftLinking.h"
#import <QTKit/QTTime.h>

SOFT_LINK_FRAMEWORK(QTKit);
SOFT_LINK_CONSTANT(QTKit, QTIndefiniteTime, QTTime);
SOFT_LINK_CONSTANT(QTKit, QTZeroTime, QTTime);
SOFT_LINK(QTKit, QTTimeCompare, NSComparisonResult, (QTTime time, QTTime otherTime), (time, otherTime));
SOFT_LINK(QTKit, QTMakeTime, QTTime, (long long timeValue, long timeScale), (timeValue, timeScale));

namespace WebCore {

MediaTime toMediaTime(const QTTime& qtTime)
{
    if (qtTime.flags & kQTTimeIsIndefinite)
        return MediaTime::indefiniteTime();
    return MediaTime(qtTime.timeValue, qtTime.timeScale);
}

QTTime toQTTime(const MediaTime& mediaTime)
{
    if (mediaTime.isIndefinite() || mediaTime.isInvalid())
        return getQTIndefiniteTime();
    if (!mediaTime)
        return getQTZeroTime();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return QTMakeTime(mediaTime.timeValue(), mediaTime.timeScale());
#pragma clang diagnostic pop
}

}

#endif
