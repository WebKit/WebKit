/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef KWQSTYLE_H_
#define KWQSTYLE_H_

#include "KWQObject.h"
#include "IntSize.h"

class QWidget;

class QStyle : public QObject {
public:
    enum PixelMetric { 
        PM_IndicatorWidth,
        PM_IndicatorHeight,
        PM_ExclusiveIndicatorWidth,
        PM_ExclusiveIndicatorHeight,
        PM_DefaultFrameWidth,
        PM_ButtonMargin
    };
    
    enum ContentType {
        CT_PushButton
    };

    int pixelMetric(PixelMetric metric, QWidget * = 0) const {
        switch (metric) {
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
        case PM_ExclusiveIndicatorWidth:
        case PM_ExclusiveIndicatorHeight:
            return 22; // FIXME! Shouldn't be hardcoded. Perhaps shouldn't all be same.
        case PM_DefaultFrameWidth:
        case PM_ButtonMargin:
            return 0;
        }
        return 0;
    }
    
    IntSize sizeFromContents(ContentType, QWidget *, const IntSize &) const;
};

#endif
