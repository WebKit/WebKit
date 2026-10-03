/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(COCOA)

#include "AXCoreObject.h"
#include "Color.h"
#include "Timer.h"
#include <wtf/CheckedRef.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakListHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AXObjectCache;
class AccessibilityObject;
class Element;

bool shouldLogFormActivity();
void logFormActivity(const String&);

#define AXFORMLOG(...) do { \
    if (shouldLogFormActivity()) [[unlikely]] \
        logFormActivity(makeString(__VA_ARGS__)); \
} while (0)

// Watches for text that appears after a form is submitted, and potentially applies repairs if the
// markup is insufficient. Specifically, this is synthesizing an aria-errormessage relationship based
// on heuristics, and announcing errors that the author didn't announce.
class AXFormActivityMonitor final : public CanMakeCheckedPtr<AXFormActivityMonitor> {
    WTF_MAKE_NONCOPYABLE(AXFormActivityMonitor);
    WTF_MAKE_TZONE_ALLOCATED(AXFormActivityMonitor);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AXFormActivityMonitor);
public:
    explicit AXFormActivityMonitor(AXObjectCache&);
    ~AXFormActivityMonitor();

    // ----
    // Inputs. Hooks that tell this class something happened.

    // Submitting a form, pressing its submit button, or pressing Enter in one of its fields is unmistakably an attempt
    // to submit. Anything else, like a click on a button that belongs to no form, only may have been one.
    enum class Attempt : bool { Submission, PossibleSubmission };

    // The user tried to submit the fields in the container, and nothing is going to navigate as a result (e.g. because
    // validation failed). This container is either a <form>, or an element that encapsulates form components (fields
    // and a submit button of some kind).
    void didAttemptSubmissionWithoutNavigation(Element& container, Element* submitter, Attempt = Attempt::Submission);

    // A navigation started after all, or the document went away.
    void didStartLoading(LocalFrame*);
    void cancel();

    // Content appeared or changed, whether inserted, unhidden, or written into.
    void onChangedContent(Element&);
    void onChangedContent(AccessibilityObject&);

    // Text that was announced, useful so we don't re-announce it as part of this class's repairs.
    void onAnnouncedText(const String&);

    // Examines what onChangedContent() recorded and tries to discover error messages that need repair.
    void collectErrorMessagesFromChangedElements();

    // ----
    // Outputs. What the cache and the platform layer can ask about the state of this watch.

    bool isWatching() const { return !!m_container; }

    // Reporting builds accessibility objects, which the cache will only do with layout clean, so the settle
    // timer asks for it rather than doing it. AXObjectCache::performDeferredCacheUpdate() is where layout is
    // guaranteed, and calls report() from there.
    bool reportIsPending() const { return m_reportIsPending; }
    void report();

    WEBCORE_EXPORT static void setSettleDelayForTesting(std::optional<Seconds>);

private:
    // Text the site added, and the element that holds it, so it can be paired with the field it is
    // most likely describing once the page has settled.
    struct CandidateErrorMessage {
        String text;
        WeakPtr<Element, WeakPtrImplWithEventTargetData> element;
        std::optional<Color> textColor;
    };

    Seconds settleDelay() const;
    Seconds checkInterval() const;
    Seconds quietPeriod() const;
    void startWatchWindow();
    void captureRenderedFields(Element& container);
    void requestReport();
    void settleTimerFired();
    void collectErrorMessagesFrom(AccessibilityObject&, unsigned depth);

    CheckedRef<AXObjectCache> m_cache;
    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_container;
    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_submitter;
    WeakListHashSet<Element, WeakPtrImplWithEventTargetData> m_changedElements;
    Vector<CandidateErrorMessage> m_candidateErrorMessages;
    HashSet<String> m_announcedText;
    // The fields rendered when the user acted, so a field the page reveals afterwards is not taken for one they left wrong.
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_fieldsRenderedAtAttempt;
    unsigned m_changedElementCount { 0 };
    unsigned m_objectsVisited { 0 };
    Attempt m_attempt { Attempt::Submission };
    bool m_reportIsPending { false };
    MonotonicTime m_watchStartTime;
    MonotonicTime m_lastChangeTime;
    MonotonicTime m_watchDeadline;
    Timer m_settleTimer;
};

} // namespace WebCore

#endif // PLATFORM(COCOA)
