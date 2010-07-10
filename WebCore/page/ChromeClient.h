/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple, Inc. All rights reserved.
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

#ifndef ChromeClient_h
#define ChromeClient_h

#include "Console.h"
#include "Cursor.h"
#include "FocusDirection.h"
#include "GraphicsContext.h"
#include "HTMLParserQuirks.h"
#include "HostWindow.h"
#include "ScrollTypes.h"
#include <wtf/Forward.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include "WebCoreKeyboardUIMode.h"
#endif

#ifndef __OBJC__
class NSMenu;
class NSResponder;
#endif

namespace WebCore {

    class AtomicString;
    class Element;
    class FileChooser;
    class FloatRect;
    class Frame;
    class Geolocation;
    class HTMLParserQuirks;
    class HitTestResult;
    class IntRect;
    class Node;
    class Page;
    class String;
    class Widget;

    struct FrameLoadRequest;
    struct ViewportArguments;
    struct WindowFeatures;

#if USE(ACCELERATED_COMPOSITING)
    class GraphicsLayer;
#endif

#if ENABLE(NOTIFICATIONS)
    class NotificationPresenter;
#endif

    class ChromeClient {
    public:
        virtual void chromeDestroyed() = 0;
        
        virtual void setWindowRect(const FloatRect&) = 0;
        virtual FloatRect windowRect() = 0;
        
        virtual FloatRect pageRect() = 0;
        
        virtual float scaleFactor() = 0;
    
        virtual void focus() = 0;
        virtual void unfocus() = 0;

        virtual bool canTakeFocus(FocusDirection) = 0;
        virtual void takeFocus(FocusDirection) = 0;

        virtual void focusedNodeChanged(Node*) = 0;

        // The Frame pointer provides the ChromeClient with context about which
        // Frame wants to create the new Page.  Also, the newly created window
        // should not be shown to the user until the ChromeClient of the newly
        // created Page has its show method called.
        virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&) = 0;
        virtual void show() = 0;

        virtual bool canRunModal() = 0;
        virtual void runModal() = 0;

        virtual void setToolbarsVisible(bool) = 0;
        virtual bool toolbarsVisible() = 0;
        
        virtual void setStatusbarVisible(bool) = 0;
        virtual bool statusbarVisible() = 0;
        
        virtual void setScrollbarsVisible(bool) = 0;
        virtual bool scrollbarsVisible() = 0;
        
        virtual void setMenubarVisible(bool) = 0;
        virtual bool menubarVisible() = 0;

        virtual void setResizable(bool) = 0;
        
        virtual void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, unsigned int lineNumber, const String& sourceID) = 0;

        virtual bool canRunBeforeUnloadConfirmPanel() = 0;
        virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame) = 0;

        virtual void closeWindowSoon() = 0;
        
        virtual void runJavaScriptAlert(Frame*, const String&) = 0;
        virtual bool runJavaScriptConfirm(Frame*, const String&) = 0;
        virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result) = 0;
        virtual void setStatusbarText(const String&) = 0;
        virtual bool shouldInterruptJavaScript() = 0;
        virtual bool tabsToLinks() const = 0;

        virtual void registerProtocolHandler(const String&, const String&, const String&, const String&) { }
        virtual void registerContentHandler(const String&, const String&, const String&, const String&) { }

        virtual IntRect windowResizerRect() const = 0;

        // Methods used by HostWindow.
        virtual void invalidateWindow(const IntRect&, bool) = 0;
        virtual void invalidateContentsAndWindow(const IntRect&, bool) = 0;
        virtual void invalidateContentsForSlowScroll(const IntRect&, bool) = 0;
        virtual void scroll(const IntSize&, const IntRect&, const IntRect&) = 0;
        virtual IntPoint screenToWindow(const IntPoint&) const = 0;
        virtual IntRect windowToScreen(const IntRect&) const = 0;
        virtual PlatformPageClient platformPageClient() const = 0;
        virtual void contentsSizeChanged(Frame*, const IntSize&) const = 0;
        virtual void scrollRectIntoView(const IntRect&, const ScrollView*) const = 0; // Currently only Mac has a non empty implementation.
        // End methods used by HostWindow.

        virtual void scrollbarsModeDidChange() const = 0;
        virtual bool shouldMissingPluginMessageBeButton() const { return false; }
        virtual void missingPluginButtonClicked(Element*) const { }
        virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) = 0;

        virtual void setToolTip(const String&, TextDirection) = 0;

        virtual void didReceiveViewportArguments(Frame*, const ViewportArguments&) const { }

        virtual void print(Frame*) = 0;

#if ENABLE(DATABASE)
        virtual void exceededDatabaseQuota(Frame*, const String& databaseName) = 0;
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        // Callback invoked when the application cache fails to save a cache object
        // because storing it would grow the database file past its defined maximum
        // size or past the amount of free space on the device. 
        // The chrome client would need to take some action such as evicting some
        // old caches.
        virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) = 0;
#endif

#if ENABLE(DASHBOARD_SUPPORT)
        virtual void dashboardRegionsChanged();
#endif

#if ENABLE(NOTIFICATIONS)
        virtual NotificationPresenter* notificationPresenter() const = 0;
#endif

        virtual void populateVisitedLinks();

        virtual FloatRect customHighlightRect(Node*, const AtomicString& type, const FloatRect& lineRect);
        virtual void paintCustomHighlight(Node*, const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect,
            bool behindText, bool entireLine);
            
        virtual bool shouldReplaceWithGeneratedFileForUpload(const String& path, String& generatedFilename);
        virtual String generateReplacementFile(const String& path);

        virtual bool paintCustomScrollbar(GraphicsContext*, const FloatRect&, ScrollbarControlSize, 
                                          ScrollbarControlState, ScrollbarPart pressedPart, bool vertical,
                                          float value, float proportion, ScrollbarControlPartMask);
        virtual bool paintCustomScrollCorner(GraphicsContext*, const FloatRect&);

        // This can be either a synchronous or asynchronous call. The ChromeClient can display UI asking the user for permission
        // to use Geolocation.
        virtual void requestGeolocationPermissionForFrame(Frame*, Geolocation*) = 0;
        virtual void cancelGeolocationPermissionRequestForFrame(Frame*, Geolocation*) = 0;
            
        virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) = 0;
        // Asynchronous request to load an icon for specified filenames.
        virtual void chooseIconForFiles(const Vector<String>&, FileChooser*) = 0;

        virtual bool setCursor(PlatformCursorHandle) = 0;

        // Notification that the given form element has changed. This function
        // will be called frequently, so handling should be very fast.
        virtual void formStateDidChange(const Node*) = 0;
        
        virtual void formDidFocus(const Node*) { };
        virtual void formDidBlur(const Node*) { };

        virtual PassOwnPtr<HTMLParserQuirks> createHTMLParserQuirks() = 0;

#if USE(ACCELERATED_COMPOSITING)
        // Pass 0 as the GraphicsLayer to detatch the root layer.
        virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) = 0;
        // Sets a flag to specify that the next time content is drawn to the window,
        // the changes appear on the screen in synchrony with updates to GraphicsLayers.
        virtual void setNeedsOneShotDrawingSynchronization() = 0;
        // Sets a flag to specify that the view needs to be updated, so we need
        // to do an eager layout before the drawing.
        virtual void scheduleCompositingLayerSync() = 0;
        // Returns whether or not the client can render the composited layer,
        // regardless of the settings.
        virtual bool allowsAcceleratedCompositing() const { return true; }
#endif

        virtual bool supportsFullscreenForNode(const Node*) { return false; }
        virtual void enterFullscreenForNode(Node*) { }
        virtual void exitFullscreenForNode(Node*) { }
        
#if PLATFORM(MAC)
        virtual KeyboardUIMode keyboardUIMode() { return KeyboardAccessDefault; }

        virtual NSResponder *firstResponder() { return 0; }
        virtual void makeFirstResponder(NSResponder *) { }

        virtual void willPopUpMenu(NSMenu *) { }
#endif

#if ENABLE(TOUCH_EVENTS)
        virtual void needTouchEvents(bool) = 0;
#endif

#if ENABLE(WIDGETS_10_SUPPORT)
        virtual bool isWindowed() { return false; }
        virtual bool isFloating() { return false; }
        virtual bool isFullscreen() { return false; }
        virtual bool isMaximized() { return false; }
        virtual bool isMinimized() { return false; }
#endif

    protected:
        virtual ~ChromeClient() { }
    };

}

#endif // ChromeClient_h
