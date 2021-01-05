/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2014 Haiku, Inc. All righrs reserved.
 *
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

#ifndef ProgressTrackerClientHaiku_h
#define ProgressTrackerClientHaiku_h

#include "ProgressTrackerClient.h"

#include <Messenger.h>

class BWebPage;

namespace WebCore {

class ProgressTrackerClientHaiku final : public WebCore::ProgressTrackerClient {
public:
    explicit ProgressTrackerClientHaiku(BWebPage*);

    void setDispatchTarget(const BMessenger& messenger) { m_messenger = messenger; }
    
private:
    // ProgressTrackerClient API
    void progressStarted(WebCore::Frame& originatingProgressFrame) override;
    void progressEstimateChanged(WebCore::Frame& originatingProgressFrame) override;
    void progressFinished(WebCore::Frame& originatingProgressFrame) override;

    void triggerNavigationHistoryUpdate(WebCore::Frame&) const;
    status_t dispatchMessage(BMessage& message) const;

    BWebPage* m_view;
    BMessenger m_messenger;
};

} // namespace WebCore

#endif // ProgressTrackerClientHaiku_h
