/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
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

#import "KWQSlot.h"

#import <kwqdebug.h>
#import <khtml_part.h>
#import <render_form.h>

using khtml::RenderTextArea;

enum FunctionNumber {
    slotRedirect,
    slotTextChanged
};

KWQSlot::KWQSlot(QObject *object, const char *member) : m_object(0)
{
    if (KWQNamesMatch(member, SLOT(slotRedirect()))) {
        KWQ_ASSERT(dynamic_cast<KHTMLPart *>(object));
        m_function = slotRedirect;
    } else if (KWQNamesMatch(member, SLOT(slotTextChanged()))) {
        KWQ_ASSERT(dynamic_cast<RenderTextArea *>(object));
        m_function = slotTextChanged;
    } else {
        // ERROR
        return;
    }
    m_object = object;
}
    
void KWQSlot::call() const
{
    if (!m_object) {
        return;
    }
    
    switch (m_function) {
        case slotRedirect: {
            KHTMLPart *part = dynamic_cast<KHTMLPart *>(m_object.pointer());
            if (part) {
                part->slotRedirect();
            }
            return;
        }
        case slotTextChanged: {
            RenderTextArea *area = dynamic_cast<RenderTextArea *>(m_object.pointer());
            if (area) {
                area->slotTextChanged();
            }
            return;
        }
    }
    // ERROR
}

bool operator==(const KWQSlot &a, const KWQSlot &b)
{
    return a.m_object == b.m_object && (a.m_object == 0 || a.m_function == b.m_function);
}

bool KWQNamesMatch(const char *a, const char *b)
{
    char ca = *a;
    char cb = *b;
    
    for (;;) {
        while (ca == ' ') {
            ca = *++a;
        }
        while (cb == ' ') {
            cb = *++b;
        }
        
        if (ca != cb) {
            return false;
        }
        if (ca == 0) {
            return true;
        }
        
        ca = *++a;
        cb = *++b;
    }
}
