/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "FrameView.h"

#import "Document.h"
#import "BlockExceptions.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "RenderObject.h"

namespace WebCore {

void FrameView::updateBorder()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [Mac(m_frame.get())->bridge() setHasBorder:hasBorder()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FrameView::updateDashboardRegions()
{
    Document* doc = m_frame->document();
    if (doc->hasDashboardRegions()) {
        DeprecatedValueList<DashboardRegionValue> newRegions = doc->renderer()->computeDashboardRegions();
        DeprecatedValueList<DashboardRegionValue> currentRegions = doc->dashboardRegions();
        doc->setDashboardRegions(newRegions);
        Mac(m_frame.get())->dashboardRegionsChanged();
    }
}

}
