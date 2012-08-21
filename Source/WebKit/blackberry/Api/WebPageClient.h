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

#ifndef WebPageClient_h
#define WebPageClient_h

#include "BlackBerryGlobal.h"
#include "WebString.h"

#include <BlackBerryPlatformCursor.h>
#include <BlackBerryPlatformInputEvents.h>
#include <BlackBerryPlatformNavigationType.h>
#include <BlackBerryPlatformPrimitives.h>
#include <imf/events.h>
#include <interaction/ScrollViewBase.h>
#include <vector>

template<typename T> class ScopeArray;
template<typename T> class SharedArray;

typedef void* WebFrame;

namespace BlackBerry {

namespace Platform {
class FilterStream;
class GeoTrackerListener;
class IntRectRegion;
class NetworkRequest;
class NetworkStreamFactory;

namespace Graphics {
class Window;
}
}

namespace WebKit {
class WebPage;

class BLACKBERRY_EXPORT WebPageClient {
public:
    virtual ~WebPageClient() { }
    enum WindowStyleFlag {
        FlagWindowHasMenuBar = 0x00000001,
        FlagWindowHasToolBar = 0x00000002,
        FlagWindowHasLocationBar = 0x00000004,
        FlagWindowHasStatusBar = 0x00000008,
        FlagWindowHasScrollBar = 0x00000010,
        FlagWindowIsResizable = 0x00000020,
        FlagWindowIsFullScreen = 0x00000040,
        FlagWindowIsDialog = 0x00000080,
        FlagWindowDefault = 0xFFFFFFFF,
    };

    enum AlertType {
        MediaOK = 0,
        MediaDecodeError,
        MediaMetaDataError,
        MediaMetaDataTimeoutError,
        MediaNoMetaDataError,
        MediaVideoReceiveError,
        MediaAudioReceiveError,
        MediaInvalidError,
    };

    enum SaveCredentialType {
        SaveCredentialNeverForThisSite = 0,
        SaveCredentialNotNow,
        SaveCredentialYes
    };

    virtual int getInstanceId() const = 0;

    virtual void notifyLoadStarted() = 0;
    virtual void notifyLoadCommitted(const unsigned short* originalUrl, unsigned int originalUrlLength, const unsigned short* finalUrl, unsigned int finalUrlLength, const unsigned short* networkToken, unsigned int networkTokenLength) = 0;
    virtual void notifyLoadFailedBeforeCommit(const unsigned short* originalUrl, unsigned int originalUrlLength, const unsigned short* finalUrl, unsigned int finalUrlLength, const unsigned short* networkToken, unsigned int networkTokenLength) = 0;
    virtual void notifyLoadToAnchor(const unsigned short* url, unsigned int urlLength, const unsigned short* networkToken, unsigned int networkTokenLength) = 0;
    virtual void notifyLoadProgress(int percentage) = 0;
    virtual void notifyLoadReadyToRender(bool pageIsVisuallyNonEmpty) = 0;
    virtual void notifyFirstVisuallyNonEmptyLayout() = 0;
    virtual void notifyLoadFinished(int status) = 0;
    virtual void notifyClientRedirect(const unsigned short* originalUrl, unsigned int originalUrlLength, const unsigned short* finalUrl, unsigned int finalUrlLength) = 0;

    virtual void notifyFrameDetached(const WebFrame) = 0;

    virtual void notifyRunLayoutTestsFinished() = 0;

    virtual void notifyInRegionScrollingStartingPointChanged(const std::vector<Platform::ScrollViewBase*>&) = 0;
    virtual void notifyNoMouseMoveOrTouchMoveHandlers() = 0;

    virtual void notifyDocumentOnLoad() = 0;

    virtual void notifyWindowObjectCleared() = 0;
    virtual WebString invokeClientJavaScriptCallback(const char* const* args, unsigned numArgs) = 0;

    virtual void addMessageToConsole(const unsigned short* message, unsigned messageLength, const unsigned short* source, unsigned sourceLength, unsigned lineNumber) = 0;
    virtual int showAlertDialog(AlertType) = 0;

    virtual void runJavaScriptAlert(const unsigned short* message, unsigned messageLength, const char* origin, unsigned originLength) = 0;
    virtual bool runJavaScriptConfirm(const unsigned short* message, unsigned messageLength, const char* origin, unsigned originLength) = 0;
    virtual bool runJavaScriptPrompt(const unsigned short* message, unsigned messageLength, const unsigned short* defaultValue, unsigned defaultValueLength, const char* origin, unsigned originLength, WebString& result) = 0;
    virtual bool runBeforeUnloadConfirmPanel(const unsigned short* message, unsigned messageLength, const char* origin, unsigned originLength) = 0;

    virtual bool shouldInterruptJavaScript() = 0;

    virtual void javascriptSourceParsed(const unsigned short* url, unsigned urlLength, const unsigned short* script, unsigned scriptLength) = 0;
    virtual void javascriptParsingFailed(const unsigned short* url, unsigned urlLength, const unsigned short* error, unsigned errorLength, int lineNumber) = 0;
    virtual void javascriptPaused(const unsigned short* stack, unsigned stackLength) = 0;
    virtual void javascriptContinued() = 0;

    // All of these methods use transformed coordinates.
    virtual void contentsSizeChanged(const Platform::IntSize&) const = 0;
    virtual void scrollChanged(const Platform::IntPoint&) const = 0;
    virtual void zoomChanged(bool isMinZoomed, bool isMaxZoomed, bool isAtInitialZoom, double newZoom) const = 0;

    virtual void requestUpdateViewport(int width, int height) = 0;

    virtual void setPageTitle(const unsigned short* title, unsigned titleLength) = 0;

    virtual Platform::Graphics::Window* window() const = 0;

    virtual void notifyContentRendered(const Platform::IntRect&) = 0;
    virtual void resizeSurfaceIfNeeded() = 0;

    virtual void inputFocusGained(Platform::BlackBerryInputType, int inputStyle, Platform::VirtualKeyboardType, Platform::VirtualKeyboardEnterKeyType) = 0;
    virtual void inputFocusLost() = 0;
    virtual void inputTextChanged() = 0;
    virtual void inputSelectionChanged(unsigned selectionStart, unsigned selectionEnd) = 0;
    virtual void inputLearnText(wchar_t* text, int length) = 0;

    virtual void showVirtualKeyboard(bool) = 0;

    virtual void requestSpellingCheckingOptions(imf_sp_text_t&) = 0;
    virtual int32_t checkSpellingOfStringAsync(wchar_t* text, int length) = 0;

    virtual void notifySelectionDetailsChanged(const Platform::IntRect& start, const Platform::IntRect& end, const Platform::IntRectRegion&, bool overrideTouchHandling = false) = 0;
    virtual void cancelSelectionVisuals() = 0;
    virtual void notifySelectionHandlesReversed() = 0;
    virtual void notifyCaretChanged(const Platform::IntRect& caret, bool userTouchTriggered, bool singleLineInput = false, const Platform::IntRect& singleLineBoundingBox = Platform::IntRect()) = 0;

    virtual void cursorChanged(Platform::CursorType, const char* url, int x, int y) = 0;

    virtual void requestGeolocationPermission(Platform::GeoTrackerListener*, void* context, const char* origin, unsigned originLength) = 0;
    virtual void cancelGeolocationPermission(Platform::GeoTrackerListener*, void* context) = 0;
    virtual Platform::NetworkStreamFactory* networkStreamFactory() = 0;

    virtual void handleStringPattern(const unsigned short* pattern, unsigned length) = 0;
    virtual void handleExternalLink(const Platform::NetworkRequest&, const unsigned short* context, unsigned contextLength, bool isClientRedirect) = 0;

    virtual void resetBackForwardList(unsigned listSize, unsigned currentIndex) = 0;

    virtual void openPopupList(bool multiple, int size, const ScopeArray<WebString>& labels, const bool* enableds, const int* itemType, const bool* selecteds) = 0;
    virtual void openDateTimePopup(int type, const WebString& value, const WebString& min, const WebString& max, double step) = 0;
    virtual void openColorPopup(const WebString& value) = 0;

    virtual bool chooseFilenames(bool allowMultiple, const SharedArray<WebString>& acceptTypes, const SharedArray<WebString>& initialFiles, const WebString& capture, SharedArray<WebString>& chosenFiles) = 0;

    virtual void loadPluginForMimetype(int, int width, int height, const SharedArray<WebString>& paramNames, const SharedArray<WebString>& paramValues, const char* url) = 0;
    virtual void notifyPluginRectChanged(int, Platform::IntRect rectChanged) = 0;
    virtual void destroyPlugin(int) = 0;
    virtual void playMedia(int) = 0;
    virtual void pauseMedia(int) = 0;
    virtual float getTime(int) = 0;
    virtual void setTime(int, float) = 0;
    virtual void setVolume(int, float) = 0;
    virtual void setMuted(int, bool) = 0;

    virtual WebPage* createWindow(int x, int y, int width, int height, unsigned flags, const WebString& url, const WebString& windowName) = 0;

    virtual void scheduleCloseWindow() = 0;

    // Database interface.
    virtual unsigned long long databaseQuota(const unsigned short* origin, unsigned originLength, const unsigned short* databaseName, unsigned databaseNameLength, unsigned long long totalUsage, unsigned long long originUsage, unsigned long long estimatedSize) = 0;

    virtual void setIconForUrl(const char* originalPageUrl, const char* finalPageUrl, const char* iconUrl) = 0;
    virtual void setFavicon(const char* dataInBase64, const char* url) = 0;
    virtual void setLargeIcon(const char* iconUrl) = 0;
    virtual void setWebAppCapable() = 0;
    virtual void setSearchProviderDetails(const char* title, const char* documentUrl) = 0;
    virtual void setAlternateFeedDetails(const char* title, const char* feedUrl) = 0;

    virtual WebString getErrorPage(int errorCode, const char* errorMessage, const char* url) = 0;

    virtual void willDeferLoading() = 0;
    virtual void didResumeLoading() = 0;

    // Headers is a list of alternating key and value.
    virtual void setMetaHeaders(const ScopeArray<WebString>& headers, unsigned int headersSize) = 0;

    virtual void needMoreData() = 0;
    virtual void handleWebInspectorMessageToFrontend(int id, const char* message, int length) = 0;

    virtual bool hasPendingScrollOrZoomEvent() const = 0;
    virtual Platform::IntRect userInterfaceBlittedDestinationRect() const = 0;
    virtual Platform::IntRect userInterfaceBlittedVisibleContentsRect() const = 0;

    virtual void resetBitmapZoomScale(double scale) = 0;
    virtual void animateBlockZoom(const Platform::FloatPoint& finalPoint, double finalScale) = 0;

    virtual void setPreventsScreenIdleDimming(bool noDimming) = 0;
    virtual bool authenticationChallenge(const unsigned short* realm, unsigned int realmLength, WebString& username, WebString& password) = 0;
    virtual SaveCredentialType notifyShouldSaveCredential(bool isNew) = 0;
    virtual void syncProxyCredential(const WebString& username, const WebString& password) = 0;
    virtual void notifyPopupAutofillDialog(const std::vector<std::string>&, const Platform::IntRect&) = 0;
    virtual void notifyDismissAutofillDialog() = 0;

    virtual bool shouldPluginEnterFullScreen() = 0;
    virtual void didPluginEnterFullScreen() = 0;
    virtual void didPluginExitFullScreen() = 0;
    virtual void onPluginStartBackgroundPlay() = 0;
    virtual void onPluginStopBackgroundPlay() = 0;
    virtual bool lockOrientation(bool landscape) = 0;
    virtual void unlockOrientation() = 0;
    virtual bool isActive() const = 0;
    virtual bool isVisible() const = 0;
    virtual void requestWebGLPermission(const WebString&) = 0;

    virtual void setToolTip(WebString) = 0;
    virtual void setStatus(WebString) = 0;
    virtual bool acceptNavigationRequest(const Platform::NetworkRequest&, Platform::NavigationType) = 0;
    virtual void cursorEventModeChanged(Platform::CursorEventMode) = 0;
    virtual void touchEventModeChanged(Platform::TouchEventMode) = 0;

    virtual bool downloadAllowed(const char* url) = 0;
    virtual void downloadRequested(Platform::FilterStream*, const WebString& suggestedFilename) = 0;

    virtual int fullscreenStart() = 0;
    virtual int fullscreenStart(const char* contextName, Platform::Graphics::Window*, unsigned x, unsigned y, unsigned width, unsigned height) = 0;

    virtual int fullscreenStop() = 0;

    virtual int fullscreenWindowSet(unsigned x, unsigned y, unsigned width, unsigned height) = 0;

    virtual void drawVerticalScrollbar() = 0;
    virtual void drawHorizontalScrollbar() = 0;
    virtual void populateCustomHeaders(Platform::NetworkRequest&) = 0;

    virtual void notifyWillUpdateApplicationCache() = 0;
    virtual void notifyDidLoadFromApplicationCache() = 0;

    virtual void clearCookies() = 0;
    virtual void clearCache() = 0;

    virtual bool hasKeyboardFocus() = 0;
    virtual bool createPopupWebView(const Platform::IntRect&) = 0;
    virtual void closePopupWebView() = 0;

    // Match with ChromeClient::CustomHandlersState.
    enum ProtocolHandlersState {
        ProtocolHandlersNew,
        ProtocolHandlersRegistered,
        ProtocolHandlersDeclined
    };
    virtual void registerProtocolHandler(const WebString& /*scheme*/, const WebString& /*baseURL*/, const WebString& /*url*/, const WebString& /*title*/) = 0;
    virtual ProtocolHandlersState isProtocolHandlerRegistered(const WebString& /*scheme*/, const WebString& /*baseURL*/, const WebString& /*url*/) = 0;
    virtual void unregisterProtocolHandler(const WebString& /*scheme*/, const WebString& /*baseURL*/, const WebString& /*url*/) = 0;
};
} // namespace WebKit
} // namespace BlackBerry

#endif // WebPageClient_h
