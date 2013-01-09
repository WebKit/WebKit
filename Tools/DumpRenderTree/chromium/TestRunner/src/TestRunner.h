/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Pawel Hajdan (phajdan.jr@chromium.org)
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

#ifndef TestRunner_h
#define TestRunner_h

#include "CppBoundClass.h"
#include "WebDeliveredIntentClient.h"
#include "WebTestRunner.h"
#include "platform/WebArrayBufferView.h"
#include "platform/WebURL.h"

namespace WebKit {
class WebView;
}

namespace WebTestRunner {

class WebTestDelegate;

class TestRunner : public CppBoundClass, public WebTestRunner {
public:
    TestRunner();
    virtual ~TestRunner();

    // FIXME: once DRTTestRunner is moved entirely to this class, change this
    // method to take a TestDelegate* instead.
    void setDelegate(WebTestDelegate* delegate) { m_delegate = delegate; }
    void setWebView(WebKit::WebView* webView) { m_webView = webView; }

    void reset();

    // WebTestRunner implementation.
    virtual void setTestIsRunning(bool) OVERRIDE;
    virtual bool shouldDumpEditingCallbacks() const OVERRIDE;
    virtual bool shouldDumpAsText() const OVERRIDE;
    virtual void setShouldDumpAsText(bool) OVERRIDE;
    virtual bool shouldGeneratePixelResults() const OVERRIDE;
    virtual void setShouldGeneratePixelResults(bool) OVERRIDE;
    virtual bool shouldDumpChildFrameScrollPositions() const OVERRIDE;
    virtual bool shouldDumpChildFramesAsText() const OVERRIDE;
    virtual bool shouldDumpAsAudio() const OVERRIDE;
    virtual const WebKit::WebArrayBufferView* audioData() const OVERRIDE;
    virtual bool shouldDumpFrameLoadCallbacks() const OVERRIDE;
    virtual void setShouldDumpFrameLoadCallbacks(bool) OVERRIDE;
    virtual bool shouldDumpUserGestureInFrameLoadCallbacks() const OVERRIDE;
    virtual bool stopProvisionalFrameLoads() const OVERRIDE;
    virtual bool shouldDumpTitleChanges() const OVERRIDE;
    virtual bool shouldDumpCreateView() const OVERRIDE;
    virtual bool canOpenWindows() const OVERRIDE;

protected:
    // FIXME: make these private once the move from DRTTestRunner to TestRunner
    // is complete.
    bool cppVariantToBool(const CppVariant&);
    int32_t cppVariantToInt32(const CppVariant&);
    WebKit::WebString cppVariantToWebString(const CppVariant&);

    void printErrorMessage(const std::string&);

private:
    ///////////////////////////////////////////////////////////////////////////
    // Methods implemented entirely in terms of chromium's public WebKit API

    // Method that controls whether pressing Tab key cycles through page elements
    // or inserts a '\t' char in text area
    void setTabKeyCyclesThroughElements(const CppArgumentList&, CppVariant*);

    // Executes an internal command (superset of document.execCommand() commands).
    void execCommand(const CppArgumentList&, CppVariant*);

    // Checks if an internal command is currently available.
    void isCommandEnabled(const CppArgumentList&, CppVariant*);

    void pauseAnimationAtTimeOnElementWithId(const CppArgumentList&, CppVariant*);
    void pauseTransitionAtTimeOnElementWithId(const CppArgumentList&, CppVariant*);
    void elementDoesAutoCompleteForElementWithId(const CppArgumentList&, CppVariant*);
    void numberOfActiveAnimations(const CppArgumentList&, CppVariant*);
    void callShouldCloseOnWebView(const CppArgumentList&, CppVariant*);
    void setDomainRelaxationForbiddenForURLScheme(const CppArgumentList&, CppVariant*);
    void evaluateScriptInIsolatedWorldAndReturnValue(const CppArgumentList&, CppVariant*);
    void evaluateScriptInIsolatedWorld(const CppArgumentList&, CppVariant*);
    void setIsolatedWorldSecurityOrigin(const CppArgumentList&, CppVariant*);
    void setIsolatedWorldContentSecurityPolicy(const CppArgumentList&, CppVariant*);

    // Allows layout tests to manage origins' whitelisting.
    void addOriginAccessWhitelistEntry(const CppArgumentList&, CppVariant*);
    void removeOriginAccessWhitelistEntry(const CppArgumentList&, CppVariant*);

    // Returns true if the current page box has custom page size style for
    // printing.
    void hasCustomPageSizeStyle(const CppArgumentList&, CppVariant*);

    // Forces the selection colors for testing under Linux.
    void forceRedSelectionColors(const CppArgumentList&, CppVariant*);

    // Adds a user script or user style sheet to be injected into new documents.
    void addUserScript(const CppArgumentList&, CppVariant*);
    void addUserStyleSheet(const CppArgumentList&, CppVariant*);

    void startSpeechInput(const CppArgumentList&, CppVariant*);

    void markerTextForListItem(const CppArgumentList&, CppVariant*);
    void findString(const CppArgumentList&, CppVariant*);

    // Expects the first argument to be an input element and the second argument to be a boolean.
    // Forwards the setAutofilled() call to the element.
    void setAutofilled(const CppArgumentList&, CppVariant*);

    // Expects the first argument to be an input element and the second argument to be a string value.
    // Forwards the setValueForUser() call to the element.
    void setValueForUser(const CppArgumentList&, CppVariant*);

    void enableFixedLayoutMode(const CppArgumentList&, CppVariant*);
    void setFixedLayoutSize(const CppArgumentList&, CppVariant*);

    void selectionAsMarkup(const CppArgumentList&, CppVariant*);

    // Enables or disables subpixel positioning (i.e. fractional X positions for
    // glyphs) in text rendering on Linux. Since this method changes global
    // settings, tests that call it must use their own custom font family for
    // all text that they render. If not, an already-cached style will be used,
    // resulting in the changed setting being ignored.
    void setTextSubpixelPositioning(const CppArgumentList&, CppVariant*);

    // Switch the visibility of the page.
    void setPageVisibility(const CppArgumentList&, CppVariant*);
    void resetPageVisibility(const CppArgumentList&, CppVariant*);

    // Changes the direction of the focused element.
    void setTextDirection(const CppArgumentList&, CppVariant*);

    // Retrieves the text surrounding a position in a text node.
    // Expects the first argument to be a text node, the second and third to be
    // point coordinates relative to the node and the fourth the maximum text
    // length to retrieve.
    void textSurroundingNode(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Methods modifying WebPreferences.

    // Passes through to WebPreferences which allows the user to have a custom
    // style sheet.
    void setUserStyleSheetEnabled(const CppArgumentList&, CppVariant*);
    void setUserStyleSheetLocation(const CppArgumentList&, CppVariant*);

    // Passes this preference through to WebSettings.
    void setAuthorAndUserStylesEnabled(const CppArgumentList&, CppVariant*);

    // Set the WebPreference that controls webkit's popup blocking.
    void setPopupBlockingEnabled(const CppArgumentList&, CppVariant*);

    void setJavaScriptCanAccessClipboard(const CppArgumentList&, CppVariant*);
    void setXSSAuditorEnabled(const CppArgumentList&, CppVariant*);
    void setAllowUniversalAccessFromFileURLs(const CppArgumentList&, CppVariant*);
    void setAllowFileAccessFromFileURLs(const CppArgumentList&, CppVariant*);
    void overridePreference(const CppArgumentList&, CppVariant*);

    // Enable or disable plugins.
    void setPluginsEnabled(const CppArgumentList&, CppVariant*);

    // Changes asynchronous spellchecking flag on the settings.
    void setAsynchronousSpellCheckingEnabled(const CppArgumentList&, CppVariant*);

    void setMinimumTimerInterval(const CppArgumentList&, CppVariant*);
    void setTouchDragDropEnabled(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Methods that modify the state of TestRunner

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for each editing command. It takes no arguments, and
    // ignores any that may be present.
    void dumpEditingCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump pages as
    // plain text, rather than as a text representation of the renderer's state.
    // It takes an optional argument, whether to dump pixels results or not.
    void dumpAsText(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print out the
    // scroll offsets of the child frames. It ignores all.
    void dumpChildFrameScrollPositions(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to recursively
    // dump all frames as plain text if the dumpAsText flag is set.
    // It takes no arguments, and ignores any that may be present.
    void dumpChildFramesAsText(const CppArgumentList&, CppVariant*);

    // Deals with Web Audio WAV file data.
    void setAudioData(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print a line of
    // descriptive text for each frame load callback. It takes no arguments, and
    // ignores any that may be present.
    void dumpFrameLoadCallbacks(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to print a line of
    // user gesture status text for some frame load callbacks. It takes no
    // arguments, and ignores any that may be present.
    void dumpUserGestureInFrameLoadCallbacks(const CppArgumentList&, CppVariant*);

    // If true, causes provisional frame loads to be stopped for the remainder of
    // the test.
    void setStopProvisionalFrameLoads(const CppArgumentList&, CppVariant*);

    void dumpTitleChanges(const CppArgumentList&, CppVariant*);

    // This function sets a flag that tells the test_shell to dump all calls to
    // WebViewClient::createView().
    // It takes no arguments, and ignores any that may be present.
    void dumpCreateView(const CppArgumentList&, CppVariant*);

    void setCanOpenWindows(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Methods interacting with the WebTestProxy

    // Expects one string argument for sending successful result, zero
    // arguments for sending a failure result.
    void sendWebIntentResponse(const CppArgumentList&, CppVariant*);

    // Cause the web intent to be delivered to this context.
    void deliverWebIntent(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Properties
    void workerThreadCount(CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Fallback and stub methods

    // The fallback method is called when a nonexistent method is called on
    // the layout test controller object.
    // It is usefull to catch typos in the JavaScript code (a few layout tests
    // do have typos in them) and it allows the script to continue running in
    // that case (as the Mac does).
    void fallbackMethod(const CppArgumentList&, CppVariant*);

    // Stub for not implemented methods.
    void notImplemented(const CppArgumentList&, CppVariant*);

    ///////////////////////////////////////////////////////////////////////////
    // Internal helpers
    bool pauseAnimationAtTimeOnElementWithId(const WebKit::WebString& animationName, double time, const WebKit::WebString& elementId);
    bool pauseTransitionAtTimeOnElementWithId(const WebKit::WebString& propertyName, double time, const WebKit::WebString& elementId);
    bool elementDoesAutoCompleteForElementWithId(const WebKit::WebString&);
    int numberOfActiveAnimations();

    bool m_testIsRunning;

    WebKit::WebURL m_userStyleSheetLocation;

    // globalFlag is used by a number of layout tests in http/tests/security/dataURL.
    CppVariant m_globalFlag;

    // Bound variable to return the name of this platform (chromium).
    CppVariant m_platformName;

    // If true, the test_shell will write a descriptive line for each editing
    // command.
    bool m_dumpEditingCallbacks;

    // If true, the test_shell will generate pixel results in dumpAsText mode
    bool m_generatePixelResults;

    // If true, the test_shell will produce a plain text dump rather than a
    // text representation of the renderer.
    bool m_dumpAsText;

    // If true and if dump_as_text_ is true, the test_shell will recursively
    // dump all frames as plain text.
    bool m_dumpChildFramesAsText;

    // If true, the test_shell will print out the child frame scroll offsets as
    // well.
    bool m_dumpChildFrameScrollPositions;

    // If true, the test_shell will output a base64 encoded WAVE file.
    bool m_dumpAsAudio;

    // If true, the test_shell will output a descriptive line for each frame
    // load callback.
    bool m_dumpFrameLoadCallbacks;

    // If true, the test_shell will output a line of the user gesture status
    // text for some frame load callbacks.
    bool m_dumpUserGestureInFrameLoadCallbacks;

    // If true, stops provisional frame loads during the
    // DidStartProvisionalLoadForFrame callback.
    bool m_stopProvisionalFrameLoads;

    // If true, output a message when the page title is changed.
    bool m_dumpTitleChanges;

    // If true, output a descriptive line each time WebViewClient::createView
    // is invoked.
    bool m_dumpCreateView;

    // If true, new windows can be opened via javascript or by plugins. By
    // default, set to false and can be toggled to true using
    // setCanOpenWindows().
    bool m_canOpenWindows;

    // WAV audio data is stored here.
    WebKit::WebArrayBufferView m_audioData;

    WebTestDelegate* m_delegate;
    WebKit::WebView* m_webView;

    // Mock object for testing delivering web intents.
    OwnPtr<WebKit::WebDeliveredIntentClient> m_intentClient;
};

}

#endif // TestRunner_h
