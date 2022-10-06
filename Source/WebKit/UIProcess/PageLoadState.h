/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#include <WebCore/CertificateInfo.h>
#include <wtf/URL.h>
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

        virtual void willChangeNegotiatedLegacyTLS() { };
        virtual void didChangeNegotiatedLegacyTLS() { };

        virtual void willChangeEstimatedProgress() = 0;
        virtual void didChangeEstimatedProgress() = 0;

        virtual void willChangeCanGoBack() = 0;
        virtual void didChangeCanGoBack() = 0;

        virtual void willChangeCanGoForward() = 0;
        virtual void didChangeCanGoForward() = 0;

        virtual void willChangeNetworkRequestsInProgress() = 0;
        virtual void didChangeNetworkRequestsInProgress() = 0;

        virtual void willChangeCertificateInfo() = 0;
        virtual void didChangeCertificateInfo() = 0;

        virtual void willChangeWebProcessIsResponsive() = 0;
        virtual void didChangeWebProcessIsResponsive() = 0;
        
        virtual void didSwapWebProcesses() = 0;
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
#if ASSERT_ENABLED
                : m_pageLoadState(*transaction.m_pageLoadState)
#endif
            {
                transaction.m_pageLoadState->m_mayHaveUncommittedChanges = true;
            }

#if ASSERT_ENABLED
            PageLoadState& m_pageLoadState;
#endif
        };

        RefPtr<WebPageProxy> m_webPageProxy;
        PageLoadState* m_pageLoadState;
    };

    struct PendingAPIRequest {
        uint64_t navigationID { 0 };
        String url;
    };

    void addObserver(Observer&);
    void removeObserver(Observer&);

    Transaction transaction() { return Transaction(*this); }
    void commitChanges();

    void reset(const Transaction::Token&);

    bool isLoading() const;
    bool isProvisional() const { return m_committedState.state == State::Provisional; }
    bool isCommitted() const { return m_committedState.state == State::Committed; }
    bool isFinished() const { return m_committedState.state == State::Finished; }

    bool hasUncommittedLoad() const;

    const String& provisionalURL() const { return m_committedState.provisionalURL; }
    const String& url() const { return m_committedState.url; }
    const String& unreachableURL() const { return m_committedState.unreachableURL; }

    String activeURL() const;

    bool hasOnlySecureContent() const;
    bool hasNegotiatedLegacyTLS() const;
    void negotiatedLegacyTLS(const Transaction::Token&);

    double estimatedProgress() const;
    bool networkRequestsInProgress() const { return m_committedState.networkRequestsInProgress; }

    const WebCore::CertificateInfo& certificateInfo() const { return m_committedState.certificateInfo; }

    const URL& resourceDirectoryURL() const;

    const String& pendingAPIRequestURL() const;
    const PendingAPIRequest& pendingAPIRequest() const;
    void setPendingAPIRequest(const Transaction::Token&, PendingAPIRequest&& pendingAPIRequest, const URL& resourceDirectoryPath = { });
    void clearPendingAPIRequest(const Transaction::Token&);

    void didStartProvisionalLoad(const Transaction::Token&, const String& url, const String& unreachableURL);
    void didExplicitOpen(const Transaction::Token&, const String& url);
    void didReceiveServerRedirectForProvisionalLoad(const Transaction::Token&, const String& url);
    void didFailProvisionalLoad(const Transaction::Token&);

    void didCommitLoad(const Transaction::Token&, const WebCore::CertificateInfo&, bool hasInsecureContent, bool usedLegacyTLS);
    void didFinishLoad(const Transaction::Token&);
    void didFailLoad(const Transaction::Token&);

    void didSameDocumentNavigation(const Transaction::Token&, const String& url);

    void didDisplayOrRunInsecureContent(const Transaction::Token&);

    void setUnreachableURL(const Transaction::Token&, const String&);

    const String& title() const;
    void setTitle(const Transaction::Token&, const String&);
    void setTitleFromSafeBrowsingWarning(const Transaction::Token&, const String&);

    bool canGoBack() const;
    void setCanGoBack(const Transaction::Token&, bool);

    bool canGoForward() const;
    void setCanGoForward(const Transaction::Token&, bool);

    void didStartProgress(const Transaction::Token&);
    void didChangeProgress(const Transaction::Token&, double);
    void didFinishProgress(const Transaction::Token&);
    void setNetworkRequestsInProgress(const Transaction::Token&, bool);

    void didSwapWebProcesses();

    bool committedHasInsecureContent() const { return m_committedState.hasInsecureContent; }

    // FIXME: We piggy-back off PageLoadState::Observer so that both WKWebView and WKObservablePageState
    // can listen for changes. Once we get rid of WKObservablePageState these could just be part of API::NavigationClient.
    void willChangeProcessIsResponsive();
    void didChangeProcessIsResponsive();

private:
    void beginTransaction() { ++m_outstandingTransactionCount; }
    void endTransaction();

    void callObserverCallback(void (Observer::*)());

    Vector<Observer*> m_observers;

    struct Data {
        State state { State::Finished };
        bool hasInsecureContent { false };
        bool negotiatedLegacyTLS { false };

        PendingAPIRequest pendingAPIRequest;

        String provisionalURL;
        String url;

        String unreachableURL;

        String title;
        String titleFromSafeBrowsingWarning;

        URL resourceDirectoryURL;

        bool canGoBack { false };
        bool canGoForward { false };

        double estimatedProgress { 0 };
        bool networkRequestsInProgress { false };

        WebCore::CertificateInfo certificateInfo;
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
