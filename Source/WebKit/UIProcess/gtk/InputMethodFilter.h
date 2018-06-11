/*
 * Copyright (C) 2012, 2014 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef InputMethodFilter_h
#define InputMethodFilter_h

#include <WebCore/IntPoint.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _GdkEventKey GdkEventKey;
typedef struct _GtkIMContext GtkIMContext;

namespace WebCore {
struct CompositionResults;
class IntRect;
}

namespace WebKit {

class WebPageProxy;

class InputMethodFilter {
    WTF_MAKE_NONCOPYABLE(InputMethodFilter);
public:
    enum EventFakedForComposition {
        EventFaked,
        EventNotFaked
    };

    InputMethodFilter();
    ~InputMethodFilter();

    GtkIMContext* context() const { return m_context.get(); }

    void setPage(WebPageProxy* page) { m_page = page; }

    void setEnabled(bool);
    void setCursorRect(const WebCore::IntRect&);

    using FilterKeyEventCompletionHandler = Function<void(const WebCore::CompositionResults&, InputMethodFilter::EventFakedForComposition)>;
    void filterKeyEvent(GdkEventKey*, FilterKeyEventCompletionHandler&& = nullptr);
    void notifyFocusedIn();
    void notifyFocusedOut();
    void notifyMouseButtonPress();

#if ENABLE(API_TESTS)
    void setTestingMode(bool enable) { m_testingMode = enable; }
    const Vector<String>& events() const { return m_events; }
#endif

private:
    enum ResultsToSend {
        Preedit = 1 << 1,
        Composition = 1 << 2,
        PreeditAndComposition = Preedit | Composition
    };

    static void handleCommitCallback(InputMethodFilter*, const char* compositionString);
    static void handlePreeditStartCallback(InputMethodFilter*);
    static void handlePreeditChangedCallback(InputMethodFilter*);
    static void handlePreeditEndCallback(InputMethodFilter*);

    void handleCommit(const char* compositionString);
    void handlePreeditChanged();
    void handlePreeditStart();
    void handlePreeditEnd();

    void handleKeyboardEvent(GdkEventKey*, const String& eventString = String(), EventFakedForComposition = EventNotFaked);
    void handleKeyboardEventWithCompositionResults(GdkEventKey*, ResultsToSend = PreeditAndComposition, EventFakedForComposition = EventNotFaked);

    void sendCompositionAndPreeditWithFakeKeyEvents(ResultsToSend);
    void confirmComposition();
    void updatePreedit();
    void confirmCurrentComposition();
    void cancelContextComposition();

    bool isViewFocused() const;

#if ENABLE(API_TESTS)
    void logHandleKeyboardEventForTesting(GdkEventKey*, const String&, EventFakedForComposition);
    void logHandleKeyboardEventWithCompositionResultsForTesting(GdkEventKey*, ResultsToSend, EventFakedForComposition);
    void logConfirmCompositionForTesting();
    void logSetPreeditForTesting();
#endif

    GRefPtr<GtkIMContext> m_context;
    WebPageProxy* m_page;
    unsigned m_enabled : 1;
    unsigned m_composingTextCurrently : 1;
    unsigned m_filteringKeyEvent : 1;
    unsigned m_preeditChanged : 1;
    unsigned m_preventNextCommit : 1;
    unsigned m_justSentFakeKeyUp : 1;
    int m_cursorOffset;
    unsigned m_lastFilteredKeyPressCodeWithNoResults;
    WebCore::IntPoint m_lastCareLocation;
    String m_confirmedComposition;
    String m_preedit;

    FilterKeyEventCompletionHandler m_filterKeyEventCompletionHandler;

#if ENABLE(API_TESTS)
    bool m_testingMode;
    Vector<String> m_events;
#endif
};

} // namespace WebKit

#endif // InputMethodFilter_h
