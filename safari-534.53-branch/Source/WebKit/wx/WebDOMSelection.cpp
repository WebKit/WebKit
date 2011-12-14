/*
 * Copyright (C) 2008 Kevin Ollivier <kevino@theolliviers.com>
 *
 * All rights reserved.
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
 
#include "config.h"
#include "WebDOMSelection.h"

#include "Element.h"
#include "FrameSelection.h"
#include "WebDOMElement.h"
#include "WebDOMRange.h"

#include <wtf/RefPtr.h>

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

IMPLEMENT_DYNAMIC_CLASS(wxWebKitSelection, wxObject)

wxWebKitSelection::wxWebKitSelection(const wxWebKitSelection& other)
{
    m_selection = other.m_selection;
}

WebDOMElement* wxWebKitSelection::GetRootEditableElement() const
{
    if (m_selection)
        return new WebDOMElement(m_selection->rootEditableElement());
        
    return 0;
}

WebDOMRange* wxWebKitSelection::GetAsRange()
{
    if (m_selection) {
        WTF::RefPtr<WebCore::Range> range = m_selection->toNormalizedRange();
        // keep it alive until it reaches wxWebKitDOMRange, which takes ownership
        
        if (range) {
            range->ref();
            return new WebDOMRange(range.get());
        }
    }
        
    return 0;
}
