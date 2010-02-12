/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayoutTestControllerQt_h
#define LayoutTestControllerQt_h

#include <QBasicTimer>
#include <QObject>
#include <QSize>
#include <QString>
#include <QtDebug>
#include <QTimer>
#include <QTimerEvent>
#include <QVariant>

#include <qwebdatabase.h>
#include <qwebframe.h>
#include <qwebhistory.h>
#include <qwebpage.h>
#include <qwebsecurityorigin.h>

class QWebFrame;
namespace WebCore {
    class DumpRenderTree;
}
class LayoutTestController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int webHistoryItemCount READ webHistoryItemCount)
    Q_PROPERTY(int workerThreadCount READ workerThreadCount)
    Q_PROPERTY(bool globalFlag READ globalFlag WRITE setGlobalFlag)
public:
    LayoutTestController(WebCore::DumpRenderTree* drt);

    bool shouldDumpAsText() const { return m_textDump; }
    bool shouldDumpBackForwardList() const { return m_dumpBackForwardList; }
    bool shouldDumpChildrenAsText() const { return m_dumpChildrenAsText; }
    bool shouldDumpDatabaseCallbacks() const { return m_dumpDatabaseCallbacks; }
    bool shouldDumpStatusCallbacks() const { return m_dumpStatusCallbacks; }
    bool shouldWaitUntilDone() const { return m_waitForDone; }
    bool shouldHandleErrorPages() const { return m_handleErrorPages; }
    bool canOpenWindows() const { return m_canOpenWindows; }
    bool shouldDumpTitleChanges() const { return m_dumpTitleChanges; }
    bool waitForPolicy() const { return m_waitForPolicy; }

    void reset();

protected:
    void timerEvent(QTimerEvent*);

signals:
    void done();

    void showPage();
    void hidePage();

public slots:
    void maybeDump(bool ok);
    void dumpAsText() { m_textDump = true; }
    void dumpChildFramesAsText() { m_dumpChildrenAsText = true; }
    void dumpDatabaseCallbacks() { m_dumpDatabaseCallbacks = true; }
    void dumpStatusCallbacks() { m_dumpStatusCallbacks = true; }
    void setCanOpenWindows() { m_canOpenWindows = true; }
    void waitUntilDone();
    QString counterValueForElementById(const QString& id);
    int webHistoryItemCount();
    void keepWebHistory();
    void notifyDone();
    void dumpBackForwardList() { m_dumpBackForwardList = true; }
    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool flag) { m_globalFlag = flag; }
    void handleErrorPages() { m_handleErrorPages = true; }
    void dumpEditingCallbacks();
    void dumpFrameLoadCallbacks();
    void dumpResourceLoadCallbacks();
    void queueBackNavigation(int howFarBackward);
    void queueForwardNavigation(int howFarForward);
    void queueLoad(const QString& url, const QString& target = QString());
    void queueReload();
    void queueLoadingScript(const QString& script);
    void queueNonLoadingScript(const QString& script);
    void provisionalLoad();
    void setCloseRemainingWindowsWhenComplete(bool = false) {}
    int windowCount();
    void display();
    void clearBackForwardList();
    QString pathToLocalResource(const QString& url);
    void dumpTitleChanges() { m_dumpTitleChanges = true; }
    QString encodeHostName(const QString& host);
    QString decodeHostName(const QString& host);
    void dumpSelectionRect() const {}
    void showWebInspector();
    void hideWebInspector();

    void setFrameSetFlatteningEnabled(bool enable);
    void setAllowUniversalAccessFromFileURLs(bool enable);
    void setJavaScriptProfilingEnabled(bool enable);
    void setFixedContentsSize(int width, int height);
    void setPrivateBrowsingEnabled(bool enable);
    void setPopupBlockingEnabled(bool enable);
    void setPOSIXLocale(const QString& locale);
    void resetLoadFinished() { m_loadFinished = false; }
    void setWindowIsKey(bool isKey);
    void setMainFrameIsFirstResponder(bool isFirst);
    void setXSSAuditorEnabled(bool enable);

    bool pauseAnimationAtTimeOnElementWithId(const QString& animationName, double time, const QString& elementId);
    bool pauseTransitionAtTimeOnElementWithId(const QString& propertyName, double time, const QString& elementId);
    bool sampleSVGAnimationForElementAtTime(const QString& animationId, double time, const QString& elementId);

    unsigned numberOfActiveAnimations() const;

    void whiteListAccessFromOrigin(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);

    void dispatchPendingLoadRequests();
    void disableImageLoading();

    void setDatabaseQuota(int size);
    void clearAllDatabases();

    void waitForPolicyDelegate();
    void overridePreference(const QString& name, const QVariant& value);
    void setUserStyleSheetLocation(const QString& url);
    void setUserStyleSheetEnabled(bool enabled);
    void setDomainRelaxationForbiddenForURLScheme(bool forbidden, const QString& scheme);
    int workerThreadCount();
    int pageNumberForElementById(const QString& id, float width = 0, float height = 0);

private slots:
    void processWork();

private:
    bool m_hasDumped;
    bool m_textDump;
    bool m_dumpBackForwardList;
    bool m_dumpChildrenAsText;
    bool m_canOpenWindows;
    bool m_waitForDone;
    bool m_dumpTitleChanges;
    bool m_dumpDatabaseCallbacks;
    bool m_dumpStatusCallbacks;
    bool m_waitForPolicy;
    bool m_handleErrorPages;
    bool m_loadFinished;
    bool m_globalFlag;

    QUrl m_userStyleSheetLocation;
    QBasicTimer m_timeoutTimer;
    QWebFrame* m_topLoadingFrame;
    WebCore::DumpRenderTree* m_drt;
    QWebHistory* m_webHistory;
};

#endif // LayoutTestControllerQt_h
