/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DumpRenderTree.h"

#include "MockPlatform.h"
#include "TestShell.h"
#include "webkit/support/webkit_support.h"
#include <public/WebCompositorSupport.h>
#include <v8/include/v8-testing.h>
#include <v8/include/v8.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

using namespace std;

static const char optionComplexText[] = "--complex-text";
static const char optionDumpPixels[] = "--pixel-tests";
static const char optionDumpPixelsShortForm[] = "-p";
static const char optionNotree[] = "--notree";
static const char optionThreaded[] = "--threaded";
static const char optionDebugRenderTree[] = "--debug-render-tree";
static const char optionDebugLayerTree[] = "--debug-layer-tree";

static const char optionAllowExternalPages[] = "--allow-external-pages";
static const char optionStartupDialog[] = "--testshell-startup-dialog";
static const char optionCheckLayoutTestSystemDeps[] = "--check-layout-test-sys-deps";

static const char optionHardwareAcceleratedGL[] = "--enable-hardware-gpu";
static const char optionEnableSoftwareCompositing[] = "--enable-software-compositing";
static const char optionEnableThreadedCompositing[] = "--enable-threaded-compositing";
static const char optionForceCompositingMode[] = "--force-compositing-mode";
static const char optionEnableAccelerated2DCanvas[] = "--enable-accelerated-2d-canvas";
static const char optionEnableDeferred2DCanvas[] = "--enable-deferred-2d-canvas";
static const char optionEnableAcceleratedPainting[] = "--enable-accelerated-painting";
static const char optionEnableAcceleratedCompositingForVideo[] = "--enable-accelerated-video";
static const char optionEnableAcceleratedFixedPosition[] = "--enable-accelerated-fixed-position";
static const char optionEnableAcceleratedOverflowScroll[] = "--enable-accelerated-overflow-scroll";
static const char optionUseGraphicsContext3DImplementation[] = "--use-graphics-context-3d-implementation=";
static const char optionEnablePerTilePainting[] = "--enable-per-tile-painting";
static const char optionEnableDeferredImageDecoding[] = "--enable-deferred-image-decoding";
static const char optionDisableThreadedHTMLParser[] = "--disable-threaded-html-parser";

static const char optionStressOpt[] = "--stress-opt";
static const char optionStressDeopt[] = "--stress-deopt";
static const char optionJavaScriptFlags[] = "--js-flags=";
static const char optionEncodeBinary[] = "--encode-binary";
static const char optionNoTimeout[] = "--no-timeout";
static const char optionWebCoreLogChannels[] = "--webcore-log-channels=";

class WebKitSupportTestEnvironment {
public:
    WebKitSupportTestEnvironment()
    {
        m_mockPlatform = MockPlatform::create();
        webkit_support::SetUpTestEnvironment(m_mockPlatform.get());
    }
    ~WebKitSupportTestEnvironment()
    {
        webkit_support::TearDownTestEnvironment();
    }

    MockPlatform* mockPlatform() { return m_mockPlatform.get(); }

private:
    OwnPtr<MockPlatform> m_mockPlatform;
};

static void runTest(TestShell& shell, TestParams& params, const string& inputLine, const bool forceDumpPixels)
{
    int oldTimeoutMsec = shell.layoutTestTimeout();
    TestCommand command = parseInputLine(inputLine);
    params.testUrl = webkit_support::CreateURLForPathOrURL(command.pathOrURL);
    params.pixelHash = command.shouldDumpPixels;
    webkit_support::SetCurrentDirectoryForFileURL(params.testUrl);
    v8::V8::SetFlagsFromString(shell.javaScriptFlags().c_str(), shell.javaScriptFlags().length());
    if (shell.stressOpt() || shell.stressDeopt()) {
      if (shell.stressOpt())
        v8::Testing::SetStressRunType(v8::Testing::kStressTypeOpt);
      else
        v8::Testing::SetStressRunType(v8::Testing::kStressTypeDeopt);
      for (int i = 0; i < v8::Testing::GetStressRuns(); i++) {
          v8::Testing::PrepareStressRun(i);
          bool isLastLoad = (i == (v8::Testing::GetStressRuns() - 1));
          shell.setDumpWhenFinished(isLastLoad);
          shell.resetTestController();
          shell.runFileTest(params, command.shouldDumpPixels || forceDumpPixels);
      }
    } else {
      shell.resetTestController();
      shell.runFileTest(params, command.shouldDumpPixels || forceDumpPixels);
    }
    shell.setLayoutTestTimeout(oldTimeoutMsec);
}

int main(int argc, char* argv[])
{
    WebKitSupportTestEnvironment testEnvironment;
    platformInit(&argc, &argv);

    TestParams params;
    Vector<string> tests;
    bool serverMode = false;
    bool dumpAllPixels = false;
    bool allowExternalPages = false;
    bool startupDialog = false;
    bool acceleratedCompositingForVideoEnabled = false;
    bool acceleratedCompositingForFixedPositionEnabled = false;
    bool acceleratedCompositingForOverflowScrollEnabled = false;
    bool softwareCompositingEnabled = false;
    bool threadedCompositingEnabled = false;
    bool forceCompositingMode = false;
    bool threadedHTMLParser = true;
    bool accelerated2DCanvasEnabled = false;
    bool deferred2DCanvasEnabled = false;
    bool acceleratedPaintingEnabled = false;
    bool perTilePaintingEnabled = false;
    bool deferredImageDecodingEnabled = false;
    bool stressOpt = false;
    bool stressDeopt = false;
    bool hardwareAcceleratedGL = false;
    string javaScriptFlags;
    bool encodeBinary = false;
    bool noTimeout = false;
    bool acceleratedAnimationEnabled = false;
    for (int i = 1; i < argc; ++i) {
        string argument(argv[i]);
        if (argument == "-")
            serverMode = true;
        else if (argument == optionDumpPixels || argument == optionDumpPixelsShortForm)
            dumpAllPixels = true;
        else if (argument == optionNotree)
            params.dumpTree = false;
        else if (argument == optionDebugRenderTree)
            params.debugRenderTree = true;
        else if (argument == optionDebugLayerTree)
            params.debugLayerTree = true;
        else if (argument == optionAllowExternalPages)
            allowExternalPages = true;
        else if (argument == optionStartupDialog)
            startupDialog = true;
        else if (argument == optionCheckLayoutTestSystemDeps)
            return checkLayoutTestSystemDependencies() ? EXIT_SUCCESS : EXIT_FAILURE;
        else if (argument == optionHardwareAcceleratedGL)
            hardwareAcceleratedGL = true;
        else if (argument == optionEnableAcceleratedCompositingForVideo)
            acceleratedCompositingForVideoEnabled = true;
        else if (argument == optionEnableAcceleratedFixedPosition)
            acceleratedCompositingForFixedPositionEnabled = true;
        else if (argument == optionEnableAcceleratedOverflowScroll)
            acceleratedCompositingForOverflowScrollEnabled = true;
        else if (argument == optionEnableSoftwareCompositing)
            softwareCompositingEnabled = true;
        else if (argument == optionEnableThreadedCompositing)
            threadedCompositingEnabled = true;
        else if (argument == optionForceCompositingMode)
            forceCompositingMode = true;
        else if (argument == optionDisableThreadedHTMLParser)
            threadedHTMLParser = false;
        else if (argument == optionEnableAccelerated2DCanvas)
            accelerated2DCanvasEnabled = true;
        else if (argument == optionEnableDeferred2DCanvas)
            deferred2DCanvasEnabled = true;
        else if (argument == optionEnableAcceleratedPainting)
            acceleratedPaintingEnabled = true;
        else if (!argument.find(optionUseGraphicsContext3DImplementation)) {
            string implementation = argument.substr(strlen(optionUseGraphicsContext3DImplementation));
            if (!implementation.compare("IN_PROCESS")) 
              webkit_support::SetGraphicsContext3DImplementation(webkit_support::IN_PROCESS);
            else if (!implementation.compare("IN_PROCESS_COMMAND_BUFFER")) 
              webkit_support::SetGraphicsContext3DImplementation(webkit_support::IN_PROCESS_COMMAND_BUFFER);
            else 
              fprintf(stderr, "Unknown GraphicContext3D implementation %s\n", implementation.c_str());
        } else if (argument == optionEnablePerTilePainting)
            perTilePaintingEnabled = true;
        else if (argument == optionEnableDeferredImageDecoding)
            deferredImageDecodingEnabled = true;
        else if (argument == optionStressOpt)
            stressOpt = true;
        else if (argument == optionStressDeopt)
            stressDeopt = true;
        else if (!argument.find(optionJavaScriptFlags))
            javaScriptFlags = argument.substr(strlen(optionJavaScriptFlags));
        else if (argument == optionEncodeBinary)
            encodeBinary = true;
        else if (argument == optionNoTimeout)
            noTimeout = true;
        else if (!argument.find(optionWebCoreLogChannels)) {
            string channels = argument.substr(strlen(optionWebCoreLogChannels));
            webkit_support::EnableWebCoreLogChannels(channels);
        } else if (argument.size() && argument[0] == '-')
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
        else
            tests.append(argument);
    }
    if (stressOpt && stressDeopt) {
        fprintf(stderr, "--stress-opt and --stress-deopt are mutually exclusive.\n");
        return EXIT_FAILURE;
    }

    webkit_support::SetUpGLBindings(hardwareAcceleratedGL ? webkit_support::GL_BINDING_DEFAULT : webkit_support::GL_BINDING_SOFTWARE_RENDERER);

    if (startupDialog)
        openStartupDialog();

    { // Explicit scope for the TestShell instance.
        TestShell shell;
        shell.setAllowExternalPages(allowExternalPages);
        shell.setAcceleratedCompositingForVideoEnabled(acceleratedCompositingForVideoEnabled);
        shell.setAcceleratedCompositingForFixedPositionEnabled(acceleratedCompositingForFixedPositionEnabled);
        shell.setAcceleratedCompositingForOverflowScrollEnabled(acceleratedCompositingForOverflowScrollEnabled);
        shell.setSoftwareCompositingEnabled(softwareCompositingEnabled);
        shell.setThreadedCompositingEnabled(threadedCompositingEnabled);
        shell.setForceCompositingMode(forceCompositingMode);
        shell.setThreadedHTMLParser(threadedHTMLParser);
        shell.setAccelerated2dCanvasEnabled(accelerated2DCanvasEnabled);
        shell.setDeferred2dCanvasEnabled(deferred2DCanvasEnabled);
        shell.setAcceleratedPaintingEnabled(acceleratedPaintingEnabled);
        shell.setAcceleratedAnimationEnabled(acceleratedAnimationEnabled);
        shell.setPerTilePaintingEnabled(perTilePaintingEnabled);
        shell.setDeferredImageDecodingEnabled(deferredImageDecodingEnabled);
        shell.setJavaScriptFlags(javaScriptFlags);
        shell.setStressOpt(stressOpt);
        shell.setStressDeopt(stressDeopt);
        shell.setEncodeBinary(encodeBinary);
        if (noTimeout) {
            // 0x20000000ms is big enough for the purpose to avoid timeout in debugging.
            shell.setLayoutTestTimeout(0x20000000);
        }
        shell.initialize(testEnvironment.mockPlatform());
        if (serverMode && !tests.size()) {
#if OS(ANDROID)
            // Send a signal to host to indicate DRT is ready to process commands.
            puts("#READY");
            fflush(stdout);
#endif
            params.printSeparators = true;
            char testString[2048]; // 2048 is the same as the sizes of other platforms.
            while (fgets(testString, sizeof(testString), stdin)) {
                char* newLinePosition = strchr(testString, '\n');
                if (newLinePosition)
                    *newLinePosition = '\0';
                if (testString[0] == '\0')
                    continue;
                // Explicitly quit on platforms where EOF is not reliable.
                if (!strcmp(testString, "QUIT"))
                    break;
                runTest(shell, params, testString, dumpAllPixels);
            }
        } else if (!tests.size())
            puts("#EOF");
        else {
            params.printSeparators = tests.size() > 1;
            for (unsigned i = 0; i < tests.size(); i++)
                runTest(shell, params, tests[i], dumpAllPixels);
        }

        shell.callJSGC();
        shell.callJSGC();

        // When we finish the last test, cleanup the DRTTestRunner.
        // It may have references to not-yet-cleaned up windows. By cleaning up
        // here we help purify reports.
        shell.resetTestController();
    }

    return EXIT_SUCCESS;
}
