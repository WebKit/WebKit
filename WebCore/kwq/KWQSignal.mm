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

#import "KWQSignal.h"

#import "qobject.h"

KWQSignal::KWQSignal(QObject *object, const char *name)
    : m_object(object), m_next(object->m_signalListHead), m_name(name)
{
    object->m_signalListHead = this;
}

KWQSignal::~KWQSignal()
{
    KWQSignal **prev = &m_object->m_signalListHead;
    KWQSignal *signal;
    while ((signal = *prev)) {
        if (signal == this) {
            *prev = m_next;
            break;
        }
        prev = &signal->m_next;
    }
}

void KWQSignal::connect(const KWQSlot &slot)
{
    if (!m_slot.isEmpty()) {
        // ERROR
        return;
    }
    m_slot = slot;
}

void KWQSignal::disconnect(const KWQSlot &slot)
{
    if (m_slot != slot) {
        // ERROR
        return;
    }
    m_slot.clear();
}

void KWQSignal::call() const
{
    if (!m_object->m_signalsBlocked) {
        KWQObjectSenderScope senderScope(m_object);
        m_slot.call();
    }
}

void KWQSignal::call(int i) const
{
    if (!m_object->m_signalsBlocked) {
        KWQObjectSenderScope senderScope(m_object);
        m_slot.call(i);
    }
}

void KWQSignal::call(const QString &s) const
{
    if (!m_object->m_signalsBlocked) {
        KWQObjectSenderScope senderScope(m_object);
        m_slot.call(s);
    }
}
