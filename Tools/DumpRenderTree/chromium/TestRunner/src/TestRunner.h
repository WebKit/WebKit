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
#include "platform/WebURL.h"

namespace WebKit {
class WebView;
}

namespace WebTestRunner {

class WebTestDelegate;

class TestRunner : public CppBoundClass {
public:
    TestRunner();

    // FIXME: once DRTTestRunner is moved entirely to this class, change this
    // method to take a TestDelegate* instead.
    void setDelegate(WebTestDelegate* delegate) { m_delegate = delegate; }
    void setWebView(WebKit::WebView* webView) { m_webView = webView; }

    void reset();

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

    void loseCompositorContext(const CppArgumentList& args, CppVariant* result);

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

    WebKit::WebURL m_userStyleSheetLocation;

    // globalFlag is used by a number of layout tests in http/tests/security/dataURL.
    CppVariant m_globalFlag;

    // Bound variable to return the name of this platform (chromium).
    CppVariant m_platformName;

    WebTestDelegate* m_delegate;
    WebKit::WebView* m_webView;
};

}

#endif // TestRunner_h
