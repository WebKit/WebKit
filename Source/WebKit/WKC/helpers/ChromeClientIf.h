/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (c) 2010-2014 ACCESS CO., LTD. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WKCChromeClient_h
#define WKCChromeClient_h

#include <stdint.h>
#include <stddef.h>
#include <wkc/wkcbase.h>
#include "WKCHelpersEnums.h"

namespace WKC {

class Element;
class FileChooser;
class Frame;
class FrameLoader;
class FrameLoadRequest;
class Geolocation;
class GraphicsLayer;
class HTMLParserQuirks;
class HitTestResult;
class Node;
class Page;
class ScrollView;
class SecurityOrigin;
class String;
class WindowFeatures;

/*@{*/

struct ViewportArguments;

typedef void* PlatformPageClient;
/** @brief Structure for storing cursor information */
struct WKCPlatformCursor_ {
    WKCPlatformCursor_(int type)
        : fType(type), fBitmap(0), fRowBytes(0), fBPP(0), fData(0) {
            WKCSize_Set(&fSize, 0, 0);
            WKCPoint_Set(&fHotSpot, 0, 0);
    }
    ~WKCPlatformCursor_() {}
    /** @brief Cursor type */
    int fType;
    /** @brief Pointer to bitmap data */
    void* fBitmap;
    /** @brief Number of bytes per line of offscreen */
    int fRowBytes;
    /** @brief Number of bits per pixel */
    int fBPP;
    /** @brief Size */
    WKCSize fSize;
    /** @brief Cursor position */
    WKCPoint fHotSpot;
    /** @brief Pointer to cursor data */
    void* fData;
};
/** @brief Type definition of WKC::WKCPlatformCursor */
typedef struct WKCPlatformCursor_ WKCPlatformCursor;

typedef WKCPlatformCursor *PlatformCursorHandle;

/** @brief Class that notifies of an event for the content display screen of the browser. In this description, only those functions that were extended by ACCESS for NetFront Browser NX are described, those inherited from WebCore::ChromeClient are not described. */
class WKC_API ChromeClientIf
{
public:
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Notifies of discarding chrome
       @return None
       @endcond
    */
    virtual void chromeDestroyed() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Sets window area rectangle
       @param rect Reference to rectangle information
       @return None
       @endcond
    */
    virtual void setWindowRect(const WKCFloatRect&) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Gets window area rectangle
       @retval WKCFloatRect Window rectangle information
       @endcond
    */
    virtual WKCFloatRect windowRect() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Gets page area rectangle
       @retval WKCFloatRect Page rectangle information
       @endcond
    */
    virtual WKCFloatRect pageRect() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Notifies of focus
       @return None
       @endcond
    */
    virtual void focus() = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Notifies of unfocus
       @return None
       @endcond
    */
    virtual void unfocus() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Checks if focus movement to UI side component is available
       @param focus Direction of focus move
       @retval !false Move focus
       @retval false Do not move focus
       @details
       Checks whether to switch the focus from the content to a UI-side component. @n
       Since returning !false as a return value removes focus from the current content, give focus to a UI component. @n
       If focus is not moved from content to UI, always return false.
       @endcond
    */
    virtual bool canTakeFocus(WKC::WKCFocusDirection) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKC::FocusDirection (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void takeFocus(WKC::WKCFocusDirection) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKC::Node* (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual void focusedNodeChanged(WKC::Node*) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to generate window
       @param frame Pointer to WKC::Frame
       @param request Reference to WKC::FrameLoadRequest
       @param features Reference to WKC::WindowFeatures
       @retval page Pointer to WKC::Page
       @details
       Called when a window is generated by the window.open process.@n
       This function generates a new WKC::WKCWebView and returns the WKC::Page of its WebView.
       @endcond
    */
    virtual WKC::Page* createWindow(WKC::Frame*, const WKC::FrameLoadRequest&, const WKC::WindowFeatures&) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to display window
       @return None
       @endcond
    */
    virtual void show() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Checks if modal dialog box can be displayed
       @retval !false Can display modal dialog box
       @retval false Cannot display modal dialog box
       @endcond
    */
    virtual bool canRunModal() = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to display modal dialog box
       @return None
       @details
       Displays the WebView obtained from WKC::ChromeClientIf::webView() as a modal dialog box. @n
       Implement the application so that other WebViews cannot be operated.
       @endcond
    */
    virtual void runModal() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param bool (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void setToolbarsVisible(bool) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual bool toolbarsVisible() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param bool (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual void setStatusbarVisible(bool) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual bool statusbarVisible() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENTv
       @brief (TBD) implement description
       @param bool (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void setScrollbarsVisible(bool) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual bool scrollbarsVisible() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param bool (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void setMenubarVisible(bool) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual bool menubarVisible() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param bool (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual void setResizable(bool) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param source (TBD) implement description
       @param type (TBD) implement description
       @param level (TBD) implement description
       @param message (TBD) implement description
       @param lineNumber (TBD) implement description
       @param sourceID (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void addMessageToConsole(WKC::MessageSource source, WKC::MessageType type,
                                     WKC::MessageLevel level, const WKC::String& message,
                                     unsigned int lineNumber, const WKC::String& sourceID) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Checks if beforeunload event confirmation dialog box can be displayed
       @retval !false Can display event confirmation dialog box
       @retval false Cannot display event confirmation dialog box
       @details
       (TBD) implement description
       @endcond
    */
    virtual bool canRunBeforeUnloadConfirmPanel() = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to display beforeunload event confirmation dialog box
       @param message Reference to message string
       @param frame Pointer to WKC::Frame
       @retval !false Issue unload
       @retval false Do not issue unload
       @details
       (TBD) implement description
       @endcond
    */
    virtual bool runBeforeUnloadConfirmPanel(const WKC::String& message, WKC::Frame* frame) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to close window
       @return None
       @details
       Called when the window is closed by the window.close process.
       @attention
       Since WKC::ChromeClientIf is a member of WKC::WKCWebView, do not discard the WKC::WKCWebView to be closed in this callback.@n
       Set a timer in this callback, and discard WKC::WKCWebView when the timer fires.
       @endcond
    */
    virtual void closeWindowSoon() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to display JavaScript alert dialog box
       @param frame Pointer to WKC::Frame
       @param msg Reference to alert message string
       @return None
       @details
       (TBD) implement description
       @endcond
    */
    virtual void runJavaScriptAlert(WKC::Frame*, const WKC::String&) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to display JavaScript confirm dialog box
       @param frame Pointer to WKC::Frame
       @param msg Reference to confirm message string
       @retval !false Affirmative response
       @retval false Negative response
       @details
       (TBD) implement description 
       @endcond
    */
    virtual bool runJavaScriptConfirm(WKC::Frame*, const WKC::String&) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to display JavaScript prompt dialog box
       @param frame Pointer to WKC::Frame
       @param message Reference to prompt message string
       @param defaultValue Reference to initial display string
       @param result Reference to input result string
       @retval !false Affirmative response
       @retval false Negative response
       @details
       (TBD) implement description 
       @endcond
    */
    virtual bool runJavaScriptPrompt(WKC::Frame*, const WKC::String& message, const WKC::String& defaultValue, WKC::String& result) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKC::String& (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void setStatusbarText(const WKC::String&) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to check whether to interrupt script execution
       @return None
       @endcond
    */
    virtual bool shouldInterruptJavaScript() = 0;

    virtual void registerProtocolHandler(const WKC::String& scheme, const WKC::String& baseURL, const WKC::String& url, const WKC::String& title) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual WKCRect windowResizerRect() const = 0;

    /**
       @brief Requests to redraw.
       @param rect Reference to rectangle to redraw
       @param contentChanged Type of redraw request @n
       - != false Redraw due to change in content details
       - == false Redraw due to other than change in content details
       @param immediate Update timing (default: false)@n
       - != false Immediately
       - == false Any time
       @param repaintContentOnly  (TBD) implement description (default: false)
       @return None
    */
    virtual void repaint(const WKCRect& rect, bool contentChanged, bool immediate = false, bool repaintContentOnly = false) = 0;
    virtual void invalidateContentsForSlowScroll(const WKCRect& rect, bool immediate) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to scroll
       @param scrollDelta Amount of scroll movement
       @param rectToScroll Reference to rectangle to scroll
       @param clipRect Reference to clipping rectangle
       @return None
       @details
       To scroll a screen, call WKC::WKCWebView::notifyScrollOffscreen() within this function.
       @endcond
    */
    virtual void scroll(const WKCSize& scrollDelta, const WKCRect& rectToScroll, const WKCRect& clipRect) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKCPoint& (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual WKCPoint screenToWindow(const WKCPoint&) const = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKCRect& (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual WKCRect windowToScreen(const WKCRect&) const = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual PlatformPageClient platformPageClient() const = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Notifies of content size change
       @param frame Pointer to WKC::Frame
       @param size Reference to changed size
       @return None
       @endcond
    */
    virtual void contentsSizeChanged(WKC::Frame*, const WKCSize&) const = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Notifies to schedule animation
       @return None
       @endcond
    */
    virtual void scheduleAnimation() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Notifies of scroll bar mode change
       @return None
       @attention
       Not supported. This function is not called in the current implementation.
       @endcond
    */
    virtual void scrollbarsModeDidChange() const = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKC::HitTestResult& (TBD) implement description
       @param modifierFlags (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual void mouseDidMoveOverElement(const WKC::HitTestResult&, unsigned modifierFlags) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKC::String& (TBD) implement description
       @param  WKC::TextDirection (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual void setToolTip(const WKC::String&, WKC::TextDirection) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKC::Frame* (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual void print(WKC::Frame*) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param DATABASE (TBD) implement description
       @return (TBD) implement description 
       @endcond
    */
    virtual void exceededDatabaseQuota(WKC::Frame*, const WKC::String&) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param spaceNeeded (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param origin (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void reachedApplicationCacheOriginQuota(WKC::SecurityOrigin* origin, int64_t totalSpaceNeeded) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Requests to display file selection dialog box
       @param frame Pointer to WKC::Frame
       @param chooser Pointer to WKC::FileChooser
       @return None
       @details
       @ref See bbb-fileselect.
       @endcond
    */
    virtual void runOpenPanel(WKC::Frame*, WKC::FileChooser*) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param msg (TBD) implement description
       @param chooser (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void enumerateChosenDirectory(WKC::FileChooser*) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKC::Node* (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual void formStateDidChange(const WKC::Node*) = 0;

    virtual void elementDidFocus(const WKC::Node*) = 0;
    virtual void elementDidBlur(const WKC::Node*) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual WKC::HTMLParserQuirks* createHTMLParserQuirks() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param WKC::PlatformCursorHandle (TBD) implement description
       @return None
       @endcond
    */
    virtual void setCursor(WKC::PlatformCursorHandle) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Gives viewport data change notification
       @param viewport Viewport data
       @return None
       @endcond
    */
    virtual void dispatchViewportDataDidChange(const WKC::ViewportArguments&) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @return (TBD) implement description
       @endcond
    */
    virtual bool shouldUnavailablePluginMessageBeButton(int) const = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param element (TBD) implement description
       @param reason (TBD) implement description
       @return None
       @endcond
    */
    virtual void unavailablePluginButtonClicked(WKC::Element* element, int reason) const =0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Notifies that scroll occurred in the content
       @param rect Coordinate to scroll to
       @param view Pointer to ScrollView
       @return None
       @details
       This is called by a scroll request from a fragment jump or JavaScript.
       @endcond
    */
    virtual void scrollRectIntoView(const WKCRect&) const = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual void focusedFrameChanged(WKC::Frame*) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Gives keyboard input mode
       @param None
       @return
       @retval KeyboardAccessDefault Default settings
       @retval KeyboardAccessFull Full keyboard settings
       @retval KeyboardAccessTabsToLinks Settings for focus movement of link by pressing tab
       @endcond
    */
    virtual WKC::KeyboardUIMode keyboardUIMode() = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual bool shouldRubberBandInDirection(WKC::ScrollDirection) const = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual bool selectItemWritingDirectionIsNatural() = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param num (TBD) implement description
       @endcond
    */
    virtual void numWheelEventHandlersChanged(unsigned num) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param num (TBD) implement description
       @endcond
    */
    virtual void numTouchEventHandlersChanged(unsigned num) = 0;
    virtual bool hasOpenedPopup() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual void populateVisitedLinks() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual WKCFloatRect customHighlightRect(WKC::Node*, const WKC::String& type, const WKCFloatRect& lineRect) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual void paintCustomHighlight(WKC::Node*, const WKC::String& type, const WKCFloatRect& boxRect, const WKCFloatRect& lineRect,
            bool behindText, bool entireLine) = 0;
            
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual bool shouldReplaceWithGeneratedFileForUpload(const WKC::String& path, WKC::String& generatedFilename) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual WKC::String generateReplacementFile(const WKC::String& path) = 0;

    enum CompositingTrigger {
        ThreeDTransformTrigger = 1 << 0,
        VideoTrigger = 1 << 1,
        PluginTrigger = 1 << 2,
        CanvasTrigger = 1 << 3,
        AnimationTrigger = 1 << 4,
        FilterTrigger = 1 << 5,
        ThreeDBlockTrigger = 1 << 28, // belows are WKC Original
        ThreeDImageTrigger = 1 << 29,
        TwoDTransformTrigger = 1 << 30,
        OpacityTrigger = 1 << 31,
        AllTriggers = 0xFFFFFFFF
    };
    typedef unsigned CompositingTriggerFlags;
    virtual void attachRootGraphicsLayer(WKC::Frame*, WKC::GraphicsLayer*) = 0;
    virtual void setNeedsOneShotDrawingSynchronization() = 0;
    virtual void scheduleCompositingLayerSync() = 0;
    virtual bool allowsAcceleratedCompositing() const = 0;
    virtual CompositingTriggerFlags allowedCompositingTriggers() const = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual bool supportsFullscreenForNode(const WKC::Node*) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual void enterFullscreenForNode(WKC::Node*) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual void exitFullscreenForNode(WKC::Node*) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual bool requiresFullscreenForVideoPlayback() = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual bool supportsFullScreenForElement(const WKC::Element*, bool) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual void enterFullScreenForElement(WKC::Element*) = 0;
    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual void exitFullScreenForElement(WKC::Element*) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief (TBD) implement description
       @param view (TBD) implement description
       @endcond
    */
    virtual void notifyScrollerThumbIsVisibleInRect(const WKCRect&) = 0;
    virtual void recommendedScrollbarStyleDidChange(int) = 0;

    /**
       @cond WKC_PRIVATE_DOCUMENT
       @brief Sets touch event processing
       @param flag Settings for enabling/disabling touch event
       @retval !false Enable
       @retval false Disable
       @return None
       @endcond
    */
    virtual void needTouchEvents(bool) = 0;

    // Web Notifications API integration hooks.
    // Permission state values: -1 = default (not decided), 0 = denied, 1 = granted.
    // Embedders should persist decisions per origin and invoke the callback exactly
    // once, including when the permission prompt is dismissed.
    virtual int checkWebNotificationPermission(const WKC::String& origin) const
    {
        (void)origin;
        return -1;
    }
    virtual void requestWebNotificationPermission(const WKC::String& origin, void (*completion)(void*, int), void* context)
    {
        (void)origin;
        if (completion)
            completion(context, 0);
    }
    // Return true only when delivery was accepted. An embedder may use a Wii U
    // system notification facility. This hook does not promise official-inbox
    // persistence; the platform delivery backend must implement that separately.
    virtual bool showWebNotification(const WKC::String& origin, const WKC::String& title, const WKC::String& body)
    {
        (void)origin; (void)title; (void)body;
        return false;
    }
    virtual void cancelWebNotification(const WKC::String& origin, const WKC::String& title)
    {
        (void)origin; (void)title;
    }

};

/*@}*/

} // namespace

#endif // WKCChromeClient_h
