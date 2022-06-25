/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "NetworkStateNotifier.h"

#include <wtf/MainThread.h>
#include <wtf/Vector.h>

#include <winsock2.h>
#include <iphlpapi.h>

namespace WebCore {

void NetworkStateNotifier::updateStateWithoutNotifying()
{
    DWORD size = 0;
    if (::GetAdaptersAddresses(AF_UNSPEC, 0, 0, 0, &size) != ERROR_BUFFER_OVERFLOW)
        return;

    Vector<char> buffer(size);
    auto addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (::GetAdaptersAddresses(AF_UNSPEC, 0, 0, addresses, &size) != ERROR_SUCCESS)
        return;

    for (; addresses; addresses = addresses->Next) {
        if (addresses->IfType != MIB_IF_TYPE_LOOPBACK && addresses->OperStatus == IfOperStatusUp) {
            // We found an interface that was up.
            m_isOnLine = true;
            return;
        }
    }

    m_isOnLine = false;
}

void CALLBACK NetworkStateNotifier::addressChangeCallback(void*, BOOLEAN timedOut)
{
    callOnMainThread([] {
        // NotifyAddrChange only notifies us of a single address change. Now that we've been notified,
        // we need to call it again so we'll get notified the *next* time.
        singleton().registerForAddressChange();

        singleton().updateStateSoon();
    });
}

void NetworkStateNotifier::registerForAddressChange()
{
    HANDLE handle;
    ::NotifyAddrChange(&handle, &m_overlapped);
}

void NetworkStateNotifier::startObserving()
{
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_overlapped.hEvent = ::CreateEvent(0, false, false, 0);
    ::RegisterWaitForSingleObject(&m_waitHandle, m_overlapped.hEvent, addressChangeCallback, nullptr, INFINITE, 0);
    registerForAddressChange();
}

} // namespace WebCore
