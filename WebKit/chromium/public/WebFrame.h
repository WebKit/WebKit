/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebFrame_h
#define WebFrame_h

#include "WebCanvas.h"
#include "WebURL.h"

struct NPObject;

#if WEBKIT_USING_V8
namespace v8 {
class Context;
template <class T> class Local;
}
#endif

namespace WebKit {

class WebAnimationController;
class WebData;
class WebDataSource;
class WebDocument;
class WebElement;
class WebFormElement;
class WebHistoryItem;
class WebInputElement;
class WebPasswordAutocompleteListener;
class WebRange;
class WebSecurityOrigin;
class WebString;
class WebURL;
class WebURLRequest;
class WebView;
struct WebConsoleMessage;
struct WebFindOptions;
struct WebRect;
struct WebScriptSource;
struct WebSize;
template <typename T> class WebVector;

class WebFrame {
public:
    // Returns the number of live WebFrame objects, used for leak checking.
    WEBKIT_API static int instanceCount();

    // The two functions below retrieve the WebFrame instances relating the
    // currently executing JavaScript.  Since JavaScript can make function
    // calls across frames, though, we need to be more precise.
    //
    // For example, imagine that a JS function in frame A calls a function
    // in frame B, which calls native code, which wants to know what the
    // 'active' frame is.
    //
    // The 'entered context' is the context where execution first entered
    // the script engine; the context that is at the bottom of the JS
    // function stack.  frameForEnteredContext() would return frame A in
    // our example.
    //
    // The 'current context' is the context the JS engine is currently
    // inside of; the context that is at the top of the JS function stack.
    // frameForCurrentContext() would return frame B in our example.
    WEBKIT_API static WebFrame* frameForEnteredContext();
    WEBKIT_API static WebFrame* frameForCurrentContext();

    // Returns the frame inside a given frame or iframe element. Returns 0 if
    // the given element is not a frame, iframe or if the frame is empty.
    WEBKIT_API static WebFrame* fromFrameOwnerElement(const WebElement&);


    // Basic properties ---------------------------------------------------

    // The name of this frame.
    virtual WebString name() const = 0;
    virtual void clearName() = 0;

    // The url of the document loaded in this frame.  This is equivalent to
    // dataSource()->request().url().
    virtual WebURL url() const = 0;

    // The url of the favicon (if any) specified by the document loaded in
    // this frame.
    virtual WebURL favIconURL() const = 0;

    // The url of the OpenSearch Desription Document (if any) specified by
    // the document loaded in this frame.
    virtual WebURL openSearchDescriptionURL() const = 0;

    // Return the frame's encoding.
    virtual WebString encoding() const = 0;


    // Geometry -----------------------------------------------------------

    // NOTE: These routines do not force page layout so their results may
    // not be accurate if the page layout is out-of-date.

    // The scroll offset from the top-left corner of the frame in pixels.
    virtual WebSize scrollOffset() const = 0;

    // The size of the contents area.
    virtual WebSize contentsSize() const = 0;

    // Returns the minimum preferred width of the content contained in the
    // current document.
    virtual int contentsPreferredWidth() const = 0;

    // Returns the scroll height of the document element. This is
    // equivalent to the DOM property of the same name, and is the minimum
    // height required to display the document without scrollbars.
    virtual int documentElementScrollHeight() const = 0;

    // Returns true if the contents (minus scrollbars) has non-zero area.
    virtual bool hasVisibleContent() const = 0;


    // Hierarchy ----------------------------------------------------------

    // Returns the containing view.
    virtual WebView* view() const = 0;

    // Returns the frame that opened this frame or 0 if there is none.
    virtual WebFrame* opener() const = 0;

    // Returns the parent frame or 0 if this is a top-most frame.
    virtual WebFrame* parent() const = 0;

    // Returns the top-most frame in the hierarchy containing this frame.
    virtual WebFrame* top() const = 0;

    // Returns the first/last child frame.
    virtual WebFrame* firstChild() const = 0;
    virtual WebFrame* lastChild() const = 0;

    // Returns the next/previous sibling frame.
    virtual WebFrame* nextSibling() const = 0;
    virtual WebFrame* previousSibling() const = 0;

    // Returns the next/previous frame in "frame traversal order"
    // optionally wrapping around.
    virtual WebFrame* traverseNext(bool wrap) const = 0;
    virtual WebFrame* traversePrevious(bool wrap) const = 0;

    // Returns the child frame identified by the given name.
    virtual WebFrame* findChildByName(const WebString& name) const = 0;

    // Returns the child frame identified by the given xpath expression.
    virtual WebFrame* findChildByExpression(const WebString& xpath) const = 0;


    // Content ------------------------------------------------------------

    virtual WebDocument document() const = 0;

    virtual void forms(WebVector<WebFormElement>&) const = 0;

    virtual WebAnimationController* animationController() = 0;


    // Scripting ----------------------------------------------------------

    // Returns the security origin of the current document.
    virtual WebSecurityOrigin securityOrigin() const = 0;

    // This grants the currently loaded document access to all security
    // origins (including file URLs).  Use with care.  The access is
    // revoked when a new document is loaded into this frame.
    virtual void grantUniversalAccess() = 0;

    // Returns a NPObject corresponding to this frame's DOMWindow.
    virtual NPObject* windowObject() const = 0;

    // Binds a NPObject as a property of this frame's DOMWindow.
    virtual void bindToWindowObject(const WebString& name, NPObject*) = 0;

    // Executes script in the context of the current page.
    virtual void executeScript(const WebScriptSource&) = 0;

    // Executes JavaScript in a new world associated with the web frame.
    // The script gets its own global scope and its own prototypes for
    // intrinsic JavaScript objects (String, Array, and so-on). It also
    // gets its own wrappers for all DOM nodes and DOM constructors.
    // extensionGroup is an embedder-provided specifier that controls which
    // v8 extensions are loaded into the new context - see
    // WebKit::registerExtension for the corresponding specifier.
    virtual void executeScriptInIsolatedWorld(
        int worldId, const WebScriptSource* sources, unsigned numSources,
        int extensionGroup) = 0;

    // Logs to the console associated with this frame.
    virtual void addMessageToConsole(const WebConsoleMessage&) = 0;

    // Calls window.gc() if it is defined.
    virtual void collectGarbage() = 0;

#if WEBKIT_USING_V8
    // Returns the V8 context for this frame, or an empty handle if there
    // is none.
    virtual v8::Local<v8::Context> mainWorldScriptContext() const = 0;
#endif


    // Styling -------------------------------------------------------------

    // Insert the given text as a STYLE element at the beginning of the
    // document. |elementId| can be empty, but if specified then it is used
    // as the id for the newly inserted element (replacing an existing one
    // with the same id, if any).
    virtual bool insertStyleText(const WebString& styleText,
                                 const WebString& elementId) = 0;


    // Navigation ----------------------------------------------------------

    // Reload the current document.
    virtual void reload() = 0;

    // Load the given URL.
    virtual void loadRequest(const WebURLRequest&) = 0;

    // Load the given history state, corresponding to a back/forward
    // navigation.
    virtual void loadHistoryItem(const WebHistoryItem&) = 0;

    // Loads the given data with specific mime type and optional text
    // encoding.  For HTML data, baseURL indicates the security origin of
    // the document and is used to resolve links.  If specified,
    // unreachableURL is reported via WebDataSource::unreachableURL.  If
    // replace is false, then this data will be loaded as a normal
    // navigation.  Otherwise, the current history item will be replaced.
    virtual void loadData(const WebData& data,
                          const WebString& mimeType,
                          const WebString& textEncoding,
                          const WebURL& baseURL,
                          const WebURL& unreachableURL = WebURL(),
                          bool replace = false) = 0;

    // This method is short-hand for calling LoadData, where mime_type is
    // "text/html" and text_encoding is "UTF-8".
    virtual void loadHTMLString(const WebData& html,
                                const WebURL& baseURL,
                                const WebURL& unreachableURL = WebURL(),
                                bool replace = false) = 0;

    // Returns true if the current frame is busy loading content.
    virtual bool isLoading() const = 0;

    // Stops any pending loads on the frame and its children.
    virtual void stopLoading() = 0;

    // Returns the data source that is currently loading.  May be null.
    virtual WebDataSource* provisionalDataSource() const = 0;

    // Returns the data source that is currently loaded.
    virtual WebDataSource* dataSource() const = 0;

    // Returns the previous history item.  Check WebHistoryItem::isNull()
    // before using.
    virtual WebHistoryItem previousHistoryItem() const = 0;

    // Returns the current history item.  Check WebHistoryItem::isNull()
    // before using.
    virtual WebHistoryItem currentHistoryItem() const = 0;

    // View-source rendering mode.  Set this before loading an URL to cause
    // it to be rendered in view-source mode.
    virtual void enableViewSourceMode(bool) = 0;
    virtual bool isViewSourceModeEnabled() const = 0;

    // Sets the referrer for the given request to be the specified URL or
    // if that is null, then it sets the referrer to the referrer that the
    // frame would use for subresources.  NOTE: This method also filters
    // out invalid referrers (e.g., it is invalid to send a HTTPS URL as
    // the referrer for a HTTP request).
    virtual void setReferrerForRequest(WebURLRequest&, const WebURL&) = 0;

    // Called to associate the WebURLRequest with this frame.  The request
    // will be modified to inherit parameters that allow it to be loaded.
    // This method ends up triggering WebFrameClient::willSendRequest.
    virtual void dispatchWillSendRequest(WebURLRequest&) = 0;

    // Called from within WebFrameClient::didReceiveDocumentData to commit
    // data for the frame that will be used to construct the frame's
    // document.
    virtual void commitDocumentData(const char* data, size_t length) = 0;

    // Returns the number of registered unload listeners.
    virtual unsigned unloadListenerCount() const = 0;

    // Returns true if a user gesture is currently being processed.
    virtual bool isProcessingUserGesture() const = 0;

    // Returns true if this frame is in the process of opening a new frame
    // with a suppressed opener.
    virtual bool willSuppressOpenerInNewFrame() const = 0;


    // Editing -------------------------------------------------------------

    // Replaces the selection with the given text.
    virtual void replaceSelection(const WebString& text) = 0;

    virtual void insertText(const WebString& text) = 0;

    virtual void setMarkedText(const WebString& text, unsigned location, unsigned length) = 0;
    virtual void unmarkText() = 0;
    virtual bool hasMarkedText() const = 0;

    virtual WebRange markedRange() const = 0;

    // Supports commands like Undo, Redo, Cut, Copy, Paste, SelectAll,
    // Unselect, etc. See EditorCommand.cpp for the full list of supported
    // commands.
    virtual bool executeCommand(const WebString&) = 0;
    virtual bool executeCommand(const WebString&, const WebString& value) = 0;
    virtual bool isCommandEnabled(const WebString&) const = 0;

    // Spell-checking support.
    virtual void enableContinuousSpellChecking(bool) = 0;
    virtual bool isContinuousSpellCheckingEnabled() const = 0;


    // Selection -----------------------------------------------------------

    virtual bool hasSelection() const = 0;

    virtual WebRange selectionRange() const = 0;

    virtual WebString selectionAsText() const = 0;
    virtual WebString selectionAsMarkup() const = 0;

    // Expands the selection to a word around the caret and returns
    // true. Does nothing and returns false if there is no caret or
    // there is ranged selection.
    virtual bool selectWordAroundCaret() = 0;


    // Printing ------------------------------------------------------------

    // Reformats the WebFrame for printing.  pageSize is the page size in
    // pixels.  Returns the number of pages that can be printed at the
    // given page size.
    virtual int printBegin(const WebSize& pageSize) = 0;

    // Returns the page shrinking factor calculated by webkit (usually
    // between 1/1.25 and 1/2). Returns 0 if the page number is invalid or
    // not in printing mode.
    virtual float getPrintPageShrink(int page) = 0;

    // Prints one page, and returns the calculated page shrinking factor
    // (usually between 1/1.25 and 1/2).  Returns 0 if the page number is
    // invalid or not in printing mode.
    virtual float printPage(int pageToPrint, WebCanvas*) = 0;

    // Reformats the WebFrame for screen display.
    virtual void printEnd() = 0;


    // Find-in-page --------------------------------------------------------

    // Searches a frame for a given string.
    //
    // If a match is found, this function will select it (scrolling down to
    // make it visible if needed) and fill in selectionRect with the
    // location of where the match was found (in window coordinates).
    //
    // If no match is found, this function clears all tickmarks and
    // highlighting.
    //
    // Returns true if the search string was found, false otherwise.
    virtual bool find(int identifier,
                      const WebString& searchText,
                      const WebFindOptions& options,
                      bool wrapWithinFrame,
                      WebRect* selectionRect) = 0;

    // Notifies the frame that we are no longer interested in searching.
    // This will abort any asynchronous scoping effort already under way
    // (see the function scopeStringMatches for details) and erase all
    // tick-marks and highlighting from the previous search.  If
    // clearSelection is true, it will also make sure the end state for the
    // find operation does not leave a selection.  This can occur when the
    // user clears the search string but does not close the find box.
    virtual void stopFinding(bool clearSelection) = 0;

    // Counts how many times a particular string occurs within the frame.
    // It also retrieves the location of the string and updates a vector in
    // the frame so that tick-marks and highlighting can be drawn.  This
    // function does its work asynchronously, by running for a certain
    // time-slice and then scheduling itself (co-operative multitasking) to
    // be invoked later (repeating the process until all matches have been
    // found).  This allows multiple frames to be searched at the same time
    // and provides a way to cancel at any time (see
    // cancelPendingScopingEffort).  The parameter searchText specifies
    // what to look for and |reset| signals whether this is a brand new
    // request or a continuation of the last scoping effort.
    virtual void scopeStringMatches(int identifier,
                                    const WebString& searchText,
                                    const WebFindOptions& options,
                                    bool reset) = 0;

    // Cancels any outstanding requests for scoping string matches on a frame.
    virtual void cancelPendingScopingEffort() = 0;

    // This function is called on the main frame during the scoping effort
    // to keep a running tally of the accumulated total match-count for all
    // frames.  After updating the count it will notify the WebViewClient
    // about the new count.
    virtual void increaseMatchCount(int count, int identifier) = 0;

    // This function is called on the main frame to reset the total number
    // of matches found during the scoping effort.
    virtual void resetMatchCount() = 0;


    // Password autocompletion ---------------------------------------------

    // Registers a listener for the specified user name input element. The
    // listener will receive notifications for blur and when autocomplete
    // should be triggered.
    // The WebFrame becomes the owner of the passed listener.
    virtual void registerPasswordListener(
        WebInputElement,
        WebPasswordAutocompleteListener*) = 0;


    // Utility -------------------------------------------------------------

    // Given a relative URL, returns an absolute URL by resolving the URL
    // relative to the base URL of the frame's document.  This uses the
    // same algorithm that WebKit uses to resolve hyperlinks found in a
    // HTML document.
    // Deprecated. Use document().completeURL() instead.
    virtual WebURL completeURL(const WebString&) const = 0;

    // Returns the contents of this frame as a string.  If the text is
    // longer than maxChars, it will be clipped to that length.  WARNING:
    // This function may be slow depending on the number of characters
    // retrieved and page complexity.  For a typically sized page, expect
    // it to take on the order of milliseconds.
    //
    // If there is room, subframe text will be recursively appended. Each
    // frame will be separated by an empty line.
    virtual WebString contentAsText(size_t maxChars) const = 0;

    // Returns HTML text for the contents of this frame.  This is generated
    // from the DOM.
    virtual WebString contentAsMarkup() const = 0;

    // Returns a text representation of the render tree.  This method is used
    // to support layout tests.
    virtual WebString renderTreeAsText() const = 0;

    // Returns the counter value for the specified element.  This method is
    // used to support layout tests.
    virtual WebString counterValueForElementById(const WebString& id) const = 0;


    // Returns the number of page where the specified element will be put.
    // This method is used to support layout tests.
    virtual int pageNumberForElementById(const WebString& id,
                                         float pageWidthInPixels,
                                         float pageHeightInPixels) const = 0;

protected:
    ~WebFrame() { }
};

} // namespace WebKit

#endif
