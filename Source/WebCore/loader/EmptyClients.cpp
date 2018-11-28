/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "EmptyClients.h"

#include "ApplicationCacheStorage.h"
#include "BackForwardClient.h"
#include "CacheStorageProvider.h"
#include "ColorChooser.h"
#include "ContextMenuClient.h"
#include "DataListSuggestionPicker.h"
#include "DatabaseProvider.h"
#include "DiagnosticLoggingClient.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DragClient.h"
#include "EditorClient.h"
#include "EmptyFrameLoaderClient.h"
#include "FileChooser.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "FrameNetworkingContext.h"
#include "HTMLFormElement.h"
#include "HistoryItem.h"
#include "InProcessIDBServer.h"
#include "InspectorClient.h"
#include "LibWebRTCProvider.h"
#include "NetworkStorageSession.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PaymentCoordinatorClient.h"
#include "PluginInfoProvider.h"
#include "ProgressTrackerClient.h"
#include "SecurityOriginData.h"
#include "SocketProvider.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "StorageType.h"
#include "TextCheckerClient.h"
#include "ThreadableWebSocketChannel.h"
#include "UserContentProvider.h"
#include "VisitedLinkStore.h"
#include <JavaScriptCore/HeapInlines.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "CompiledContentExtension.h"
#endif

#if USE(QUICK_LOOK)
#include "PreviewLoaderClient.h"
#endif

namespace WebCore {

class UserMessageHandlerDescriptor;

class EmptyBackForwardClient final : public BackForwardClient {
    void addItem(Ref<HistoryItem>&&) final { }
    void goToItem(HistoryItem&) final { }
    RefPtr<HistoryItem> itemAtIndex(int) final { return nullptr; }
    unsigned backListCount() const final { return 0; }
    unsigned forwardListCount() const final { return 0; }
    void close() final { }
};

#if ENABLE(CONTEXT_MENUS)

class EmptyContextMenuClient final : public ContextMenuClient {
    void contextMenuDestroyed() final { }

    void downloadURL(const URL&) final { }
    void searchWithGoogle(const Frame*) final { }
    void lookUpInDictionary(Frame*) final { }
    bool isSpeaking() final { return false; }
    void speak(const String&) final { }
    void stopSpeaking() final { }

#if PLATFORM(COCOA)
    void searchWithSpotlight() final { }
#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    void showContextMenu() final { }
#endif
};

#endif // ENABLE(CONTEXT_MENUS)

class EmptyDatabaseProvider final : public DatabaseProvider {
#if ENABLE(INDEXED_DATABASE)
    IDBClient::IDBConnectionToServer& idbConnectionToServerForSession(const PAL::SessionID&) final
    {
        static auto& sharedConnection = InProcessIDBServer::create().leakRef();
        return sharedConnection.connectionToServer();
    }
#endif
};

class EmptyDiagnosticLoggingClient final : public DiagnosticLoggingClient {
    void logDiagnosticMessage(const String&, const String&, ShouldSample) final { }
    void logDiagnosticMessageWithResult(const String&, const String&, DiagnosticLoggingResultType, ShouldSample) final { }
    void logDiagnosticMessageWithValue(const String&, const String&, double, unsigned, ShouldSample) final { }
    void logDiagnosticMessageWithEnhancedPrivacy(const String&, const String&, ShouldSample) final { }
};

#if ENABLE(DRAG_SUPPORT)

class EmptyDragClient final : public DragClient {
    void willPerformDragDestinationAction(DragDestinationAction, const DragData&) final { }
    void willPerformDragSourceAction(DragSourceAction, const IntPoint&, DataTransfer&) final { }
    DragSourceAction dragSourceActionMaskForPoint(const IntPoint&) final { return DragSourceActionNone; }
    void startDrag(DragItem, DataTransfer&, Frame&) final { }
    void dragControllerDestroyed() final { }
};

#endif // ENABLE(DRAG_SUPPORT)

class EmptyEditorClient final : public EditorClient {
    WTF_MAKE_FAST_ALLOCATED;

public:
    EmptyEditorClient() = default;

private:
    bool shouldDeleteRange(Range*) final { return false; }
    bool smartInsertDeleteEnabled() final { return false; }
    bool isSelectTrailingWhitespaceEnabled() const final { return false; }
    bool isContinuousSpellCheckingEnabled() final { return false; }
    void toggleContinuousSpellChecking() final { }
    bool isGrammarCheckingEnabled() final { return false; }
    void toggleGrammarChecking() final { }
    int spellCheckerDocumentTag() final { return -1; }

    bool shouldBeginEditing(Range*) final { return false; }
    bool shouldEndEditing(Range*) final { return false; }
    bool shouldInsertNode(Node*, Range*, EditorInsertAction) final { return false; }
    bool shouldInsertText(const String&, Range*, EditorInsertAction) final { return false; }
    bool shouldChangeSelectedRange(Range*, Range*, EAffinity, bool) final { return false; }

    bool shouldApplyStyle(StyleProperties*, Range*) final { return false; }
    void didApplyStyle() final { }
    bool shouldMoveRangeAfterDelete(Range*, Range*) final { return false; }

    void didBeginEditing() final { }
    void respondToChangedContents() final { }
    void respondToChangedSelection(Frame*) final { }
    void updateEditorStateAfterLayoutIfEditabilityChanged() final { }
    void discardedComposition(Frame*) final { }
    void canceledComposition() final { }
    void didUpdateComposition() final { }
    void didEndEditing() final { }
    void didEndUserTriggeredSelectionChanges() final { }
    void willWriteSelectionToPasteboard(Range*) final { }
    void didWriteSelectionToPasteboard() final { }
    void getClientPasteboardDataForRange(Range*, Vector<String>&, Vector<RefPtr<SharedBuffer>>&) final { }
    String replacementURLForResource(Ref<SharedBuffer>&&, const String&) final { return { }; }
    void requestCandidatesForSelection(const VisibleSelection&) final { }
    void handleAcceptedCandidateWithSoftSpaces(TextCheckingResult) final { }

    void registerUndoStep(UndoStep&) final;
    void registerRedoStep(UndoStep&) final;
    void clearUndoRedoOperations() final { }

    bool canCopyCut(Frame*, bool defaultValue) const final { return defaultValue; }
    bool canPaste(Frame*, bool defaultValue) const final { return defaultValue; }
    bool canUndo() const final { return false; }
    bool canRedo() const final { return false; }

    void undo() final { }
    void redo() final { }

    void handleKeyboardEvent(KeyboardEvent*) final { }
    void handleInputMethodKeydown(KeyboardEvent*) final { }

    void textFieldDidBeginEditing(Element*) final { }
    void textFieldDidEndEditing(Element*) final { }
    void textDidChangeInTextField(Element*) final { }
    bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) final { return false; }
    void textWillBeDeletedInTextField(Element*) final { }
    void textDidChangeInTextArea(Element*) final { }
    void overflowScrollPositionChanged() final { }

#if PLATFORM(IOS_FAMILY)
    void startDelayingAndCoalescingContentChangeNotifications() final { }
    void stopDelayingAndCoalescingContentChangeNotifications() final { }
    bool hasRichlyEditableSelection() final { return false; }
    int getPasteboardItemsCount() final { return 0; }
    RefPtr<DocumentFragment> documentFragmentFromDelegate(int) final { return nullptr; }
    bool performsTwoStepPaste(DocumentFragment*) final { return false; }
#endif

    bool performTwoStepDrop(DocumentFragment&, Range&, bool) final { return false; }

#if PLATFORM(COCOA)
    void setInsertionPasteboard(const String&) final { };
#endif

#if USE(APPKIT)
    void uppercaseWord() final { }
    void lowercaseWord() final { }
    void capitalizeWord() final { }
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void showSubstitutionsPanel(bool) final { }
    bool substitutionsPanelIsShowing() final { return false; }
    void toggleSmartInsertDelete() final { }
    bool isAutomaticQuoteSubstitutionEnabled() final { return false; }
    void toggleAutomaticQuoteSubstitution() final { }
    bool isAutomaticLinkDetectionEnabled() final { return false; }
    void toggleAutomaticLinkDetection() final { }
    bool isAutomaticDashSubstitutionEnabled() final { return false; }
    void toggleAutomaticDashSubstitution() final { }
    bool isAutomaticTextReplacementEnabled() final { return false; }
    void toggleAutomaticTextReplacement() final { }
    bool isAutomaticSpellingCorrectionEnabled() final { return false; }
    void toggleAutomaticSpellingCorrection() final { }
#endif

#if PLATFORM(GTK)
    bool shouldShowUnicodeMenu() final { return false; }
#endif

    TextCheckerClient* textChecker() final { return &m_textCheckerClient; }

    void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) final { }
    void updateSpellingUIWithMisspelledWord(const String&) final { }
    void showSpellingUI(bool) final { }
    bool spellingUIIsShowing() final { return false; }

    void willSetInputMethodState() final { }
    void setInputMethodState(bool) final { }

    class EmptyTextCheckerClient final : public TextCheckerClient {
        bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const final { return true; }
        void ignoreWordInSpellDocument(const String&) final { }
        void learnWord(const String&) final { }
        void checkSpellingOfString(StringView, int*, int*) final { }
        String getAutoCorrectSuggestionForMisspelledWord(const String&) final { return { }; }
        void checkGrammarOfString(StringView, Vector<GrammarDetail>&, int*, int*) final { }

#if USE(UNIFIED_TEXT_CHECKING)
        Vector<TextCheckingResult> checkTextOfParagraph(StringView, OptionSet<TextCheckingType>, const VisibleSelection&) final { return Vector<TextCheckingResult>(); }
#endif

        void getGuessesForWord(const String&, const String&, const VisibleSelection&, Vector<String>&) final { }
        void requestCheckingOfString(TextCheckingRequest&, const VisibleSelection&) final;
    };

    EmptyTextCheckerClient m_textCheckerClient;
};

class EmptyFrameNetworkingContext final : public FrameNetworkingContext {
public:
    static Ref<EmptyFrameNetworkingContext> create() { return adoptRef(*new EmptyFrameNetworkingContext); }

private:
    EmptyFrameNetworkingContext();

    bool shouldClearReferrerOnHTTPSToHTTPRedirect() const { return true; }
    NetworkStorageSession& storageSession() const final { return NetworkStorageSession::defaultStorageSession(); }

#if PLATFORM(COCOA)
    bool localFileContentSniffingEnabled() const { return false; }
    SchedulePairHashSet* scheduledRunLoopPairs() const { return nullptr; }
    RetainPtr<CFDataRef> sourceApplicationAuditData() const { return nullptr; };
#endif

#if PLATFORM(COCOA) || PLATFORM(WIN)
    ResourceError blockedError(const ResourceRequest&) const final { return { }; }
#endif
};

class EmptyInspectorClient final : public InspectorClient {
    void inspectedPageDestroyed() final { }
    Inspector::FrontendChannel* openLocalFrontend(InspectorController*) final { return nullptr; }
    void bringFrontendToFront() final { }
    void highlight() final { }
    void hideHighlight() final { }
};

#if ENABLE(APPLE_PAY)

class EmptyPaymentCoordinatorClient final : public PaymentCoordinatorClient {
    bool supportsVersion(unsigned) final { return false; }
    std::optional<String> validatedPaymentNetwork(const String&) final { return std::nullopt; }
    bool canMakePayments() final { return false; }
    void canMakePaymentsWithActiveCard(const String&, const String&, WTF::Function<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTFMove(completionHandler)] { completionHandler(false); }); }
    void openPaymentSetup(const String&, const String&, WTF::Function<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTFMove(completionHandler)] { completionHandler(false); }); }
    bool showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest&) final { return false; }
    void completeMerchantValidation(const PaymentMerchantSession&) final { }
    void completeShippingMethodSelection(std::optional<ShippingMethodUpdate>&&) final { }
    void completeShippingContactSelection(std::optional<ShippingContactUpdate>&&) final { }
    void completePaymentMethodSelection(std::optional<PaymentMethodUpdate>&&) final { }
    void completePaymentSession(std::optional<PaymentAuthorizationResult>&&) final { }
    void cancelPaymentSession() final { }
    void abortPaymentSession() final { }
    void paymentCoordinatorDestroyed() final { }
};

#endif

class EmptyPluginInfoProvider final : public PluginInfoProvider {
    void refreshPlugins() final { };
    Vector<PluginInfo> pluginInfo(Page&, std::optional<Vector<SupportedPluginIdentifier>>&) final { return { }; }
    Vector<PluginInfo> webVisiblePluginInfo(Page&, const URL&) final { return { }; }
};

class EmptyPopupMenu : public PopupMenu {
public:
    EmptyPopupMenu() = default;
private:
    void show(const IntRect&, FrameView*, int) final { }
    void hide() final { }
    void updateFromElement() final { }
    void disconnectClient() final { }
};

class EmptyProgressTrackerClient final : public ProgressTrackerClient {
    void willChangeEstimatedProgress() final { }
    void didChangeEstimatedProgress() final { }
    void progressStarted(Frame&) final { }
    void progressEstimateChanged(Frame&) final { }
    void progressFinished(Frame&) final { }
};

class EmptySearchPopupMenu : public SearchPopupMenu {
public:
    EmptySearchPopupMenu()
        : m_popup(adoptRef(*new EmptyPopupMenu))
    {
    }

private:
    PopupMenu* popupMenu() final { return m_popup.ptr(); }
    void saveRecentSearches(const AtomicString&, const Vector<RecentSearch>&) final { }
    void loadRecentSearches(const AtomicString&, Vector<RecentSearch>&) final { }
    bool enabled() final { return false; }

    Ref<EmptyPopupMenu> m_popup;
};

class EmptyStorageNamespaceProvider final : public StorageNamespaceProvider {
    struct EmptyStorageArea : public StorageArea {
        unsigned length() final { return 0; }
        String key(unsigned) final { return { }; }
        String item(const String&) final { return { }; }
        void setItem(Frame*, const String&, const String&, bool&) final { }
        void removeItem(Frame*, const String&) final { }
        void clear(Frame*) final { }
        bool contains(const String&) final { return false; }
        StorageType storageType() const final { return StorageType::Local; }
        size_t memoryBytesUsedByCache() final { return 0; }
        const SecurityOriginData& securityOrigin() const final { static NeverDestroyed<SecurityOriginData> origin; return origin.get(); }
    };

    struct EmptyStorageNamespace final : public StorageNamespace {
        RefPtr<StorageArea> storageArea(const SecurityOriginData&) final { return adoptRef(*new EmptyStorageArea); }
        RefPtr<StorageNamespace> copy(Page*) final { return adoptRef(*new EmptyStorageNamespace); }
    };

    RefPtr<StorageNamespace> createSessionStorageNamespace(Page&, unsigned) final;
    RefPtr<StorageNamespace> createLocalStorageNamespace(unsigned) final;
    RefPtr<StorageNamespace> createEphemeralLocalStorageNamespace(Page&, unsigned) final;
    RefPtr<StorageNamespace> createTransientLocalStorageNamespace(SecurityOrigin&, unsigned) final;
};

class EmptyUserContentProvider final : public UserContentProvider {
    void forEachUserScript(Function<void(DOMWrapperWorld&, const UserScript&)>&&) const final { }
    void forEachUserStyleSheet(Function<void(const UserStyleSheet&)>&&) const final { }
#if ENABLE(USER_MESSAGE_HANDLERS)
    void forEachUserMessageHandler(Function<void(const UserMessageHandlerDescriptor&)>&&) const final { }
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    ContentExtensions::ContentExtensionsBackend& userContentExtensionBackend() final { static NeverDestroyed<ContentExtensions::ContentExtensionsBackend> backend; return backend.get(); };
#endif
};

class EmptyVisitedLinkStore final : public VisitedLinkStore {
    bool isLinkVisited(Page&, SharedStringHash, const URL&, const AtomicString&) final { return false; }
    void addVisitedLink(Page&, SharedStringHash) final { }
};

RefPtr<PopupMenu> EmptyChromeClient::createPopupMenu(PopupMenuClient&) const
{
    return adoptRef(*new EmptyPopupMenu);
}

RefPtr<SearchPopupMenu> EmptyChromeClient::createSearchPopupMenu(PopupMenuClient&) const
{
    return adoptRef(*new EmptySearchPopupMenu);
}

#if ENABLE(INPUT_TYPE_COLOR)

std::unique_ptr<ColorChooser> EmptyChromeClient::createColorChooser(ColorChooserClient&, const Color&)
{
    return nullptr;
}

#endif

#if ENABLE(DATALIST_ELEMENT)

std::unique_ptr<DataListSuggestionPicker> EmptyChromeClient::createDataListSuggestionPicker(DataListSuggestionsClient&)
{
    return nullptr;
}

#endif

void EmptyChromeClient::runOpenPanel(Frame&, FileChooser&)
{
}
    
void EmptyChromeClient::showShareSheet(ShareDataWithParsedURL&, CompletionHandler<void(bool)>&&)
{
}

PAL::SessionID EmptyFrameLoaderClient::sessionID() const
{
    return PAL::SessionID::defaultSessionID();
}

void EmptyFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String&, FramePolicyFunction&&)
{
}

void EmptyFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse&, FormState*, PolicyDecisionMode, FramePolicyFunction&&)
{
}

void EmptyFrameLoaderClient::dispatchWillSendSubmitEvent(Ref<FormState>&&)
{
}

void EmptyFrameLoaderClient::dispatchWillSubmitForm(FormState&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

Ref<DocumentLoader> EmptyFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

RefPtr<Frame> EmptyFrameLoaderClient::createFrame(const URL&, const String&, HTMLFrameOwnerElement&, const String&)
{
    return nullptr;
}

RefPtr<Widget> EmptyFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement&, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    return nullptr;
}

void EmptyFrameLoaderClient::recreatePlugin(Widget*)
{
}

RefPtr<Widget> EmptyFrameLoaderClient::createJavaAppletWidget(const IntSize&, HTMLAppletElement&, const URL&, const Vector<String>&, const Vector<String>&)
{
    return nullptr;
}

inline EmptyFrameNetworkingContext::EmptyFrameNetworkingContext()
    : FrameNetworkingContext { nullptr }
{
}

Ref<FrameNetworkingContext> EmptyFrameLoaderClient::createNetworkingContext()
{
    return EmptyFrameNetworkingContext::create();
}

void EmptyEditorClient::EmptyTextCheckerClient::requestCheckingOfString(TextCheckingRequest&, const VisibleSelection&)
{
}

void EmptyEditorClient::registerUndoStep(UndoStep&)
{
}

void EmptyEditorClient::registerRedoStep(UndoStep&)
{
}

RefPtr<StorageNamespace> EmptyStorageNamespaceProvider::createSessionStorageNamespace(Page&, unsigned)
{
    return adoptRef(*new EmptyStorageNamespace);
}

RefPtr<StorageNamespace> EmptyStorageNamespaceProvider::createLocalStorageNamespace(unsigned)
{
    return adoptRef(*new EmptyStorageNamespace);
}

RefPtr<StorageNamespace> EmptyStorageNamespaceProvider::createEphemeralLocalStorageNamespace(Page&, unsigned)
{
    return adoptRef(*new EmptyStorageNamespace);
}

RefPtr<StorageNamespace> EmptyStorageNamespaceProvider::createTransientLocalStorageNamespace(SecurityOrigin&, unsigned)
{
    return adoptRef(*new EmptyStorageNamespace);
}

PageConfiguration pageConfigurationWithEmptyClients()
{
    PageConfiguration pageConfiguration {
        makeUniqueRef<EmptyEditorClient>(),
        SocketProvider::create(),
        LibWebRTCProvider::create(),
        CacheStorageProvider::create(),
        adoptRef(*new EmptyBackForwardClient)
    };

    static NeverDestroyed<EmptyChromeClient> dummyChromeClient;
    pageConfiguration.chromeClient = &dummyChromeClient.get();

#if ENABLE(APPLE_PAY)
    static NeverDestroyed<EmptyPaymentCoordinatorClient> dummyPaymentCoordinatorClient;
    pageConfiguration.paymentCoordinatorClient = &dummyPaymentCoordinatorClient.get();
#endif

#if ENABLE(CONTEXT_MENUS)
    static NeverDestroyed<EmptyContextMenuClient> dummyContextMenuClient;
    pageConfiguration.contextMenuClient = &dummyContextMenuClient.get();
#endif

#if ENABLE(DRAG_SUPPORT)
    static NeverDestroyed<EmptyDragClient> dummyDragClient;
    pageConfiguration.dragClient = &dummyDragClient.get();
#endif

    static NeverDestroyed<EmptyInspectorClient> dummyInspectorClient;
    pageConfiguration.inspectorClient = &dummyInspectorClient.get();

    static NeverDestroyed<EmptyFrameLoaderClient> dummyFrameLoaderClient;
    pageConfiguration.loaderClientForMainFrame = &dummyFrameLoaderClient.get();

    static NeverDestroyed<EmptyProgressTrackerClient> dummyProgressTrackerClient;
    pageConfiguration.progressTrackerClient = &dummyProgressTrackerClient.get();

    pageConfiguration.diagnosticLoggingClient = std::make_unique<EmptyDiagnosticLoggingClient>();

    pageConfiguration.applicationCacheStorage = ApplicationCacheStorage::create({ }, { });
    pageConfiguration.databaseProvider = adoptRef(*new EmptyDatabaseProvider);
    pageConfiguration.pluginInfoProvider = adoptRef(*new EmptyPluginInfoProvider);
    pageConfiguration.storageNamespaceProvider = adoptRef(*new EmptyStorageNamespaceProvider);
    pageConfiguration.userContentProvider = adoptRef(*new EmptyUserContentProvider);
    pageConfiguration.visitedLinkStore = adoptRef(*new EmptyVisitedLinkStore);
    
    return pageConfiguration;
}

DiagnosticLoggingClient& emptyDiagnosticLoggingClient()
{
    static NeverDestroyed<EmptyDiagnosticLoggingClient> client;
    return client;
}

}
