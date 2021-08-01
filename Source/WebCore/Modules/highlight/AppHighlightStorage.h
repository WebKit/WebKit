/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AppHighlight.h"
#include "AppHighlightRangeData.h"
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

#if ENABLE(APP_HIGHLIGHTS)

class Document;
class SharedBuffer;
class StaticRange;
class Highlight;

enum class RestoreWithTextSearch : bool { No, Yes };

enum class ScrollToHighlight : bool { No, Yes };

class AppHighlightStorage final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AppHighlightStorage(Document&);
    ~AppHighlightStorage();

    WEBCORE_EXPORT void storeAppHighlight(Ref<StaticRange>&&);
    WEBCORE_EXPORT void restoreAndScrollToAppHighlight(Ref<SharedBuffer>&&, ScrollToHighlight);
    void restoreUnrestoredAppHighlights();
    MonotonicTime lastRangeSearchTime() const { return m_timeAtLastRangeSearch; }
    void resetLastRangeSearchTime() { m_timeAtLastRangeSearch = MonotonicTime::now(); }
    bool hasUnrestoredHighlights() const { return m_unrestoredHighlights.size() || m_unrestoredScrollHighlight; }

private:
    bool attemptToRestoreHighlightAndScroll(AppHighlightRangeData&, ScrollToHighlight);
    
    WeakPtr<Document> m_document;
    MonotonicTime m_timeAtLastRangeSearch;
    Vector<AppHighlightRangeData> m_unrestoredHighlights;
    std::optional<AppHighlightRangeData> m_unrestoredScrollHighlight;
};

#endif

} // namespace WebCore
