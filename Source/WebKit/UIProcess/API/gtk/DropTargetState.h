/*
 * Copyright (C) 2026 Hayden Barnes
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

#pragma once

#include <utility>

namespace WebKit {

// GtkDropTargetAsync hands over the GdkDrop when ::drop returns TRUE and the source
// then waits for one gdk_drop_finish(). The mime data is read asynchronously, so a
// drop can arrive before it is complete and has to be deferred, and the finish is
// owed on every teardown path. Free of GTK types so it can be unit tested.
class DropTargetState {
public:
    void didAccept(bool waitingForData)
    {
        m_waitingForData = waitingForData;
        m_deferredDrop = false;
        m_unfinishedDrop = false;
    }

    void didFinishLoadingData() { m_waitingForData = false; }

    // Returns true when the drop has to wait for the data.
    bool didRequestDrop()
    {
        m_unfinishedDrop = true;
        if (m_waitingForData)
            m_deferredDrop = true;
        return m_deferredDrop;
    }

    bool takeDeferredDrop()
    {
        return std::exchange(m_deferredDrop, false);
    }

    void didFinishDrop()
    {
        m_deferredDrop = false;
        m_unfinishedDrop = false;
    }

    // True once for a drop that was taken over but never finished.
    bool takeUnfinishedDrop()
    {
        m_waitingForData = false;
        m_deferredDrop = false;
        return std::exchange(m_unfinishedDrop, false);
    }

    bool isWaitingForData() const { return m_waitingForData; }

private:
    bool m_waitingForData { false };
    bool m_deferredDrop { false };
    bool m_unfinishedDrop { false };
};

} // namespace WebKit
