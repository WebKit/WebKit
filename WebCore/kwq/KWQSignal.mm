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
#import "KWQAssertions.h"

using KIO::Job;

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
    const int numSlots = sizeof(m_slots) / sizeof(m_slots[0]);
    for (int i = 0; i != numSlots; ++i) {
        if (m_slots[i].isEmpty()) {
            m_slots[i] = slot;
            return;
        }
    }
    ERROR("more than %d connections to the same signal not supported, %s", numSlots, m_name);
}

void KWQSignal::disconnect(const KWQSlot &slot)
{
    const int numSlots = sizeof(m_slots) / sizeof(m_slots[0]);
    for (int i = 0; i != numSlots; ++i) {
        if (m_slots[i] == slot) {
            m_slots[i].clear();
            return;
        }
    }
    ERROR("disconnecting a signal that wasn't connected, %s", m_name);
}

void KWQSignal::call() const
{
    if (!m_object->m_signalsBlocked) {
        KWQObjectSenderScope senderScope(m_object);
        const int numSlots = sizeof(m_slots) / sizeof(m_slots[0]);
        KWQSlot copiedSlots[numSlots];
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i] = m_slots[i];
        }
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i].call();
        }
    }
}

void KWQSignal::call(bool b) const
{
    if (!m_object->m_signalsBlocked) {
        KWQObjectSenderScope senderScope(m_object);
        const int numSlots = sizeof(m_slots) / sizeof(m_slots[0]);
        KWQSlot copiedSlots[numSlots];
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i] = m_slots[i];
        }
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i].call(b);
        }
    }
}

void KWQSignal::call(int j) const
{
    if (!m_object->m_signalsBlocked) {
        KWQObjectSenderScope senderScope(m_object);
        const int numSlots = sizeof(m_slots) / sizeof(m_slots[0]);
        KWQSlot copiedSlots[numSlots];
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i] = m_slots[i];
        }
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i].call(j);
        }
    }
}

void KWQSignal::call(const QString &s) const
{
    if (!m_object->m_signalsBlocked) {
        KWQObjectSenderScope senderScope(m_object);
        const int numSlots = sizeof(m_slots) / sizeof(m_slots[0]);
        KWQSlot copiedSlots[numSlots];
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i] = m_slots[i];
        }
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i].call(s);
        }
    }
}

void KWQSignal::call(Job *j) const
{
    if (!m_object->m_signalsBlocked) {
        KWQObjectSenderScope senderScope(m_object);
        const int numSlots = sizeof(m_slots) / sizeof(m_slots[0]);
        KWQSlot copiedSlots[numSlots];
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i] = m_slots[i];
        }
        for (int i = 0; i != numSlots; ++i) {
            copiedSlots[i].call(j);
        }
    }
}
