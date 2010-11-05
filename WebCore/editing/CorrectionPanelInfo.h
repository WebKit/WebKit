/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef CorrectionPanelInfo_h
#define CorrectionPanelInfo_h

#if PLATFORM(MAC) && !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
// Some platforms provide UI for suggesting autocorrection.
#define SUPPORT_AUTOCORRECTION_PANEL 1
// Some platforms use spelling and autocorrection markers to provide visual cue.
// On such platform, if word with marker is edited, we need to remove the marker.
#define REMOVE_MARKERS_UPON_EDITING 1
#else
#define SUPPORT_AUTOCORRECTION_PANEL 0
#define REMOVE_MARKERS_UPON_EDITING 0
#endif // #if PLATFORM(MAC) && !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)

#include "Range.h"

namespace WebCore {

struct CorrectionPanelInfo {
    enum PanelType {
        PanelTypeCorrection = 0,
        PanelTypeReversion
    };

    RefPtr<Range> m_rangeToBeReplaced;
    String m_replacedString;
    String m_replacementString;
    PanelType m_panelType;
    bool m_isActive;
};

enum CorrectionWasRejectedOrNot { CorrectionWasNotRejected, CorrectionWasRejected };

} // namespace WebCore

#endif // CorrectionPanelInfo_h
