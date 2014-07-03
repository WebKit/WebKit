/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef PageLoadState_h
#define PageLoadState_h

#include <wtf/text/WTFString.h>

namespace WebKit {

class WebPageProxy;

class PageLoadState {
public:
    explicit PageLoadState(WebPageProxy&);
    ~PageLoadState();

    enum class State {
        Provisional,
        Committed,
        Finished
    };

    class Observer {
    public:
        virtual ~Observer() { }

        virtual void willChangeIsLoading() = 0;
        virtual void didChangeIsLoading() = 0;

        virtual void willChangeTitle() = 0;
        virtual void didChangeTitle() = 0;

        virtual void willChangeActiveURL() = 0;
        virtual void didChangeActiveURL() = 0;

        virtual void willChangeHasOnlySecureContent() = 0;
        virtual void didChangeHasOnlySecureContent() = 0;

        virtual void willChangeEstimatedProgress() = 0;
        virtual void didChangeEstimatedProgress() = 0;

        virtual void willChangeCanGoBack() = 0;
        virtual void didChangeCanGoBack() = 0;

        virtual void willChangeCanGoForward() = 0;
        virtual void didChangeCanGoForward() = 0;

        virtual void willChangeNetworkRequestsInProgress() = 0;
        virtual void didChangeNetworkRequestsInProgress() = 0;
    };

    class Transaction {
        WTF_MAKE_NONCOPYABLE(Transaction);
    public:
        Transaction(Transaction&&);
        ~Transaction();

    private:
        friend class PageLoadState;

        explicit Transaction(PageLoadState&);

        class Token {
        public:
            Token(Transaction& transaction)
#if !ASSERT_DISABLED
                : m_pageLoadState(*transaction.m_pageLoadState)
#endif
            {
                transaction.m_pageLoadState->m_mayHaveUncommittedChanges = true;
            }

#if !ASSERT_DISABLED
            PageLoadState& m_pageLoadState;
#endif
        };

        RefPtr<WebPageProxy> m_webPageProxy;
        PageLoadState* m_pageLoadState;
    };

    void addObserver(Observer&);
    void removeObserver(Observer&);

    Transaction transaction() { return Transaction(*this); }
    void commitChanges();

    void reset(const Transaction::Token&);

    bool isLoading() const;

    const String& provisionalURL() const { return m_committedState.provisionalURL; }
    const String& url() const { return m_committedState.url; }
    const String& unreachableURL() const { return m_committedState.unreachableURL; }

    String activeURL() const;

    bool hasOnlySecureContent() const;

    double estimatedProgress() const;
    bool networkRequestsInProgress() const { return m_committedState.networkRequestsInProgress; }

    const String& pendingAPIRequestURL() const;
    void setPendingAPIRequestURL(const Transaction::Token&, const String&);
    void clearPendingAPIRequestURL(const Transaction::Token&);

    void didStartProvisionalLoad(const Transaction::Token&, const String& url, const String& unreachableURL);
    void didReceiveServerRedirectForProvisionalLoad(const Transaction::Token&, const String& url);
    void didFailProvisionalLoad(const Transaction::Token&);

    void didCommitLoad(const Transaction::Token&);
    void didFinishLoad(const Transaction::Token&);
    void didFailLoad(const Transaction::Token&);

    void didSameDocumentNavigation(const Transaction::Token&, const String& url);

    void didDisplayOrRunInsecureContent(const Transaction::Token&);

    void setUnreachableURL(const Transaction::Token&, const String&);

    const String& title() const;
    void setTitle(const Transaction::Token&, const String&);

    bool canGoBack() const;
    void setCanGoBack(const Transaction::Token&, bool);

    bool canGoForward() const;
    void setCanGoForward(const Transaction::Token&, bool);

    void didStartProgress(const Transaction::Token&);
    void didChangeProgress(const Transaction::Token&, double);
    void didFinishProgress(const Transaction::Token&);
    void setNetworkRequestsInProgress(const Transaction::Token&, bool);

private:
    void beginTransaction() { ++m_outstandingTransactionCount; }
    void endTransaction();

    void callObserverCallback(void (Observer::*)());

    Vector<Observer*> m_observers;

    struct Data {
        Data()
            : state(State::Finished)
            , hasInsecureContent(false)
            , canGoBack(false)
            , canGoForward(false)
            , estimatedProgress(0)
            , networkRequestsInProgress(false)
        {
        }

        State state;
        bool hasInsecureContent;

        String pendingAPIRequestURL;

        String provisionalURL;
        String url;

        String unreachableURL;

        String title;

        bool canGoBack;
        bool canGoForward;

        double estimatedProgress;
        bool networkRequestsInProgress;
    };

    static bool isLoading(const Data&);
    static String activeURL(const Data&);
    static bool hasOnlySecureContent(const Data&);
    static double estimatedProgress(const Data&);

    WebPageProxy& m_webPageProxy;

    Data m_committedState;
    Data m_uncommittedState;

    String m_lastUnreachableURL;

    bool m_mayHaveUncommittedChanges;
    unsigned m_outstandingTransactionCount;
};

} // namespace WebKit

#endif // PageLoadState_h
