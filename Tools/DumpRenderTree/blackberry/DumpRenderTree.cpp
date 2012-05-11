/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DumpRenderTree.h"

#include "AccessibilityController.h"
#include "BackForwardController.h"
#include "BackForwardListImpl.h"
#include "CString.h"
#include "DatabaseTracker.h"
#include "DocumentLoader.h"
#include "DumpRenderTree/GCController.h"
#include "DumpRenderTreeSupport.h"
#include "EditingBehaviorTypes.h"
#include "EditorClientBlackBerry.h"
#include "EditorInsertAction.h"
#include "Element.h"
#include "EventSender.h"
#include "Frame.h"
#include "FrameLoaderTypes.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "IntSize.h"
#include "LayoutTestController.h"
#include "NotImplemented.h"
#include "OwnArrayPtr.h"
#include "Page.h"
#include "PageGroup.h"
#include "PixelDumpSupport.h"
#include "PixelDumpSupportBlackBerry.h"
#include "Range.h"
#include "RenderTreeAsText.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "TextAffinity.h"
#include "Timer.h"
#include "Vector.h"
#include "WebCoreTestSupport.h"
#include "WebPage.h"
#include "WebPageClient.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <WebSettings.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wtf/NonCopyingSort.h>

#define SDCARD_PATH "/developer"

volatile bool testDone;

RefPtr<LayoutTestController> gLayoutTestController;

WebCore::Frame* mainFrame = 0;
WebCore::Frame* topLoadingFrame = 0;
bool waitForPolicy = false;

// FIXME: Assuming LayoutTests has been copied to /developer/LayoutTests/
static const char* const kSDCLayoutTestsURI = "file:///developer/LayoutTests/";
static const char* httpTestSyntax = "http/tests/";
static const char* localTestSyntax = "local/";
static const char* httpPrefixURL = "http://127.0.0.1:8000/";

using namespace std;

static String drtAffinityDescription(WebCore::EAffinity affinity)
{
    if (affinity == WebCore::UPSTREAM)
        return String("NSSelectionAffinityUpstream");
    if (affinity == WebCore::DOWNSTREAM)
        return String("NSSelectionAffinityDownstream");
    return "";
}

static String drtDumpPath(WebCore::Node* node)
{
    WebCore::Node* parent = node->parentNode();
    String str = String::format("%s", node->nodeName().utf8().data());
    if (parent) {
        str.append(" > ");
        str.append(drtDumpPath(parent));
    }
    return str;
}

static String drtRangeDescription(WebCore::Range* range)
{
    if (!range)
        return "(null)";
    return String::format("range from %d of %s to %d of %s", range->startOffset(), drtDumpPath(range->startContainer()).utf8().data(), range->endOffset(), drtDumpPath(range->endContainer()).utf8().data());
}

static String drtFrameDescription(WebCore::Frame* frame)
{
    String name = frame->tree()->uniqueName().string();
    if (frame == mainFrame) {
        if (!name.isNull() && name.length())
            return String::format("main frame \"%s\"", name.utf8().data());
        return "main frame";
    }
    if (!name.isNull())
        return String::format("frame \"%s\"", name.utf8().data());
    return "frame (anonymous)";
}

static bool shouldLogFrameLoadDelegates(const String& url)
{
    return url.contains("loading/");
}

namespace BlackBerry {
namespace WebKit {

DumpRenderTree* DumpRenderTree::s_currentInstance = 0;
bool DumpRenderTree::s_selectTrailingWhitespaceEnabled = false;

static void createFile(const String& fileName)
{
    FILE* fd = fopen(fileName.utf8().data(), "wb");
    fclose(fd);
}

DumpRenderTree::DumpRenderTree(BlackBerry::WebKit::WebPage* page)
    : m_gcController(0)
    , m_accessibilityController(0)
    , m_page(page)
    , m_dumpPixels(false)
    , m_waitToDumpWatchdogTimer(this, &DumpRenderTree::waitToDumpWatchdogTimerFired)
    , m_workTimer(this, &DumpRenderTree::processWork)
    , m_acceptsEditing(true)
    , m_runningRefTests(false)
{
    String sdcardPath = SDCARD_PATH;
    m_resultsDir = sdcardPath + "/results/";
    m_indexFile = sdcardPath + "/index.drt";
    m_doneFile = sdcardPath + "/done";
    m_currentTestFile = sdcardPath + "/current.drt";
    m_page->resetVirtualViewportOnCommitted(false);
    m_page->setVirtualViewportSize(800, 600);
    s_currentInstance = this;
}

DumpRenderTree::~DumpRenderTree()
{
    delete m_gcController;
    delete m_accessibilityController;
}

void DumpRenderTree::runTest(const String& url)
{
    createFile(m_resultsDir + *m_currentTest + ".dump.crash");

    mainFrame->loader()->stopForUserCancel();
    resetToConsistentStateBeforeTesting();
    if (shouldLogFrameLoadDelegates(url))
        gLayoutTestController->setDumpFrameLoadCallbacks(true);
    String stdoutFile = m_resultsDir + *m_currentTest + ".dump";
    String stderrFile = m_resultsDir + *m_currentTest + ".stderr";

    // FIXME: we should preserve the original stdout and stderr here but aren't doing
    // that yet due to issues with dup, etc.
    freopen(stdoutFile.utf8().data(), "wb", stdout);
    freopen(stderrFile.utf8().data(), "wb", stderr);

    FILE* current = fopen(m_currentTestFile.utf8().data(), "w");
    fwrite(m_currentTest->utf8().data(), 1, m_currentTest->utf8().length(), current);
    fclose(current);
    m_page->load(url.utf8().data(), 0, false);
}

void DumpRenderTree::doneDrt()
{
    fclose(stdout);
    fclose(stderr);

    // Notify the external world that we're done.
    createFile(m_doneFile);
    (m_page->client())->notifyRunLayoutTestsFinished();
}

void DumpRenderTree::getRefTests(const String& testName)
{
    if (m_runningRefTests)
        return;

    const char* suffixes[] = {"-expected", "-expected-mismatch"};
    const int countofSuffixes = sizeof(suffixes) / sizeof(const char*);
    // FIXME: Currently we only have ref tests with .html extension, you many need to add more
    // when they have more extensions(.htm, .shtml, .xhtml, etc.).
    const char* extensions[] = {".html", ".svg"};
    const int countofExtensions = sizeof(extensions) / sizeof(const char*);
    String layoutDir = kSDCLayoutTestsURI + 7; // 7: strlen("file://"), layoutDir: "/developer/LayoutTests/"

    size_t iEnd = testName.reverseFind('.');

    String nameWithoutExtension = testName.substring(0, iEnd);
    for (int i = 0; i < countofSuffixes; ++i)
        for (int j = 0; j < countofExtensions; ++j) {
            String candidateFile = layoutDir + nameWithoutExtension + suffixes[i] + extensions[j];
            if (!access(candidateFile.utf8().data(), F_OK))
                m_refTests.append(nameWithoutExtension + suffixes[i] + extensions[j]);
        }
}

void DumpRenderTree::runCurrentTest()
{
    if (isHTTPTest(m_currentTest->utf8().data())) {
        m_currentHttpTest = m_currentTest->utf8().data();
        m_currentHttpTest.remove(0, strlen(httpTestSyntax));
        runTest(httpPrefixURL + m_currentHttpTest);
    } else
        runTest(kSDCLayoutTestsURI + *m_currentTest);
}

void DumpRenderTree::runRemainingTests()
{
    // FIXME: fflush should not be necessary but is temporarily required due to a bug in stdio output.
    fflush(stdout);
    fflush(stderr);

    if (m_currentTest >= m_tests.end() - 1) {
        // Run ref-tests after real tests were finished
        if (!m_runningRefTests && !m_refTests.isEmpty()) {
            m_tests.clear();
            m_tests.append(m_refTests);
            m_refTests.clear();
            m_currentTest = m_tests.begin();
            m_runningRefTests = true;
        } else {
            doneDrt();
            return;
        }
    } else
        m_currentTest++;

    runCurrentTest();
}

void DumpRenderTree::resetToConsistentStateBeforeTesting()
{
    if (isHTTPTest(m_currentTest->utf8().data()))
        gLayoutTestController = LayoutTestController::create(String(httpPrefixURL + *m_currentTest).utf8().data(), "");
    else
        gLayoutTestController = LayoutTestController::create(String(kSDCLayoutTestsURI + *m_currentTest).utf8().data(), "");

    gLayoutTestController->setIconDatabaseEnabled(false);

    DumpRenderTreeSupport::resetGeolocationMock(m_page);

    topLoadingFrame = 0;
    m_loadFinished = false;
    s_selectTrailingWhitespaceEnabled = false;

    testDone = false;
    WorkQueue::shared()->clear();
    WorkQueue::shared()->setFrozen(false);

    WebSettings* settings = m_page->settings();

    settings->setTextReflowMode(WebSettings::TextReflowDisabled);
    settings->setJavaScriptEnabled(true);
    settings->setLoadsImagesAutomatically(true);
    settings->setJavaScriptOpenWindowsAutomatically(true);
    settings->setZoomToFitOnLoad(false);
    settings->setDefaultFontSize(16);
    settings->setDefaultFixedFontSize(13);
    settings->setMinimumFontSize(1);
    settings->setSerifFontFamily("Times");
    settings->setFixedFontFamily("Courier New");
    settings->setSansSerifFontFamily("Arial");
    settings->setStandardFontFamily("Times");
    settings->setXSSAuditorEnabled(false);
    settings->setFrameFlatteningEnabled(false);
    settings->setMaximumPagesInCache(0);
    settings->setPluginsEnabled(true);
    // Apply new settings to current page, see more in the destructor of WebSettingsTransaction.
    WebSettingsTransaction webSettingTransaction(settings);

    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->clearBackForwardList(false);

    setAcceptsEditing(true);
    DumpRenderTreeSupport::setLinksIncludedInFocusChain(true);

    m_page->setVirtualViewportSize(800, 600);
    m_page->resetVirtualViewportOnCommitted(false);
    m_page->setUserScalable(true);
    m_page->setJavaScriptCanAccessClipboard(true);

    if (WebCore::Page* page = DumpRenderTreeSupport::corePage(m_page)) {
        page->setTabKeyCyclesThroughElements(true);
        page->settings()->setEditingBehaviorType(WebCore::EditingUnixBehavior);
        page->settings()->setDOMPasteAllowed(true);
        page->settings()->setValidationMessageTimerMagnification(-1);
        page->settings()->setInteractiveFormValidationEnabled(true);
        page->settings()->setAllowFileAccessFromFileURLs(true);
        page->settings()->setAllowUniversalAccessFromFileURLs(true);
        page->settings()->setAuthorAndUserStylesEnabled(true);
        page->settings()->setUsePreHTML5ParserQuirks(false);
        // FIXME: Other ports also clear history/backForwardList allong with visited links.
        page->group().removeVisitedLinks();
        if (mainFrame = page->mainFrame()) {
            mainFrame->tree()->clearName();
            mainFrame->loader()->setOpener(0);
        }
    }

    // For now we manually garbage collect between each test to make sure the device won't run out of memory due to lazy collection.
    DumpRenderTreeSupport::garbageCollectorCollect();
}

void DumpRenderTree::runTests()
{
    m_gcController = new GCController();
    m_accessibilityController = new AccessibilityController();
    getTestsToRun();

    mainFrame = DumpRenderTreeSupport::corePage(m_page)->mainFrame();

    m_currentTest = m_tests.begin();

    if (m_currentTest == m_tests.end()) {
        doneDrt();
        return;
    }
    runCurrentTest();
}


String DumpRenderTree::dumpFramesAsText(WebCore::Frame* frame)
{
    String s;
    WebCore::Element* documentElement = frame->document()->documentElement();
    if (!documentElement)
        return s.utf8().data();

    if (frame->tree()->parent())
        s = String::format("\n--------\nFrame: '%s'\n--------\n", frame->tree()->uniqueName().string().utf8().data());

    s += documentElement->innerText() + "\n";

    if (gLayoutTestController->dumpChildFramesAsText()) {
        WebCore::FrameTree* tree = frame->tree();
        for (WebCore::Frame* child = tree->firstChild(); child; child = child->tree()->nextSibling())
            s += dumpFramesAsText(child);
    }
    return s;
}

static void dumpToFile(const String& data)
{
    fwrite(data.utf8().data(), 1, data.utf8().length(), stdout);
}

bool DumpRenderTree::isHTTPTest(const String& test)
{
    if (test.length() < strlen(httpTestSyntax))
        return false;
    String testLower = test.lower();
    int lenHttpTestSyntax = strlen(httpTestSyntax);
    return testLower.substring(0, lenHttpTestSyntax) == httpTestSyntax
            && testLower.substring(lenHttpTestSyntax, strlen(localTestSyntax)) != localTestSyntax;
}

void DumpRenderTree::getTestsToRun()
{
    Vector<String> files;

    FILE* fd = fopen(m_indexFile.utf8().data(), "r");
    fseek(fd, 0, SEEK_END);
    int size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    OwnArrayPtr<char> buf = adoptArrayPtr(new char[size]);
    fread(buf.get(), 1, size, fd);
    fclose(fd);
    String s(buf.get(), size);
    s.split("\n", files);

    m_tests = files;

    // Find ref-tests for each of the real tests, one test may have multiple expected ref-tests
    // and multiple mismatch ref-tests.
    for (Vector<String>::iterator iter = files.begin(); files.end() != iter; ++iter)
        getRefTests(*iter);
}

void DumpRenderTree::invalidateAnyPreviousWaitToDumpWatchdog()
{
    m_waitToDumpWatchdogTimer.stop();
    waitForPolicy = false;
}

String DumpRenderTree::renderTreeDump() const
{
    if (mainFrame) {
        if (mainFrame->view() && mainFrame->view()->layoutPending())
            mainFrame->view()->layout();

        return externalRepresentation(mainFrame);
    }
    return "";
}

static bool historyItemCompare(const RefPtr<WebCore::HistoryItem>& a, const RefPtr<WebCore::HistoryItem>& b)
{
    return codePointCompare(a->urlString(), b->urlString()) < 0;
}

static String dumpHistoryItem(PassRefPtr<WebCore::HistoryItem> item, int indent, bool current)
{
    String result;

    int start = 0;
    if (current) {
        result += "curr->";
        start = 6;
    }
    for (int i = start; i < indent; i++)
        result += " ";

    String url = item->urlString();
    if (url.contains("file://")) {
        static String layoutTestsString("/LayoutTests/");
        static String fileTestString("(file test):");

        String res = url.substring(url.find(layoutTestsString) + layoutTestsString.length());
        if (res.isEmpty())
            return result;

        result += fileTestString;
        result += res;
    } else
        result += url;

    String target = item->target();
    if (!target.isEmpty())
        result += " (in frame \"" + target + "\")";

    if (item->isTargetItem())
        result += "  **nav target**";
    result += "\n";

    WebCore::HistoryItemVector children = item->children();
    // Must sort to eliminate arbitrary result ordering which defeats reproducible testing.
    nonCopyingSort(children.begin(), children.end(), historyItemCompare);
    unsigned resultSize = children.size();
    for (unsigned i = 0; i < resultSize; ++i)
        result += dumpHistoryItem(children[i], indent + 4, false);

    return result;
}

static String dumpBackForwardListForWebView()
{
    String result = "\n============== Back Forward List ==============\n";
    // FORMAT:
    // "        (file test):fast/loader/resources/click-fragment-link.html  **nav target**"
    // "curr->  (file test):fast/loader/resources/click-fragment-link.html#testfragment  **nav target**"
    WebCore::BackForwardListImpl* bfList = static_cast<WebCore::BackForwardListImpl*>(mainFrame->page()->backForward()->client());
    int maxItems = bfList->capacity();
    WebCore::HistoryItemVector entries;
    bfList->backListWithLimit(maxItems, entries);
    unsigned resultSize = entries.size();
    for (unsigned i = 0; i < resultSize; ++i)
        result += dumpHistoryItem(entries[i], 8, false);

    result += dumpHistoryItem(bfList->currentItem(), 8, true);

    bfList->forwardListWithLimit(maxItems, entries);
    resultSize = entries.size();
    for (unsigned i = 0; i < resultSize; ++i)
        result += dumpHistoryItem(entries[i], 8, false);

    result += "===============================================\n";

    return result;
}

void DumpRenderTree::dump()
{
    invalidateAnyPreviousWaitToDumpWatchdog();

    String dumpFile = m_resultsDir + *m_currentTest + ".dump";

    String resultMimeType = "text/plain";
    String responseMimeType = mainFrame->loader()->documentLoader()->responseMIMEType();

    bool dumpAsText = gLayoutTestController->dumpAsText() || responseMimeType == "text/plain";
    String data = dumpAsText ? dumpFramesAsText(mainFrame) : renderTreeDump();

    if (gLayoutTestController->dumpBackForwardList())
        data += dumpBackForwardListForWebView();

    String result = "Content-Type: " + resultMimeType + "\n" + data;

    dumpToFile(result);
    if (m_dumpPixels && !dumpAsText && gLayoutTestController->generatePixelResults())
        dumpWebViewAsPixelsAndCompareWithExpected(gLayoutTestController->expectedPixelHash());

    String crashFile = dumpFile + ".crash";
    unlink(crashFile.utf8().data());

    testDone = true;
    runRemainingTests();
}

void DumpRenderTree::setWaitToDumpWatchdog(double interval)
{
    invalidateAnyPreviousWaitToDumpWatchdog();
    m_waitToDumpWatchdogTimer.startOneShot(interval);
}

void DumpRenderTree::waitToDumpWatchdogTimerFired(WebCore::Timer<DumpRenderTree>*)
{
    gLayoutTestController->waitToDumpWatchdogTimerFired();
}

void DumpRenderTree::processWork(WebCore::Timer<DumpRenderTree>*)
{
    if (topLoadingFrame)
        return;

    if (WorkQueue::shared()->processWork() && !gLayoutTestController->waitToDump())
        dump();
}

void DumpRenderTree::locationChangeForFrame(WebCore::Frame* frame)
{
    if (frame != topLoadingFrame)
        return;

    topLoadingFrame = 0;
    WorkQueue::shared()->setFrozen(true); // first complete load freezes the queue
    if (gLayoutTestController->waitToDump())
        return;

    if (WorkQueue::shared()->count())
        m_workTimer.startOneShot(0);
    else
        dump();
}

// FrameLoadClient delegates.
void DumpRenderTree::didStartProvisionalLoadForFrame(WebCore::Frame* frame)
{
    if (!testDone && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didStartProvisionalLoadForFrame\n", drtFrameDescription(frame).utf8().data());

    if (!testDone && gLayoutTestController->dumpUserGestureInFrameLoadCallbacks())
        printf("Frame with user gesture \"%s\" - in didStartProvisionalLoadForFrame\n", WebCore::ScriptController::processingUserGesture() ? "true" : "false");

    if (!topLoadingFrame && !testDone)
        topLoadingFrame = frame;

    if (!testDone && gLayoutTestController->stopProvisionalFrameLoads()) {
        printf("%s - stopping load in didStartProvisionalLoadForFrame callback\n", drtFrameDescription(frame).utf8().data());
        frame->loader()->stopForUserCancel();
    }
}

void DumpRenderTree::didCommitLoadForFrame(WebCore::Frame* frame)
{
    if (!testDone && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didCommitLoadForFrame\n", drtFrameDescription(frame).utf8().data());

    gLayoutTestController->setWindowIsKey(true);
}

void DumpRenderTree::didFailProvisionalLoadForFrame(WebCore::Frame* frame)
{
    if (!testDone && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didFailProvisionalLoadWithError\n", drtFrameDescription(frame).utf8().data());

    locationChangeForFrame(frame);
}

void DumpRenderTree::didFailLoadForFrame(WebCore::Frame* frame)
{
    if (!testDone && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didFailLoadWithError\n", drtFrameDescription(frame).utf8().data());

    locationChangeForFrame(frame);
}

void DumpRenderTree::didFinishLoadForFrame(WebCore::Frame* frame)
{
    if (!testDone && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didFinishLoadForFrame\n", drtFrameDescription(frame).utf8().data());

    if (frame == topLoadingFrame)
        m_loadFinished = true;
    locationChangeForFrame(frame);
}

void DumpRenderTree::didFinishDocumentLoadForFrame(WebCore::Frame* frame)
{
    if (!testDone) {
        if (gLayoutTestController->dumpFrameLoadCallbacks())
            printf("%s - didFinishDocumentLoadForFrame\n", drtFrameDescription(frame).utf8().data());
        else {
            unsigned pendingFrameUnloadEvents = frame->domWindow()->pendingUnloadEventListeners();
            if (pendingFrameUnloadEvents)
                printf("%s - has %u onunload handler(s)\n", drtFrameDescription(frame).utf8().data(), pendingFrameUnloadEvents);
        }
    }
}

void DumpRenderTree::didClearWindowObjectInWorld(WebCore::DOMWrapperWorld*, JSGlobalContextRef context, JSObjectRef windowObject)
{
    JSValueRef exception = 0;

    gLayoutTestController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    m_gcController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    m_accessibilityController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    JSStringRef eventSenderStr = JSStringCreateWithUTF8CString("eventSender");
    JSValueRef eventSender = makeEventSender(context);
    JSObjectSetProperty(context, windowObject, eventSenderStr, eventSender, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, 0);
    JSStringRelease(eventSenderStr);
    WebCoreTestSupport::injectInternalsObject(context);
}

void DumpRenderTree::didReceiveTitleForFrame(const String& title, WebCore::Frame* frame)
{
    if (!testDone && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didReceiveTitle: %s\n", drtFrameDescription(frame).utf8().data(), title.utf8().data());

    if (gLayoutTestController->dumpTitleChanges())
        printf("TITLE CHANGED: %s\n", title.utf8().data());
}

// ChromeClient delegates.
void DumpRenderTree::addMessageToConsole(const String& message, unsigned int lineNumber, const String& sourceID)
{
    printf("CONSOLE MESSAGE: line %d: %s\n", lineNumber, message.utf8().data());
}

void DumpRenderTree::runJavaScriptAlert(const String& message)
{
    if (!testDone)
        printf("ALERT: %s\n", message.utf8().data());
}

bool DumpRenderTree::runJavaScriptConfirm(const String& message)
{
    if (!testDone)
        printf("CONFIRM: %s\n", message.utf8().data());
    return true;
}

String DumpRenderTree::runJavaScriptPrompt(const String& message, const String& defaultValue)
{
    if (!testDone)
        printf("PROMPT: %s, default text: %s\n", message.utf8().data(), defaultValue.utf8().data());
    return defaultValue;
}

bool DumpRenderTree::runBeforeUnloadConfirmPanel(const String& message)
{
    if (!testDone)
        printf("CONFIRM NAVIGATION: %s\n", message.utf8().data());
    return true;
}

void DumpRenderTree::setStatusText(const String& status)
{
    if (gLayoutTestController->dumpStatusCallbacks())
        printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n", status.utf8().data());
}

void DumpRenderTree::exceededDatabaseQuota(WebCore::SecurityOrigin* origin, const String& name)
{
    if (!testDone && gLayoutTestController->dumpDatabaseCallbacks())
        printf("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:{%s, %s, %i} database:%s\n", origin->protocol().utf8().data(), origin->host().utf8().data(), origin->port(), name.utf8().data());

    WebCore::DatabaseTracker::tracker().setQuota(mainFrame->document()->securityOrigin(), 5 * 1024 * 1024);
}

bool DumpRenderTree::allowsOpeningWindow()
{
    return gLayoutTestController->canOpenWindows();
}

void DumpRenderTree::windowCreated(BlackBerry::WebKit::WebPage* page)
{
    page->settings()->setJavaScriptOpenWindowsAutomatically(true);
}

// EditorClient delegates.
void DumpRenderTree::didBeginEditing()
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: webViewDidBeginEditing:%s\n", "WebViewDidBeginEditingNotification");
}

void DumpRenderTree::didEndEditing()
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: webViewDidEndEditing:%s\n", "WebViewDidEndEditingNotification");
}

void DumpRenderTree::didChange()
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: webViewDidChange:%s\n", "WebViewDidChangeNotification");
}

void DumpRenderTree::didChangeSelection()
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: webViewDidChangeSelection:%s\n", "WebViewDidChangeSelectionNotification");
}

bool DumpRenderTree::findString(const String& string, WebCore::FindOptions options)
{
    WebCore::Page* page = mainFrame ? mainFrame->page() : 0;
    return page && page->findString(string, options);
}

bool DumpRenderTree::shouldBeginEditingInDOMRange(WebCore::Range* range)
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: shouldBeginEditingInDOMRange:%s\n", drtRangeDescription(range).utf8().data());
    return m_acceptsEditing;
}

bool DumpRenderTree::shouldEndEditingInDOMRange(WebCore::Range* range)
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: shouldEndEditingInDOMRange:%s\n", drtRangeDescription(range).utf8().data());
    return m_acceptsEditing;
}

bool DumpRenderTree::shouldDeleteDOMRange(WebCore::Range* range)
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: shouldDeleteDOMRange:%s\n", drtRangeDescription(range).utf8().data());
    return m_acceptsEditing;
}

bool DumpRenderTree::shouldChangeSelectedDOMRangeToDOMRangeAffinityStillSelecting(WebCore::Range* fromRange, WebCore::Range* toRange, int affinity, bool stillSelecting)
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: shouldChangeSelectedDOMRange:%s toDOMRange:%s affinity:%s stillSelecting:%s\n", drtRangeDescription(fromRange).utf8().data(), drtRangeDescription(toRange).utf8().data(), drtAffinityDescription(static_cast<WebCore::EAffinity>(affinity)).utf8().data(), stillSelecting ? "TRUE" : "FALSE");
    return m_acceptsEditing;
}

static const char* insertActionString(WebCore::EditorInsertAction action)
{
    switch (action) {
    case WebCore::EditorInsertActionTyped:
        return "WebViewInsertActionTyped";
    case WebCore::EditorInsertActionPasted:
        return "WebViewInsertActionPasted";
    case WebCore::EditorInsertActionDropped:
        return "WebViewInsertActionDropped";
    }
    ASSERT_NOT_REACHED();
    return "WebViewInsertActionTyped";
}

bool DumpRenderTree::shouldInsertNode(WebCore::Node* node, WebCore::Range* range, int action)
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: shouldInsertNode:%s replacingDOMRange:%s givenAction:%s\n", drtDumpPath(node).utf8().data(), drtRangeDescription(range).utf8().data(), insertActionString((WebCore::EditorInsertAction)action));
    return m_acceptsEditing;
}

bool DumpRenderTree::shouldInsertText(const String& text, WebCore::Range* range, int action)
{
    if (!testDone && gLayoutTestController->dumpEditingCallbacks())
        printf("EDITING DELEGATE: shouldInsertText:%s replacingDOMRange:%s givenAction:%s\n", text.utf8().data(), drtRangeDescription(range).utf8().data(), insertActionString((WebCore::EditorInsertAction)action));
    return m_acceptsEditing;
}

void DumpRenderTree::didDecidePolicyForNavigationAction(const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request)
{
    if (!waitForPolicy)
        return;

    const char* typeDescription;
    switch (action.type()) {
    case WebCore::NavigationTypeLinkClicked:
        typeDescription = "link clicked";
        break;
    case WebCore::NavigationTypeFormSubmitted:
        typeDescription = "form submitted";
        break;
    case WebCore::NavigationTypeBackForward:
        typeDescription = "back/forward";
        break;
    case WebCore::NavigationTypeReload:
        typeDescription = "reload";
        break;
    case WebCore::NavigationTypeFormResubmitted:
        typeDescription = "form resubmitted";
        break;
    case WebCore::NavigationTypeOther:
        typeDescription = "other";
        break;
    default:
        typeDescription = "illegal value";
    }

    printf("Policy delegate: attempt to load %s with navigation type '%s'\n", request.url().string().utf8().data(), typeDescription);
    // FIXME: do originating part.

    gLayoutTestController->notifyDone();
}

void DumpRenderTree::didDispatchWillPerformClientRedirect()
{
    if (!testDone && gLayoutTestController->dumpUserGestureInFrameLoadCallbacks())
        printf("Frame with user gesture \"%s\" - in willPerformClientRedirect\n", WebCore::ScriptController::processingUserGesture() ? "true" : "false");
}

void DumpRenderTree::didHandleOnloadEventsForFrame(WebCore::Frame* frame)
{
    if (!testDone && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didHandleOnloadEventsForFrame\n", drtFrameDescription(frame).utf8().data());
}

void DumpRenderTree::didReceiveResponseForFrame(WebCore::Frame* frame, const WebCore::ResourceResponse& response)
{
    if (!testDone && gLayoutTestController->dumpResourceResponseMIMETypes())
        printf("%s has MIME type %s\n", response.url().lastPathComponent().utf8().data(), response.mimeType().utf8().data());
}

}
}

// Static dump() function required by cross-platform DRT code.
void dump()
{
    BlackBerry::WebKit::DumpRenderTree* dumper = BlackBerry::WebKit::DumpRenderTree::currentInstance();
    if (!dumper)
        return;

    dumper->dump();
}
