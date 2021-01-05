/*
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
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

#include "config.h"
#include "DumpRenderTree.h"

// ScriptController must be before NotImplemented...
#include "ScriptController.h"

#include "AccessibilityController.h"
#include "DumpRenderTreeClient.h"
#include "EditingCallbacks.h"
#include "EventSender.h"
#include "Frame.h"
#include <GCController.h>
#include "PixelDumpSupport.h"
#include "TestCommand.h"
#include "TestRunner.h"
#include "WebCoreTestSupport.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebSettings.h"
#include "WebView.h"
#include "WebViewConstants.h"
#include "WebWindow.h"
#include "WorkQueue.h"

#include <wtf/Assertions.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#include <Application.h>
#include <File.h>
#include <JavaScriptCore/JavaScript.h>
#include <OS.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <vector>


BWebView* webView;
BWebFrame* topLoadingFrame = 0;
bool waitForPolicy = false;
BMessageRunner* waitToDumpWatchdog = NULL;

// From the top-level DumpRenderTree.h
RefPtr<TestRunner> gTestRunner;
volatile bool done = true;
volatile bool first = true;

static bool dumpPixelsForCurrentTest;
static int dumpPixelsForAllTests = false;
static bool dumpTree = true;
static bool printSeparators = true;
static int useTimeoutWatchdog = true;

using namespace std;

const unsigned maxViewHeight = 600;
const unsigned maxViewWidth = 800;

static String dumpFramesAsText(BWebFrame* frame)
{
    if (!frame)
        return String();

    String result = frame->InnerText();

    if (webView->WebPage()->MainFrame() != frame) {
        result.append("\n--------\nFrame: '");
        result.append(frame->Name());
        result.append("'\n--------\n");
    }

    result.append("\n");

    if (gTestRunner->dumpChildFramesAsText()) {
        BList children = WebCore::DumpRenderTreeClient::frameChildren(frame);

        for(int i = 0; i < children.CountItems(); i++)
        {
            BWebFrame* currentFrame = static_cast<BWebFrame*>(children.ItemAt(i));
            String tempText(dumpFramesAsText(currentFrame));

            if (tempText.isEmpty())
                continue;

            result.append(tempText);
        }
    }

    return result;
}

static void dumpFrameScrollPosition(BWebFrame* frame)
{
    BPoint pos = frame->ScrollPosition();
    if (abs(pos.x) > 0 || abs(pos.y) > 0) {
        StringBuilder result;

#if 0
        Evas_Object* parent = evas_object_smart_parent_get(frame);

        // smart parent of main frame is view object.
        if (parent != browser->mainView()) {
            result.append("frame '");
            result.append(ewk_frame_name_get(frame));
            result.append("' ");
        }
#endif

        printf("scrolled to %.f,%.f\n", pos.x, pos.y);
    }

    if (gTestRunner->dumpChildFrameScrollPositions()) {
#if 0
        Eina_List* children = DumpRenderTreeSupportEfl::frameChildren(frame);
        void* iterator;

        EINA_LIST_FREE(children, iterator) {
            Evas_Object* currentFrame = static_cast<Evas_Object*>(iterator);
            dumpFrameScrollPosition(currentFrame);
        }
#endif
    }
}

static void adjustOutputTypeByMimeType(BWebFrame* frame)
{
    const String responseMimeType(WebCore::DumpRenderTreeClient::responseMimeType(frame));
    if (responseMimeType == "text/plain") {
        gTestRunner->setDumpAsText(true);
        gTestRunner->setGeneratePixelResults(false);
    }
}

static void dumpFrameContentsAsText(BWebFrame* frame)
{
    String result;
    if (gTestRunner->dumpAsText())
        result = dumpFramesAsText(frame);
    else
        result = frame->ExternalRepresentation();

    printf("%s", result.utf8().data());
}

static bool shouldDumpFrameScrollPosition()
{
    return !gTestRunner->dumpAsText() && !gTestRunner->dumpDOMAsWebArchive() && !gTestRunner->dumpSourceAsWebArchive();
}

static bool shouldDumpPixelsAndCompareWithExpected()
{
    return dumpPixelsForCurrentTest && gTestRunner->generatePixelResults() && !gTestRunner->dumpDOMAsWebArchive() && !gTestRunner->dumpSourceAsWebArchive();
}

void dump()
{
    BWebFrame* frame = webView->WebPage()->MainFrame();

    if (dumpTree) {
        adjustOutputTypeByMimeType(frame);
        dumpFrameContentsAsText(frame);

        if (shouldDumpFrameScrollPosition())
            dumpFrameScrollPosition(frame);

        if (gTestRunner->dumpBackForwardList()) {
            // FIXME:
            // not implemented.
        }

        if (printSeparators) {
            puts("#EOF");
            fputs("#EOF\n", stderr);
            fflush(stdout);
            fflush(stderr);
        }
    }

    if(shouldDumpPixelsAndCompareWithExpected()) {
        dumpWebViewAsPixelsAndCompareWithExpected(gTestRunner->expectedPixelHash());
    }

    // Notify the BApplication it's ok to move on to the next test.
    be_app->PostMessage('dump');
}

static bool shouldLogFrameLoadDelegates(const String& pathOrURL)
{
    return pathOrURL.contains("loading/");
}

static bool shouldDumpAsText(const String& pathOrURL)
{
    return pathOrURL.contains("dumpAsText/");
}

static bool shouldOpenWebInspector(const String& pathOrURL)
{
    return pathOrURL.contains("inspector/");
}

static String getFinalTestURL(const String& testURL)
{
    if (!testURL.startsWith("http://") && !testURL.startsWith("https://")) {
        char* cFilePath = realpath(testURL.utf8().data(), NULL);
        const String filePath = String::fromUTF8(cFilePath);
        free(cFilePath);

        if (BFile(filePath.utf8().data(), B_READ_ONLY).IsFile())
            return String("file://") + filePath;
    }

    return testURL;
}

static inline bool isGlobalHistoryTest(const String& cTestPathOrURL)
{
    return cTestPathOrURL.contains("/globalhistory/");
}

static void createTestRunner(const String& testURL, const String& expectedPixelHash)
{
    gTestRunner =
        TestRunner::create(std::string(testURL.utf8().data()),
                                     std::string(expectedPixelHash.utf8().data()));

    topLoadingFrame = 0;
    done = false;
    first = true;

    gTestRunner->setIconDatabaseEnabled(false);

    if (shouldLogFrameLoadDelegates(testURL))
        gTestRunner->setDumpFrameLoadCallbacks(true);

    if (shouldOpenWebInspector(testURL))
        gTestRunner->showWebInspector();

    gTestRunner->setDumpHistoryDelegateCallbacks(isGlobalHistoryTest(testURL));

    if (shouldDumpAsText(testURL)) {
        gTestRunner->setDumpAsText(true);
        gTestRunner->setGeneratePixelResults(false);
    }
}

static void resetDefaultsToConsistentValues()
{
#if 0
    ewk_settings_icon_database_clear();
    ewk_settings_icon_database_path_set(0);

    ewk_web_database_remove_all();
    ewk_settings_web_database_default_quota_set(5 * 1024 * 1024);

    ewk_settings_memory_cache_clear();
    ewk_settings_application_cache_clear();
    ewk_settings_shadow_dom_enable_set(EINA_TRUE);

    ewk_view_setting_private_browsing_set(mainView(), EINA_FALSE);
    ewk_view_setting_spatial_navigation_set(mainView(), EINA_FALSE);
    ewk_view_setting_enable_frame_flattening_set(mainView(), EINA_FALSE);
    ewk_view_setting_application_cache_set(mainView(), EINA_TRUE);
    ewk_view_setting_enable_scripts_set(mainView(), EINA_TRUE);
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_STANDARD, "Times");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_MONOSPACE, "Courier");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_SERIF, "Times");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_SANS_SERIF, "Helvetica");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_CURSIVE, "cursive");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_FANTASY, "fantasy");
#endif
    webView->WebPage()->Settings()->SetDefaultStandardFontSize(16);
    webView->WebPage()->Settings()->SetDefaultFixedFontSize(13);
#if 0
    ewk_view_setting_font_minimum_size_set(mainView(), 0);
    ewk_view_setting_caret_browsing_set(mainView(), EINA_FALSE);
    ewk_view_setting_page_cache_set(mainView(), EINA_FALSE);
    ewk_view_setting_enable_auto_resize_window_set(mainView(), EINA_TRUE);
    ewk_view_setting_enable_plugins_set(mainView(), EINA_TRUE);
    ewk_view_setting_scripts_can_open_windows_set(mainView(), EINA_TRUE);
    ewk_view_setting_scripts_can_close_windows_set(mainView(), EINA_TRUE);
    ewk_view_setting_auto_load_images_set(mainView(), EINA_TRUE);
    ewk_view_setting_user_stylesheet_set(mainView(), 0);
    ewk_view_setting_enable_xss_auditor_set(browser->mainView(), EINA_TRUE);
    ewk_view_setting_enable_webgl_set(mainView(), EINA_TRUE);
    ewk_view_setting_enable_hyperlink_auditing_set(mainView(), EINA_FALSE);
    ewk_view_setting_include_links_in_focus_chain_set(mainView(), EINA_FALSE);
    ewk_view_setting_scripts_can_access_clipboard_set(mainView(), EINA_TRUE);
    ewk_view_setting_allow_universal_access_from_file_urls_set(mainView(), EINA_TRUE);
    ewk_view_setting_allow_file_access_from_file_urls_set(mainView(), EINA_TRUE);
    ewk_view_setting_resizable_textareas_set(mainView(), EINA_TRUE);

#endif
    // This resets both the "text" and "page" zooms.
    webView->ResetZoomFactor();
#if 0
    ewk_view_visibility_state_set(mainView(), EWK_PAGE_VISIBILITY_STATE_VISIBLE, true);
    ewk_view_text_direction_set(mainView(), EWK_TEXT_DIRECTION_DEFAULT);

    ewk_history_clear(ewk_view_history_get(mainView()));

    ewk_frame_feed_focus_in(mainFrame());

    ewk_cookies_clear();
    ewk_cookies_policy_set(EWK_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);

    ewk_security_policy_whitelist_origin_reset();

#if HAVE(ACCESSIBILITY)
    browser->accessibilityController()->resetToConsistentState();
#endif

    DumpRenderTreeSupportEfl::clearFrameName(mainFrame());
    DumpRenderTreeSupportEfl::clearOpener(mainFrame());
#endif
    WebCore::DumpRenderTreeClient::clearUserScripts(webView);
#if 0
    DumpRenderTreeSupportEfl::clearUserStyleSheets(mainView());
    DumpRenderTreeSupportEfl::resetGeolocationClientMock(mainView());
    DumpRenderTreeSupportEfl::setInteractiveFormValidationEnabled(mainView(), true);
    DumpRenderTreeSupportEfl::setValidationMessageTimerMagnification(mainView(), -1);
    DumpRenderTreeSupportEfl::setAuthorAndUserStylesEnabled(mainView(), true);
    DumpRenderTreeSupportEfl::setCSSGridLayoutEnabled(mainView(), false);
    DumpRenderTreeSupportEfl::setDefersLoading(mainView(), false);
    DumpRenderTreeSupportEfl::setLoadsSiteIconsIgnoringImageLoadingSetting(mainView(), false);
    DumpRenderTreeSupportEfl::setSerializeHTTPLoads(false);
    DumpRenderTreeSupportEfl::setMinimumLogicalFontSize(mainView(), 9);
    DumpRenderTreeSupportEfl::setCSSRegionsEnabled(mainView(), true);
    DumpRenderTreeSupportEfl::setShouldTrackVisitedLinks(false);
    DumpRenderTreeSupportEfl::setTracksRepaints(mainFrame(), false);
    DumpRenderTreeSupportEfl::setSeamlessIFramesEnabled(true);
    DumpRenderTreeSupportEfl::setWebAudioEnabled(mainView(), false);

    // Reset capacities for the memory cache for dead objects.
    static const unsigned cacheTotalCapacity =  8192 * 1024;
    ewk_settings_object_cache_capacity_set(0, cacheTotalCapacity, cacheTotalCapacity);
    DumpRenderTreeSupportEfl::setDeadDecodedDataDeletionInterval(0);
    ewk_settings_page_cache_capacity_set(3);

    policyDelegateEnabled = false;
    policyDelegatePermissive = false;
#endif
}

static void runTest(const string& inputLine)
{
    auto command = WTR::parseInputLine(inputLine);
    const String testPathOrURL(command.pathOrURL.c_str());
    ASSERT(!testPathOrURL.isEmpty());
    dumpPixelsForCurrentTest = command.shouldDumpPixels || dumpPixelsForAllTests;
    const String expectedPixelHash(command.expectedPixelHash.c_str());

    // Convert the path into a full file URL if it does not look
    // like an HTTP/S URL (doesn't start with http:// or https://).
    const String testURL = getFinalTestURL(testPathOrURL);

    resetDefaultsToConsistentValues();
    createTestRunner(testURL, expectedPixelHash);

	DRT::WorkQueue::singleton().clear();
	DRT::WorkQueue::singleton().setFrozen(false);

    const bool isSVGW3CTest = testURL.contains("svg/W3C-SVG-1.1");
    const int width = isSVGW3CTest ? TestRunner::w3cSVGViewWidth : TestRunner::viewWidth;
    const int height = isSVGW3CTest ? TestRunner::w3cSVGViewHeight : TestRunner::viewHeight;
    webView->LockLooper();
    webView->ResizeTo(width - 1, height - 1);
    webView->Window()->ResizeTo(width - 1, height - 1);
    webView->UnlockLooper();

    // TODO efl does some history cleanup here
    
    webView->WebPage()->MainFrame()->LoadURL(BString(testURL));
}

#pragma mark -

class DumpRenderTreeApp : public BApplication, WebCore::DumpRenderTreeClient {
public:
    DumpRenderTreeApp();
    ~DumpRenderTreeApp() {
        delete m_webWindow->CurrentWebView();
    }

    // BApplication
    void ArgvReceived(int32 argc, char** argv) override;
    void ReadyToRun() override;
    void MessageReceived(BMessage*) override;

    // DumpRenderTreeClient
    void didClearWindowObjectInWorld(WebCore::DOMWrapperWorld&,
        JSGlobalContextRef context, JSObjectRef windowObject) override;

    static status_t runTestFromStdin();

private:
    void topLoadingFrameLoadFinished();
private:
    GCController* m_gcController;
    AccessibilityController* m_accessibilityController;
    BWebWindow* m_webWindow;

    int m_currentTest;
    vector<const char*> m_tests;
    bool m_fromStdin;
};


DumpRenderTreeApp::DumpRenderTreeApp()
    : BApplication("application/x-vnd.DumpRenderTree")
{
}

void DumpRenderTreeApp::ArgvReceived(int32 argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--notree")) {
            dumpTree = false;
            continue;
        }

        if (!strcmp(argv[i], "--pixel-tests")) {
            dumpPixelsForAllTests = true;
            continue;
        }

        if (!strcmp(argv[i], "--tree")) {
            dumpTree = true;
            continue;
        }

        if (!strcmp(argv[i], "--no-timeout")) {
            useTimeoutWatchdog = true;
            continue;
        }

        char* str = new char[strlen(argv[i]) + 1];
        strcpy(str, argv[i]);
        m_tests.push_back(str);
    }
}

class DumpRenderTreeChrome: public BWebWindow
{
    public:
    DumpRenderTreeChrome()
        : BWebWindow(BRect(0, 0, maxViewWidth, maxViewHeight), "DumpRenderTree",
            B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, 0)
    {
    }

    void MessageReceived(BMessage* msg)
    {
        if (msg->what == SSL_CERT_ERROR)
        {
            // Always accept SSL certificates, since the test suite uses an
            // auto-generated one and that would normally produce warnings.
            BMessage reply;
            reply.AddBool("continue", true);
            msg->SendReply(&reply);
            return;
        }

        BWebWindow::MessageReceived(msg);
    }
};

void DumpRenderTreeApp::ReadyToRun()
{
    BWebPage::InitializeOnce();
    BWebPage::SetCacheModel(B_WEBKIT_CACHE_MODEL_WEB_BROWSER);

    JSC::Config::configureForTesting();

    // Create the main application window.
    m_webWindow = new DumpRenderTreeChrome();
    m_webWindow->SetSizeLimits(0, maxViewWidth - 1, 0, maxViewHeight - 1);
    m_webWindow->ResizeTo(maxViewWidth - 1, maxViewHeight - 1);

    DumpRenderTreeClient::setMockScrollbarsEnabled(true);

    webView = new BWebView("DumpRenderTree");
    m_webWindow->AddChild(webView);
    m_webWindow->SetCurrentWebView(webView);
    webView->ResizeTo(maxViewWidth - 1, maxViewHeight - 1);

    webView->WebPage()->SetListener(this);
    Register(webView->WebPage());
    webView->WebPage()->Settings()->SetDefaultStandardFontSize(16);
    webView->WebPage()->Settings()->SetDefaultFixedFontSize(13);
        // Make sure we use the same metrics are others for the tests.
    webView->WebPage()->Settings()->Apply();

    // Start the looper, but keep the window hidden
    m_webWindow->Hide();
    m_webWindow->Show();

    if (m_tests.size() == 1 && !strcmp(m_tests[0], "-")) {
        printSeparators = true;
        m_fromStdin = true;
        runTestFromStdin();
    } else if(m_tests.size() != 0) {
        printSeparators = (m_tests.size() > 1 || (dumpPixelsForAllTests && dumpTree));
        m_fromStdin = false;
        runTest(m_tests[0]);
        m_currentTest = 1;
    }
}

bool shouldSetWaitToDumpWatchdog()
{
    return !waitToDumpWatchdog && useTimeoutWatchdog;
}

static void sendPixelResultsEOF()
{
    puts("#EOF");
    fflush(stdout);
    fflush(stderr);
}

void DumpRenderTreeApp::topLoadingFrameLoadFinished()
{
    topLoadingFrame = 0;

	DRT::WorkQueue::singleton().setFrozen(true);
    if (gTestRunner->waitToDump()) {
        return;
    }

    if (DRT::WorkQueue::singleton().count()) {
        // FIXME should wait until the work queue is done before dumping
        // ecore_idler_add(processWork, 0 /*frame*/);
        dump();
    } else {
        dump();
    }
}

extern void watchdogFired();

void DumpRenderTreeApp::MessageReceived(BMessage* message)
{
    switch (message->what) {
    case 'dwdg': {
        watchdogFired();
        break;
    }

    case LOAD_STARTED: {
        // efl: DumpRenderTreeChrome::onLoadStarted
        BWebFrame* frame = NULL;
        message->FindPointer("frame", (void**)&frame);

        // Make sure we only set this once per test. If it gets cleared, and
        // then set again, we might end up doing two dumps for one test.
        if (!topLoadingFrame && first) {
            first = false;
            topLoadingFrame = frame;
        }
        break;
    }

    case RESPONSE_RECEIVED: {
        if (!done && gTestRunner->dumpResourceLoadCallbacks()) {
#if 0
            CString responseDescription(descriptionSuitableForTestResult(response));
            printf("%s - didReceiveResponse %s\n",
                m_dumpAssignedUrls.contains(response->identifier) ? m_dumpAssignedUrls.get(response->identifier).data() : "<unknown>",
                responseDescription.data());
#endif
        }

        if (!done && gTestRunner->dumpResourceResponseMIMETypes()) {
            BString mimeType = message->FindString("mimeType");
            BString url = message->FindString("url");
            printf("%s has MIME type %s\n",
                WTF::URL({ }, url).lastPathComponent().utf8().data(),
                mimeType.String());
        }
        break;
    }

    case TITLE_CHANGED: {
        // efl: DumpRenderTreeChrome::onFrameTitleChanged
        BWebFrame* frame = NULL;
        message->FindPointer("frame", (void**)&frame);
        BString title = message->FindString("title");

        if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
            const String frameName(DumpRenderTreeClient::suitableDRTFrameName(
                frame));
            printf("%s - didReceiveTitle: %s\n", frameName.utf8().data(),
                title.String());
        }

        if (!done && gTestRunner->dumpTitleChanges()) {
            printf("TITLE CHANGED: '%s'\n", title.String());
        }

        if (!done && gTestRunner->dumpHistoryDelegateCallbacks()) {
            printf("WebView updated the title for history URL \"%s\" to \"%s\".\n",
                frame->URL().String(), title.String());
        }

        gTestRunner->setTitleTextDirection(message->FindBool("ltr") ? "ltr" : "rtl");
        break;
    }

    case LOAD_FINISHED: {
        // efl: DumpRenderTreeChrome::onFrameLoadFinished
        if (!done && gTestRunner->dumpProgressFinishedCallback())
            printf("postProgressFinishedNotification\n");

        BWebFrame* frame = NULL;
        message->FindPointer("frame", (void**)&frame);
        if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
            const String frameName(DumpRenderTreeClient::suitableDRTFrameName(frame));
            printf("%s - didFinishLoadForFrame\n", frameName.utf8().data());
        }

        if (frame == topLoadingFrame)
            topLoadingFrameLoadFinished();

        break;
    }

    case LOAD_DOC_COMPLETED: {
        //efl: DumpRenderTreeChrome::onDocumentLoadFinished

        BWebFrame* frame = NULL;
        message->FindPointer("frame", (void**)&frame);
        const String frameName(DumpRenderTreeClient::suitableDRTFrameName(frame));

        if (!done && gTestRunner->dumpFrameLoadCallbacks())
            printf("%s - didFinishDocumentLoadForFrame\n", frameName.utf8().data());
        else if (!done) {
            const unsigned pendingFrameUnloadEvents = DumpRenderTreeClient::pendingUnloadEventCount(frame);
            if (pendingFrameUnloadEvents)
                printf("%s - has %u onunload handler(s)\n", frameName.utf8().data(), pendingFrameUnloadEvents);
        }

        break;
    }

    case ADD_CONSOLE_MESSAGE:
    {
        // Follow the format used by other DRTs here. Note this doesn't
        // include the fille URL, making it possible to have consistent
        // results even if the tests are moved around.
        int32 lineNumber = message->FindInt32("line");
        BString text = message->FindString("string");
        printf("CONSOLE MESSAGE: ");
        if (lineNumber)
            printf("line %" B_PRIu32 ": ", lineNumber);
        printf("%s\n", text.String());
        fflush(stdout);
        return;
    }

    case SHOW_JS_ALERT:
    {
        BString text = message->FindString("text");
        printf("ALERT: %s\n", text.String());
        fflush(stdout);
        return;
    }
    case SHOW_JS_CONFIRM:
    {
        BString text = message->FindString("text");
        printf("CONFIRM: %s\n", text.String());
        fflush(stdout);

        BMessage reply;
        reply.AddBool("result", true);
        message->SendReply(&reply);
        return;
    }

    case 'dump':
    {
        // Code from EFL runTest, after the test is done. We do this here as we're
        // sure the test is done running.

        gTestRunner->closeWebInspector();

        //browser->clearExtraViews();

        // FIXME: Move to DRTChrome::resetDefaultsToConsistentValues() after bug 85209 lands.
        WebCoreTestSupport::resetInternalsObject(
            webView->WebPage()->MainFrame()->GlobalContext());

        // TODO efl goes to "about:blank" here. But this triggers an extra
        // dump for us, confusing the test system.

        //gTestRunner.clear();
        sendPixelResultsEOF();

        if (m_fromStdin) {
            // run the next test.
            if(runTestFromStdin() != B_OK) {
                be_app->PostMessage(B_QUIT_REQUESTED);
            }
            break;
        } else {
            if (m_currentTest < m_tests.size()) {
                runTest(m_tests[m_currentTest]);
                m_currentTest++;
            } else {
                be_app->PostMessage(B_QUIT_REQUESTED);
            }
        }
        return;
    }

    default:
        if(!handleEditingCallback(message))
           BApplication::MessageReceived(message);
        break;
    }
}

void DumpRenderTreeApp::didClearWindowObjectInWorld(WebCore::DOMWrapperWorld&, JSGlobalContextRef context, JSObjectRef windowObject)
{
    JSValueRef exception = 0;

    gTestRunner->makeWindowObject(context);
    ASSERT(!exception);

    m_gcController->makeWindowObject(context);
    ASSERT(!exception);

    //m_accessibilityController->makeWindowObject(context, windowObject, &exception);
    //ASSERT(!exception);

    //m_textInputController->makeWindowObject(context, windowObject, &exception);
    //ASSERT(!exception);

    JSStringRef eventSenderStr = JSStringCreateWithUTF8CString("eventSender");
    JSValueRef eventSender = makeEventSender(context);
    JSObjectSetProperty(context, windowObject, eventSenderStr, eventSender, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, 0);
    JSStringRelease(eventSenderStr);

    WebCoreTestSupport::injectInternalsObject(context);
}

status_t DumpRenderTreeApp::runTestFromStdin()
{
    char filenameBuffer[2048];

    if(fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {

        char* newLineCharacter = strchr(filenameBuffer, '\n');

        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (!strlen(filenameBuffer)) {
            // Got an empty line, try again...
            return runTestFromStdin();
        }

        runTest(filenameBuffer);
        return B_OK;
    }

    return B_ERROR;

}

#pragma mark -


int main()
{
    DumpRenderTreeApp app;
    app.Run();
}

