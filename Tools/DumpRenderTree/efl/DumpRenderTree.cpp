/*
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2011 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DumpRenderTree.h"

#include "DumpHistoryItem.h"
#include "DumpRenderTreeChrome.h"
#include "DumpRenderTreeView.h"
#include "EventSender.h"
#include "FontManagement.h"
#include "LayoutTestController.h"
#include "NotImplemented.h"
#include "PixelDumpSupport.h"
#include "WebCoreSupport/DumpRenderTreeSupportEfl.h"
#include "WebCoreTestSupport.h"
#include "WorkQueue.h"
#include "ewk_private.h"
#include <EWebKit.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_File.h>
#include <Edje.h>
#include <Evas.h>
#include <fontconfig/fontconfig.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>

OwnPtr<DumpRenderTreeChrome> browser;
Evas_Object* topLoadingFrame = 0;
bool waitForPolicy = false;
Ecore_Timer* waitToDumpWatchdog = 0;
extern Ewk_History_Item* prevTestBFItem;

// From the top-level DumpRenderTree.h
RefPtr<LayoutTestController> gLayoutTestController;
volatile bool done = false;

static int dumpPixels = false;
static int dumpTree = true;
static int printSeparators = true;
static int useX11Window = false;

static String dumpFramesAsText(Evas_Object* frame)
{
    String result;

    if (browser->mainFrame() != frame) {
        result.append("\n--------\nFrame: '");
        result.append(String::fromUTF8(ewk_frame_name_get(frame)));
        result.append("'\n--------\n");
    }

    char* frameContents = ewk_frame_plain_text_get(frame);
    result.append(String::fromUTF8(frameContents));
    result.append("\n");
    free(frameContents);

    if (gLayoutTestController->dumpChildFramesAsText()) {
        Eina_List* children = DumpRenderTreeSupportEfl::frameChildren(frame);
        void* iterator;

        EINA_LIST_FREE(children, iterator) {
            Evas_Object* currentFrame = static_cast<Evas_Object*>(iterator);
            String tempText(dumpFramesAsText(currentFrame));

            if (tempText.isEmpty())
                continue;

            result.append(tempText);
        }
    }

    return result;
}

static void dumpFrameScrollPosition(Evas_Object*)
{
    notImplemented();
}

static bool shouldLogFrameLoadDelegates(const String& pathOrURL)
{
    return pathOrURL.contains("loading/");
}

static bool shouldDumpAsText(const String& pathOrURL)
{
    return pathOrURL.contains("dumpAsText/");
}

static void sendPixelResultsEOF()
{
    puts("#EOF");
    fflush(stdout);
    fflush(stderr);
}

static void invalidateAnyPreviousWaitToDumpWatchdog()
{
    if (waitToDumpWatchdog) {
        ecore_timer_del(waitToDumpWatchdog);
        waitToDumpWatchdog = 0;
    }
    waitForPolicy = false;
}

static void onEcoreEvasResize(Ecore_Evas* ecoreEvas)
{
    int width, height;

    ecore_evas_geometry_get(ecoreEvas, 0, 0, &width, &height);
    evas_object_move(browser->mainView(), 0, 0);
    evas_object_resize(browser->mainView(), width, height);
}

static void onCloseWindow(Ecore_Evas*)
{
    notImplemented();
}

static Eina_Bool useLongRunningServerMode(int argc, char** argv)
{
    return (argc == optind + 1 && !strcmp(argv[optind], "-"));
}

static bool parseCommandLineOptions(int argc, char** argv)
{
    static const option options[] = {
        {"notree", no_argument, &dumpTree, false},
        {"pixel-tests", no_argument, &dumpPixels, true},
        {"tree", no_argument, &dumpTree, true},
        {"gui", no_argument, &useX11Window, true},
        {0, 0, 0, 0}
    };

    int option;
    while ((option = getopt_long(argc, (char* const*)argv, "", options, 0)) != -1) {
        switch (option) {
        case '?':
        case ':':
            return false;
        }
    }

    return true;
}

static String getFinalTestURL(const String& testURL)
{
    const size_t hashSeparatorPos = testURL.find("'");
    if (hashSeparatorPos != notFound)
        return getFinalTestURL(testURL.left(hashSeparatorPos));

    // Convert the path into a full file URL if it does not look
    // like an HTTP/S URL (doesn't start with http:// or https://).
    if (!testURL.startsWith("http://") && !testURL.startsWith("https://")) {
        char* cFilePath = ecore_file_realpath(testURL.utf8().data());
        const String filePath = String::fromUTF8(cFilePath);
        free(cFilePath);

        if (ecore_file_exists(filePath.utf8().data()))
            return String("file://") + filePath;
    }

    return testURL;
}

static String getExpectedPixelHash(const String& testURL)
{
    const size_t hashSeparatorPos = testURL.find("'");
    return (hashSeparatorPos != notFound) ? testURL.substring(hashSeparatorPos + 1) : String();
}

static void createLayoutTestController(const String& testURL, const String& expectedPixelHash)
{
    gLayoutTestController =
        LayoutTestController::create(std::string(testURL.utf8().data()),
                                     std::string(expectedPixelHash.utf8().data()));

    topLoadingFrame = 0;
    done = false;

    gLayoutTestController->setIconDatabaseEnabled(false);

    if (shouldLogFrameLoadDelegates(testURL))
        gLayoutTestController->setDumpFrameLoadCallbacks(true);

    gLayoutTestController->setDeveloperExtrasEnabled(true);

    if (shouldDumpAsText(testURL)) {
        gLayoutTestController->setDumpAsText(true);
        gLayoutTestController->setGeneratePixelResults(false);
    }
}

static void runTest(const char* cTestPathOrURL)
{
    const String testPathOrURL = String::fromUTF8(cTestPathOrURL);
    ASSERT(!testPathOrURL.isEmpty());

    const String testURL = getFinalTestURL(testPathOrURL);
    const String expectedPixelHash = getExpectedPixelHash(testPathOrURL);

    browser->resetDefaultsToConsistentValues();
    createLayoutTestController(testURL, expectedPixelHash);

    WorkQueue::shared()->clear();
    WorkQueue::shared()->setFrozen(false);

    const bool isSVGW3CTest = testURL.contains("svg/W3C-SVG-1.1");
    const int width = isSVGW3CTest ? 480 : LayoutTestController::maxViewWidth;
    const int height = isSVGW3CTest ? 360 : LayoutTestController::maxViewHeight;
    evas_object_resize(browser->mainView(), width, height);

    if (prevTestBFItem)
        ewk_history_item_free(prevTestBFItem);
    const Ewk_History* history = ewk_view_history_get(browser->mainView());
    prevTestBFItem = ewk_history_history_item_current_get(history);

    evas_object_focus_set(browser->mainView(), EINA_TRUE);
    ewk_view_uri_set(browser->mainView(), testURL.utf8().data());

    ecore_main_loop_begin();

    gLayoutTestController->closeWebInspector();
    gLayoutTestController->setDeveloperExtrasEnabled(false);

    browser->clearExtraViews();

    // FIXME: Move to DRTChrome::resetDefaultsToConsistentValues() after bug 85209 lands.
    WebCoreTestSupport::resetInternalsObject(DumpRenderTreeSupportEfl::globalContextRefForFrame(browser->mainFrame()));

    ewk_view_uri_set(browser->mainView(), "about:blank");

    gLayoutTestController.clear();
    sendPixelResultsEOF();
}

static void runTestingServerLoop()
{
    char filename[PATH_MAX];

    while (fgets(filename, sizeof(filename), stdin)) {
        char* newLine = strrchr(filename, '\n');
        if (newLine)
            *newLine = '\0';

        if (filename[0] != '\0')
            runTest(filename);
    }
}

static void adjustOutputTypeByMimeType(const Evas_Object* frame)
{
    const String responseMimeType(DumpRenderTreeSupportEfl::responseMimeType(frame));
    if (responseMimeType == "text/plain") {
        gLayoutTestController->setDumpAsText(true);
        gLayoutTestController->setGeneratePixelResults(false);
    }
}

static void dumpFrameContentsAsText(Evas_Object* frame)
{
    String result;
    if (gLayoutTestController->dumpAsText())
        result = dumpFramesAsText(frame);
    else
        result = DumpRenderTreeSupportEfl::renderTreeDump(frame);

    printf("%s", result.utf8().data());
}

static bool shouldDumpFrameScrollPosition()
{
    return gLayoutTestController->dumpAsText() && !gLayoutTestController->dumpDOMAsWebArchive() && !gLayoutTestController->dumpSourceAsWebArchive();
}

static bool shouldDumpPixelsAndCompareWithExpected()
{
    return dumpPixels && gLayoutTestController->generatePixelResults() && !gLayoutTestController->dumpDOMAsWebArchive() && !gLayoutTestController->dumpSourceAsWebArchive();
}

static bool shouldDumpBackForwardList()
{
    return gLayoutTestController->dumpBackForwardList();
}

static bool initEfl()
{
    if (!ecore_evas_init())
        return false;
    if (!ecore_file_init()) {
        ecore_evas_shutdown();
        return false;
    }
    if (!edje_init()) {
        ecore_file_shutdown();
        ecore_evas_shutdown();
        return false;
    }
    if (!ewk_init()) {
        edje_shutdown();
        ecore_file_shutdown();
        ecore_evas_shutdown();
        return false;
    }

    return true;
}

static void shutdownEfl()
{
    ewk_shutdown();
    edje_shutdown();
    ecore_file_shutdown();
    ecore_evas_shutdown();
}

void displayWebView()
{
    notImplemented();
}

void dump()
{
    Evas_Object* frame = browser->mainFrame();

    invalidateAnyPreviousWaitToDumpWatchdog();

    if (dumpTree) {
        adjustOutputTypeByMimeType(frame);
        dumpFrameContentsAsText(frame);

        if (shouldDumpFrameScrollPosition())
            dumpFrameScrollPosition(frame);

        if (shouldDumpBackForwardList())
            dumpBackForwardListForWebViews();

        if (printSeparators) {
            puts("#EOF");
            fputs("#EOF\n", stderr);
            fflush(stdout);
            fflush(stderr);
        }
    }

    if (shouldDumpPixelsAndCompareWithExpected())
        dumpWebViewAsPixelsAndCompareWithExpected(gLayoutTestController->expectedPixelHash());

    done = true;
    ecore_main_loop_quit();
}

static Ecore_Evas* initEcoreEvas()
{
    Ecore_Evas* ecoreEvas = useX11Window ? ecore_evas_new(0, 0, 0, 800, 600, 0) : ecore_evas_buffer_new(800, 600);
    if (!ecoreEvas) {
        shutdownEfl();
        exit(EXIT_FAILURE);
    }

    ecore_evas_title_set(ecoreEvas, "EFL DumpRenderTree");
    ecore_evas_callback_resize_set(ecoreEvas, onEcoreEvasResize);
    ecore_evas_callback_delete_request_set(ecoreEvas, onCloseWindow);
    ecore_evas_show(ecoreEvas);

    return ecoreEvas;
}

int main(int argc, char** argv)
{
    if (!parseCommandLineOptions(argc, argv))
        return EXIT_FAILURE;

    if (!initEfl())
        return EXIT_FAILURE;

    OwnPtr<Ecore_Evas> ecoreEvas = adoptPtr(initEcoreEvas());
    browser = DumpRenderTreeChrome::create(ecore_evas_get(ecoreEvas.get()));
    addFontsToEnvironment();

    if (useLongRunningServerMode(argc, argv)) {
        printSeparators = true;
        runTestingServerLoop();
    } else {
        printSeparators = (optind < argc - 1 || (dumpPixels && dumpTree));
        for (int i = optind; i != argc; ++i)
            runTest(argv[i]);
    }

    ecoreEvas.clear();

    shutdownEfl();
    return EXIT_SUCCESS;
}
