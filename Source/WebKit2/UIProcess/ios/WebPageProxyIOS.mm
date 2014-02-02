/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebPageProxy.h"

#import "NativeWebKeyboardEvent.h"
#import "PageClient.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WebKitSystemInterfaceIOS.h"
#import "WebPageMessages.h"
#import "WebProcessProxy.h"
#import <WebCore/NotImplemented.h>
#import <WebCore/UserAgent.h>
#import <WebCore/SharedBuffer.h>

using namespace WebCore;

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

static String userVisibleWebKitVersionString()
{
    // If the version is longer than 3 digits then the leading digits represent the version of the OS. Our user agent
    // string should not include the leading digits, so strip them off and report the rest as the version. <rdar://problem/4997547>
    NSString *fullVersion = [[NSBundle bundleForClass:NSClassFromString(@"WKView")] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
    NSRange nonDigitRange = [fullVersion rangeOfCharacterFromSet:[[NSCharacterSet decimalDigitCharacterSet] invertedSet]];
    if (nonDigitRange.location == NSNotFound && fullVersion.length > 3)
        return [fullVersion substringFromIndex:fullVersion.length - 3];
    if (nonDigitRange.location != NSNotFound && nonDigitRange.location > 3)
        return [fullVersion substringFromIndex:nonDigitRange.location - 3];
    return fullVersion;
}

String WebPageProxy::standardUserAgent(const String& applicationName)
{
    return standardUserAgentWithApplicationName(applicationName, userVisibleWebKitVersionString());
}

void WebPageProxy::getIsSpeaking(bool&)
{
    notImplemented();
}

void WebPageProxy::speak(const String&)
{
    notImplemented();
}

void WebPageProxy::stopSpeaking()
{
    notImplemented();
}

void WebPageProxy::searchWithSpotlight(const String&)
{
    notImplemented();
}

void WebPageProxy::searchTheWeb(const String&)
{
    notImplemented();
}

void WebPageProxy::windowAndViewFramesChanged(const FloatRect&, const FloatPoint&)
{
    notImplemented();
}

void WebPageProxy::setComposition(const String& text, Vector<CompositionUnderline> underline, uint64_t selectionStart, uint64_t selectionEnd, uint64_t, uint64_t)
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::SetComposition(text, underline, selectionStart, selectionEnd), m_pageID);
}

void WebPageProxy::confirmComposition()
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::ConfirmComposition(), m_pageID);
}

void WebPageProxy::cancelComposition()
{
    notImplemented();

}

bool WebPageProxy::insertText(const String& text, uint64_t replacementRangeStart, uint64_t replacementRangeEnd)
{
    if (!isValid())
        return true;
    
    process().send(Messages::WebPage::InsertText(text, replacementRangeStart, replacementRangeEnd), m_pageID);
    return true;
}

bool WebPageProxy::insertDictatedText(const String&, uint64_t, uint64_t, const Vector<WebCore::TextAlternativeWithRange>&)
{
    notImplemented();
    return false;
}

void WebPageProxy::getMarkedRange(uint64_t&, uint64_t&)
{
    notImplemented();
}

void WebPageProxy::getSelectedRange(uint64_t&, uint64_t&)
{
    notImplemented();
}

void WebPageProxy::getAttributedSubstringFromRange(uint64_t, uint64_t, AttributedString&)
{
    notImplemented();
}

uint64_t WebPageProxy::characterIndexForPoint(const IntPoint)
{
    notImplemented();
    return 0;
}

IntRect WebPageProxy::firstRectForCharacterRange(uint64_t, uint64_t)
{
    notImplemented();
    return IntRect();
}

bool WebPageProxy::executeKeypressCommands(const Vector<WebCore::KeypressCommand>&)
{
    notImplemented();
    return false;
}

String WebPageProxy::stringSelectionForPasteboard()
{
    notImplemented();
    return String();
}

PassRefPtr<WebCore::SharedBuffer> WebPageProxy::dataSelectionForPasteboard(const String&)
{
    notImplemented();
    return 0;
}

bool WebPageProxy::readSelectionFromPasteboard(const String&)
{
    notImplemented();
    return false;
}

void WebPageProxy::performDictionaryLookupAtLocation(const WebCore::FloatPoint&)
{
    notImplemented();
}

void WebPageProxy::gestureCallback(const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, uint64_t callbackID)
{
    RefPtr<GestureCallback> callback = m_gestureCallbacks.take(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    callback->performCallbackWithReturnValue(point, gestureType, gestureState, flags);
}

void WebPageProxy::touchesCallback(const WebCore::IntPoint& point, uint32_t touches, uint64_t callbackID)
{
    RefPtr<TouchesCallback> callback = m_touchesCallbacks.take(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(point, touches);
}

void WebPageProxy::autocorrectionDataCallback(const Vector<WebCore::FloatRect>& rects, const String& fontName, float fontSize, uint64_t fontTraits, uint64_t callbackID)
{
    RefPtr<AutocorrectionDataCallback> callback = m_autocorrectionCallbacks.take(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(rects, fontName, fontSize, fontTraits);
}

void WebPageProxy::autocorrectionContextCallback(const String& beforeText, const String& markedText, const String& selectedText, const String& afterText, uint64_t location, uint64_t length, uint64_t callbackID)
{
    RefPtr<AutocorrectionContextCallback> callback = m_autocorrectionContextCallbacks.take(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(beforeText, markedText, selectedText, afterText, location, length);
}

void WebPageProxy::selectWithGesture(const WebCore::IntPoint point, WebCore::TextGranularity granularity, uint32_t gestureType, uint32_t gestureState, PassRefPtr<GestureCallback> callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_gestureCallbacks.set(callbackID, callback);
    m_process->send(Messages::WebPage::SelectWithGesture(point, (uint32_t)granularity, gestureType, gestureState, callbackID), m_pageID);
}

void WebPageProxy::updateSelectionWithTouches(const WebCore::IntPoint point, uint32_t touches, bool baseIsStart, PassRefPtr<TouchesCallback> callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_touchesCallbacks.set(callbackID, callback);
    m_process->send(Messages::WebPage::UpdateSelectionWithTouches(point, touches, baseIsStart, callbackID), m_pageID);
}

void WebPageProxy::requestAutocorrectionData(const String& textForAutocorrection, PassRefPtr<AutocorrectionDataCallback> callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_autocorrectionCallbacks.set(callbackID, callback);
    m_process->send(Messages::WebPage::RequestAutocorrectionData(textForAutocorrection, callbackID), m_pageID);
}

void WebPageProxy::applyAutocorrection(const String& correction, const String& originalText, PassRefPtr<StringCallback> callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_stringCallbacks.set(callbackID, callback);
    m_process->send(Messages::WebPage::ApplyAutocorrection(correction, originalText, callbackID), m_pageID);
}

void WebPageProxy::requestAutocorrectionContext(PassRefPtr<AutocorrectionContextCallback> callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_autocorrectionContextCallbacks.set(callbackID, callback);
    m_process->send(Messages::WebPage::RequestAutocorrectionContext(callbackID), m_pageID);
}

void WebPageProxy::getAutocorrectionContext(String& beforeContext, String& markedText, String& selectedText, String& afterContext, uint64_t& location, uint64_t& length)
{
    m_process->sendSync(Messages::WebPage::GetAutocorrectionContext(), Messages::WebPage::GetAutocorrectionContext::Reply(beforeContext, markedText, selectedText, afterContext, location, length), m_pageID);
}

void WebPageProxy::selectWithTwoTouches(const WebCore::IntPoint from, const WebCore::IntPoint to, uint32_t gestureType, uint32_t gestureState, PassRefPtr<GestureCallback> callback)
{
    if (!isValid()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_gestureCallbacks.set(callbackID, callback);
    m_process->send(Messages::WebPage::SelectWithTwoTouches(from, to, gestureType, gestureState, callbackID), m_pageID);
}

void WebPageProxy::didReceivePositionInformation(const InteractionInformationAtPosition& info)
{
    m_pageClient.positionInformationDidChange(info);
}

void WebPageProxy::getPositionInformation(const WebCore::IntPoint& point, InteractionInformationAtPosition& info)
{
    m_process->sendSync(Messages::WebPage::GetPositionInformation(point), Messages::WebPage::GetPositionInformation::Reply(info), m_pageID);
}

void WebPageProxy::requestPositionInformation(const WebCore::IntPoint& point)
{
    m_process->send(Messages::WebPage::RequestPositionInformation(point), m_pageID);
}

void WebPageProxy::startInteractionWithElementAtPosition(const WebCore::IntPoint& point)
{
    m_process->send(Messages::WebPage::StartInteractionWithElementAtPosition(point), m_pageID);
}

void WebPageProxy::stopInteraction()
{
    m_process->send(Messages::WebPage::StopInteraction(), m_pageID);
}

void WebPageProxy::performActionOnElement(uint32_t action)
{
    m_process->send(Messages::WebPage::PerformActionOnElement(action), m_pageID);
}

void WebPageProxy::saveImageToLibrary(const SharedMemory::Handle& imageHandle, uint64_t imageSize)
{
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::create(imageHandle, SharedMemory::ReadOnly);
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryBuffer->data()), imageSize);
    m_pageClient.saveImageToLibrary(buffer);
}

void WebPageProxy::notifyRevealedSelection()
{
    m_pageClient.selectionDidChange();
}

void WebPageProxy::extendSelection(WebCore::TextGranularity granularity)
{
    m_process->send(Messages::WebPage::ExtendSelection(static_cast<uint32_t>(granularity)), m_pageID);
}

void WebPageProxy::interpretQueuedKeyEvent(const EditorState&, bool&, Vector<WebCore::KeypressCommand>&)
{
    notImplemented();
}

void WebPageProxy::interpretKeyEvent(const EditorState& state, bool isCharEvent, bool& handled)
{
    m_editorState = state;
    handled = m_pageClient.interpretKeyEvent(m_keyEventQueue.first(), isCharEvent);
}

// Complex text input support for plug-ins.
void WebPageProxy::sendComplexTextInputToPlugin(uint64_t, const String&)
{
    notImplemented();
}

void WebPageProxy::setSmartInsertDeleteEnabled(bool)
{
    notImplemented();
}

void WebPageProxy::registerWebProcessAccessibilityToken(const IPC::DataReference&)
{
    notImplemented();
}    

void WebPageProxy::makeFirstResponder()
{
    notImplemented();
}

void WebPageProxy::registerUIProcessAccessibilityTokens(const IPC::DataReference&, const IPC::DataReference&)
{
    notImplemented();
}

void WebPageProxy::pluginFocusOrWindowFocusChanged(uint64_t, bool)
{
    notImplemented();
}

void WebPageProxy::setPluginComplexTextInputState(uint64_t, uint64_t)
{
    notImplemented();
}

void WebPageProxy::executeSavedCommandBySelector(const String&, bool&)
{
    notImplemented();
}

bool WebPageProxy::shouldDelayWindowOrderingForEvent(const WebKit::WebMouseEvent&)
{
    notImplemented();
    return false;
}

bool WebPageProxy::acceptsFirstMouse(int, const WebKit::WebMouseEvent&)
{
    notImplemented();
    return false;
}

void WebPageProxy::didFinishScrolling(const WebCore::FloatPoint& contentOffset)
{
    process().send(Messages::WebPage::DidFinishScrolling(contentOffset), m_pageID);
}

void WebPageProxy::didFinishZooming(float newScale)
{
    m_pageScaleFactor = newScale;
    process().send(Messages::WebPage::DidFinishZooming(newScale), m_pageID);
}

void WebPageProxy::tapHighlightAtPosition(const WebCore::FloatPoint& position, uint64_t& requestID)
{
    static uint64_t uniqueRequestID = 0;
    requestID = ++uniqueRequestID;

    process().send(Messages::WebPage::TapHighlightAtPosition(requestID, position), m_pageID);
}

void WebPageProxy::blurAssistedNode()
{
    process().send(Messages::WebPage::BlurAssistedNode(), m_pageID);
}

void WebPageProxy::mainDocumentDidReceiveMobileDocType()
{
    m_pageClient.mainDocumentDidReceiveMobileDocType();
}

void WebPageProxy::didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius)
{
    m_pageClient.didGetTapHighlightGeometries(requestID, color, highlightedQuads, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
}

void WebPageProxy::didChangeViewportArguments(const WebCore::ViewportArguments& viewportArguments)
{
    m_pageClient.didChangeViewportArguments(viewportArguments);
}

void WebPageProxy::startAssistingNode(const WebCore::IntRect& scrollRect, bool hasNextFocusable, bool hasPreviousFocusable)
{
    m_pageClient.startAssistingNode(scrollRect, hasNextFocusable, hasPreviousFocusable);
}

void WebPageProxy::stopAssistingNode()
{
    m_pageClient.stopAssistingNode();
}

void WebPageProxy::didPerformDictionaryLookup(const AttributedString&, const DictionaryPopupInfo&)
{
    notImplemented();
}

void WebPageProxy::savePDFToTemporaryFolderAndOpenWithNativeApplication(const String&, const String&, const IPC::DataReference&, const String&)
{
    notImplemented();
}

void WebPageProxy::savePDFToTemporaryFolderAndOpenWithNativeApplicationRaw(const String&, const String&, const uint8_t*, unsigned long, const String&)
{
    notImplemented();
}

void WebPageProxy::openPDFFromTemporaryFolderWithNativeApplication(const String&)
{
    notImplemented();
}

void WebPageProxy::setAcceleratedCompositingRootLayer(PlatformLayer* rootLayer)
{
    m_pageClient.setAcceleratedCompositingRootLayer(rootLayer);
}

} // namespace WebKit
