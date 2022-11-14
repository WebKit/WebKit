/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "AppHighlight.h"
#include "ApplicationCacheStorage.h"
#include "BackForwardClient.h"
#include "BroadcastChannelRegistry.h"
#include "CacheStorageProvider.h"
#include "ColorChooser.h"
#include "ContextMenuClient.h"
#include "CookieConsentDecisionResult.h"
#include "CookieJar.h"
#include "DOMPasteAccess.h"
#include "DataListSuggestionPicker.h"
#include "DatabaseProvider.h"
#include "DiagnosticLoggingClient.h"
#include "DisplayRefreshMonitorFactory.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DragClient.h"
#include "DummyModelPlayerProvider.h"
#include "DummySpeechRecognitionProvider.h"
#include "DummyStorageProvider.h"
#include "EditorClient.h"
#include "EmptyAttachmentElementClient.h"
#include "EmptyFrameLoaderClient.h"
#include "FileChooser.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "FrameNetworkingContext.h"
#include "HTMLFormElement.h"
#include "HistoryItem.h"
#include "IDBConnectionToServer.h"
#include "InspectorClient.h"
#include "MediaRecorderPrivate.h"
#include "MediaRecorderProvider.h"
#include "ModalContainerTypes.h"
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
#include "WebRTCProvider.h"
#include <JavaScriptCore/HeapInlines.h>
#include <pal/SessionID.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "CompiledContentExtension.h"
#endif

#if USE(QUICK_LOOK)
#include "LegacyPreviewLoaderClient.h"
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
#include "DateTimeChooser.h"
#endif

namespace WebCore {

class UserMessageHandlerDescriptor;

class EmptyBackForwardClient final : public BackForwardClient {
    void addItem(Ref<HistoryItem>&&) final { }
    void goToItem(HistoryItem&) final { }
    RefPtr<HistoryItem> itemAtIndex(int) final { return nullptr; }
    unsigned backListCount() const final { return 0; }
    unsigned forwardListCount() const final { return 0; }
    bool containsItem(const HistoryItem&) const final { return false; }
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

#if HAVE(TRANSLATION_UI_SERVICES)
    void handleTranslation(const TranslationContextMenuInfo&) final { }
#endif

#if PLATFORM(GTK)
    void insertEmoji(Frame&) final { }
#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    void showContextMenu() final { }
#endif

#if ENABLE(IMAGE_ANALYSIS)
    bool supportsLookUpInImages() final { return false; }
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    bool supportsCopySubject() final { return false; }
#endif
};

#endif // ENABLE(CONTEXT_MENUS)

class EmptyDisplayRefreshMonitor final : public DisplayRefreshMonitor {
public:
    static Ref<EmptyDisplayRefreshMonitor> create(PlatformDisplayID displayID)
    {
        return adoptRef(*new EmptyDisplayRefreshMonitor(displayID));
    }

    void displayLinkFired(const DisplayUpdate&) final { }
    bool requestRefreshCallback() final { return false; }
    void stop() final { }

    bool startNotificationMechanism() final { return true; }
    void stopNotificationMechanism() final { }

private:
    explicit EmptyDisplayRefreshMonitor(PlatformDisplayID displayID)
        : DisplayRefreshMonitor(displayID)
    {
    }
};

class EmptyDisplayRefreshMonitorFactory final : public DisplayRefreshMonitorFactory {
public:
    static DisplayRefreshMonitorFactory* sharedEmptyDisplayRefreshMonitorFactory()
    {
        static NeverDestroyed<EmptyDisplayRefreshMonitorFactory> emptyFactory;
        return &emptyFactory.get();
    }

private:
    RefPtr<DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID displayID) final
    {
        return EmptyDisplayRefreshMonitor::create(displayID);
    }
};

class EmptyDatabaseProvider final : public DatabaseProvider {
    struct EmptyIDBConnectionToServerDeletegate final : public IDBClient::IDBConnectionToServerDelegate {
        IDBConnectionIdentifier identifier() const final { return { }; }
        void deleteDatabase(const IDBRequestData&) final { }
        void openDatabase(const IDBRequestData&) final { }
        void abortTransaction(const IDBResourceIdentifier&) final { }
        void commitTransaction(const IDBResourceIdentifier&, uint64_t) final { }
        void didFinishHandlingVersionChangeTransaction(uint64_t, const IDBResourceIdentifier&) final { }
        void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&) final { }
        void deleteObjectStore(const IDBRequestData&, const String&) final { }
        void renameObjectStore(const IDBRequestData&, uint64_t, const String&) final { }
        void clearObjectStore(const IDBRequestData&, uint64_t) final { }
        void createIndex(const IDBRequestData&, const IDBIndexInfo&) final { }
        void deleteIndex(const IDBRequestData&, uint64_t, const String&) final { }
        void renameIndex(const IDBRequestData&, uint64_t, uint64_t, const String&) final { }
        void putOrAdd(const IDBRequestData&, const IDBKeyData&, const IDBValue&, const IndexedDB::ObjectStoreOverwriteMode) final { }
        void getRecord(const IDBRequestData&, const IDBGetRecordData&) final { }
        void getAllRecords(const IDBRequestData&, const IDBGetAllRecordsData&) final { }
        void getCount(const IDBRequestData&, const IDBKeyRangeData&) final { }
        void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&) final { }
        void openCursor(const IDBRequestData&, const IDBCursorInfo&) final { }
        void iterateCursor(const IDBRequestData&, const IDBIterateCursorData&) final { }
        void establishTransaction(uint64_t, const IDBTransactionInfo&) final { }
        void databaseConnectionPendingClose(uint64_t) final { }
        void databaseConnectionClosed(uint64_t) final { }
        void abortOpenAndUpgradeNeeded(uint64_t, const std::optional<IDBResourceIdentifier>&) final { }
        void didFireVersionChangeEvent(uint64_t, const IDBResourceIdentifier&, const IndexedDB::ConnectionClosedOnBehalfOfServer) final { }
        void openDBRequestCancelled(const IDBRequestData&) final { }
        void getAllDatabaseNamesAndVersions(const IDBResourceIdentifier&, const ClientOrigin&) final { }
        ~EmptyIDBConnectionToServerDeletegate() { }
    };

    IDBClient::IDBConnectionToServer& idbConnectionToServerForSession(PAL::SessionID) final
    {
        static NeverDestroyed<EmptyIDBConnectionToServerDeletegate> emptyDelegate;
        static auto& emptyConnection = IDBClient::IDBConnectionToServer::create(emptyDelegate.get()).leakRef();
        return emptyConnection;
    }
};

class EmptyDiagnosticLoggingClient final : public DiagnosticLoggingClient {
    void logDiagnosticMessage(const String&, const String&, ShouldSample) final { }
    void logDiagnosticMessageWithResult(const String&, const String&, DiagnosticLoggingResultType, ShouldSample) final { }
    void logDiagnosticMessageWithValue(const String&, const String&, double, unsigned, ShouldSample) final { }
    void logDiagnosticMessageWithEnhancedPrivacy(const String&, const String&, ShouldSample) final { }
    void logDiagnosticMessageWithValueDictionary(const String&, const String&, const ValueDictionary&, ShouldSample) final { }
    void logDiagnosticMessageWithDomain(const String&, DiagnosticLoggingDomain) final { };
};

#if ENABLE(DRAG_SUPPORT)

class EmptyDragClient final : public DragClient {
    void willPerformDragDestinationAction(DragDestinationAction, const DragData&) final { }
    void willPerformDragSourceAction(DragSourceAction, const IntPoint&, DataTransfer&) final { }
    OptionSet<DragSourceAction> dragSourceActionMaskForPoint(const IntPoint&) final { return { }; }
    void startDrag(DragItem, DataTransfer&, Frame&) final { }
};

#endif // ENABLE(DRAG_SUPPORT)

class EmptyEditorClient final : public EditorClient {
    WTF_MAKE_FAST_ALLOCATED;

public:
    EmptyEditorClient() = default;

private:
    bool shouldDeleteRange(const std::optional<SimpleRange>&) final { return false; }
    bool smartInsertDeleteEnabled() final { return false; }
    bool isSelectTrailingWhitespaceEnabled() const final { return false; }
    bool isContinuousSpellCheckingEnabled() final { return false; }
    void toggleContinuousSpellChecking() final { }
    bool isGrammarCheckingEnabled() final { return false; }
    void toggleGrammarChecking() final { }
    int spellCheckerDocumentTag() final { return -1; }

    bool shouldBeginEditing(const SimpleRange&) final { return false; }
    bool shouldEndEditing(const SimpleRange&) final { return false; }
    bool shouldInsertNode(Node&, const std::optional<SimpleRange>&, EditorInsertAction) final { return false; }
    bool shouldInsertText(const String&, const std::optional<SimpleRange>&, EditorInsertAction) final { return false; }
    bool shouldChangeSelectedRange(const std::optional<SimpleRange>&, const std::optional<SimpleRange>&, Affinity, bool) final { return false; }

    bool shouldApplyStyle(const StyleProperties&, const std::optional<SimpleRange>&) final { return false; }
    void didApplyStyle() final { }
    bool shouldMoveRangeAfterDelete(const SimpleRange&, const SimpleRange&) final { return false; }

    void didBeginEditing() final { }
    void respondToChangedContents() final { }
    void respondToChangedSelection(Frame*) final { }
    void updateEditorStateAfterLayoutIfEditabilityChanged() final { }
    void discardedComposition(Frame*) final { }
    void canceledComposition() final { }
    void didUpdateComposition() final { }
    void didEndEditing() final { }
    void didEndUserTriggeredSelectionChanges() final { }
    void willWriteSelectionToPasteboard(const std::optional<SimpleRange>&) final { }
    void didWriteSelectionToPasteboard() final { }
    void getClientPasteboardData(const std::optional<SimpleRange>&, Vector<String>&, Vector<RefPtr<SharedBuffer>>&) final { }
    void requestCandidatesForSelection(const VisibleSelection&) final { }
    void handleAcceptedCandidateWithSoftSpaces(TextCheckingResult) final { }

    void registerUndoStep(UndoStep&) final;
    void registerRedoStep(UndoStep&) final;
    void clearUndoRedoOperations() final { }

    DOMPasteAccessResponse requestDOMPasteAccess(DOMPasteAccessCategory, const String&) final { return DOMPasteAccessResponse::DeniedForGesture; }

    bool canCopyCut(Frame*, bool defaultValue) const final { return defaultValue; }
    bool canPaste(Frame*, bool defaultValue) const final { return defaultValue; }
    bool canUndo() const final { return false; }
    bool canRedo() const final { return false; }

    void undo() final { }
    void redo() final { }

    void handleKeyboardEvent(KeyboardEvent&) final { }
    void handleInputMethodKeydown(KeyboardEvent&) final { }

    void textFieldDidBeginEditing(Element&) final { }
    void textFieldDidEndEditing(Element&) final { }
    void textDidChangeInTextField(Element&) final { }
    bool doTextFieldCommandFromEvent(Element&, KeyboardEvent*) final { return false; }
    void textWillBeDeletedInTextField(Element&) final { }
    void textDidChangeInTextArea(Element&) final { }
    void overflowScrollPositionChanged() final { }
    void subFrameScrollPositionChanged() final { }

#if PLATFORM(IOS_FAMILY)
    void startDelayingAndCoalescingContentChangeNotifications() final { }
    void stopDelayingAndCoalescingContentChangeNotifications() final { }
    bool hasRichlyEditableSelection() final { return false; }
    int getPasteboardItemsCount() final { return 0; }
    RefPtr<DocumentFragment> documentFragmentFromDelegate(int) final { return nullptr; }
    bool performsTwoStepPaste(DocumentFragment*) final { return false; }
    void updateStringForFind(const String&) final { }
#endif

    bool performTwoStepDrop(DocumentFragment&, const SimpleRange&, bool) final { return false; }

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
    void setInputMethodState(Element*) final { }

    class EmptyTextCheckerClient final : public TextCheckerClient {
        bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const final { return true; }
        void ignoreWordInSpellDocument(const String&) final { }
        void learnWord(const String&) final { }
        void checkSpellingOfString(StringView, int*, int*) final { }
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
    NetworkStorageSession* storageSession() const final { return nullptr; }

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
    std::optional<String> validatedPaymentNetwork(const String&) final { return std::nullopt; }
    bool canMakePayments() final { return false; }
    void canMakePaymentsWithActiveCard(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTFMove(completionHandler)]() mutable { completionHandler(false); }); }
    void openPaymentSetup(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTFMove(completionHandler)]() mutable { completionHandler(false); }); }
    bool showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest&) final { return false; }
    void completeMerchantValidation(const PaymentMerchantSession&) final { }
    void completeShippingMethodSelection(std::optional<ApplePayShippingMethodUpdate>&&) final { }
    void completeShippingContactSelection(std::optional<ApplePayShippingContactUpdate>&&) final { }
    void completePaymentMethodSelection(std::optional<ApplePayPaymentMethodUpdate>&&) final { }
#if ENABLE(APPLE_PAY_COUPON_CODE)
    void completeCouponCodeChange(std::optional<ApplePayCouponCodeUpdate>&&) final { }
#endif
    void completePaymentSession(ApplePayPaymentAuthorizationResult&&) final { }
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
    void saveRecentSearches(const AtomString&, const Vector<RecentSearch>&) final { }
    void loadRecentSearches(const AtomString&, Vector<RecentSearch>&) final { }
    bool enabled() final { return false; }

    Ref<EmptyPopupMenu> m_popup;
};

class EmptyStorageNamespaceProvider final : public StorageNamespaceProvider {
    struct EmptyStorageArea : public StorageArea {
        unsigned length() final { return 0; }
        String key(unsigned) final { return { }; }
        String item(const String&) final { return { }; }
        void setItem(Frame&, const String&, const String&, bool&) final { }
        void removeItem(Frame&, const String&) final { }
        void clear(Frame&) final { }
        bool contains(const String&) final { return false; }
        StorageType storageType() const final { return StorageType::Local; }
        size_t memoryBytesUsedByCache() final { return 0; }
    };

    struct EmptyStorageNamespace final : public StorageNamespace {
        explicit EmptyStorageNamespace(PAL::SessionID sessionID)
            : m_sessionID(sessionID)
        {
        }

        const SecurityOrigin* topLevelOrigin() const final { return nullptr; };

    private:
        Ref<StorageArea> storageArea(const SecurityOrigin&) final { return adoptRef(*new EmptyStorageArea); }
        Ref<StorageNamespace> copy(Page&) final { return adoptRef(*new EmptyStorageNamespace { m_sessionID }); }
        PAL::SessionID sessionID() const final { return m_sessionID; }
        void setSessionIDForTesting(PAL::SessionID sessionID) final { m_sessionID = sessionID; };

        PAL::SessionID m_sessionID;
    };

    Ref<StorageNamespace> createLocalStorageNamespace(unsigned, PAL::SessionID) final;
    Ref<StorageNamespace> createTransientLocalStorageNamespace(SecurityOrigin&, unsigned, PAL::SessionID) final;

    void copySessionStorageNamespace(Page&, Page&) final { };
    RefPtr<StorageNamespace> sessionStorageNamespace(const SecurityOrigin&, Page&, ShouldCreateNamespace) final;
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
    bool isLinkVisited(Page&, SharedStringHash, const URL&, const AtomString&) final { return false; }
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

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

std::unique_ptr<DateTimeChooser> EmptyChromeClient::createDateTimeChooser(DateTimeChooserClient&)
{
    return nullptr;
}

#endif

#if ENABLE(APP_HIGHLIGHTS)

void EmptyChromeClient::storeAppHighlight(AppHighlight&&) const
{
}

#endif

void EmptyChromeClient::setTextIndicator(const TextIndicatorData&) const
{
}

DisplayRefreshMonitorFactory* EmptyChromeClient::displayRefreshMonitorFactory() const
{
    return EmptyDisplayRefreshMonitorFactory::sharedEmptyDisplayRefreshMonitorFactory();
}

void EmptyChromeClient::runOpenPanel(Frame&, FileChooser&)
{
}
    
void EmptyChromeClient::showShareSheet(ShareDataWithParsedURL&, CompletionHandler<void(bool)>&&)
{
}

void EmptyChromeClient::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    completion(CookieConsentDecisionResult::NotSupported);
}

void EmptyChromeClient::classifyModalContainerControls(Vector<String>&&, CompletionHandler<void(Vector<ModalContainerControlType>&&)>&& completion)
{
    completion({ });
}

void EmptyChromeClient::decidePolicyForModalContainer(OptionSet<ModalContainerControlType>, CompletionHandler<void(ModalContainerDecision)>&& completion)
{
    completion(ModalContainerDecision::Show);
}

void EmptyFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String&, PolicyCheckIdentifier, FramePolicyFunction&&)
{
}

void EmptyFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse&, FormState*, PolicyDecisionMode, PolicyCheckIdentifier, FramePolicyFunction&&)
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

RefPtr<Frame> EmptyFrameLoaderClient::createFrame(const AtomString&, HTMLFrameOwnerElement&)
{
    return nullptr;
}

RefPtr<Widget> EmptyFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool)
{
    return nullptr;
}

std::optional<PageIdentifier> EmptyFrameLoaderClient::pageID() const
{
    return std::nullopt;
}

bool EmptyFrameLoaderClient::hasWebView() const
{
    return true; // mainly for assertions
}

void EmptyFrameLoaderClient::makeRepresentation(DocumentLoader*)
{
}

#if PLATFORM(IOS_FAMILY)

bool EmptyFrameLoaderClient::forceLayoutOnRestoreFromBackForwardCache()
{
    return false;
}

#endif

void EmptyFrameLoaderClient::forceLayoutForNonHTML()
{
}

void EmptyFrameLoaderClient::setCopiesOnScroll()
{
}

void EmptyFrameLoaderClient::detachedFromParent2()
{
}

void EmptyFrameLoaderClient::detachedFromParent3()
{
}

void EmptyFrameLoaderClient::convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&)
{
}

void EmptyFrameLoaderClient::assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&)
{
}

bool EmptyFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier)
{
    return false;
}

void EmptyFrameLoaderClient::dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse&)
{
}

void EmptyFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&)
{
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)

bool EmptyFrameLoaderClient::canAuthenticateAgainstProtectionSpace(DocumentLoader*, ResourceLoaderIdentifier, const ProtectionSpace&)
{
    return false;
}

#endif

#if PLATFORM(IOS_FAMILY)

RetainPtr<CFDictionaryRef> EmptyFrameLoaderClient::connectionProperties(DocumentLoader*, ResourceLoaderIdentifier)
{
    return nullptr;
}

#endif

void EmptyFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&)
{
}

void EmptyFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int)
{
}

void EmptyFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier)
{
}

#if ENABLE(DATA_DETECTION)

void EmptyFrameLoaderClient::dispatchDidFinishDataDetection(NSArray *)
{
}

#endif

void EmptyFrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&)
{
}

bool EmptyFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int)
{
    return false;
}

void EmptyFrameLoaderClient::dispatchDidDispatchOnloadEvents()
{
}

void EmptyFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
}

void EmptyFrameLoaderClient::dispatchDidCancelClientRedirect()
{
}

void EmptyFrameLoaderClient::dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList)
{
}

void EmptyFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
}

void EmptyFrameLoaderClient::dispatchDidPushStateWithinPage()
{
}

void EmptyFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
}

void EmptyFrameLoaderClient::dispatchDidPopStateWithinPage()
{
}

void EmptyFrameLoaderClient::dispatchWillClose()
{
}

void EmptyFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
}

void EmptyFrameLoaderClient::dispatchDidReceiveTitle(const StringWithDirection&)
{
}

void EmptyFrameLoaderClient::dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<UsedLegacyTLS>, std::optional<WasPrivateRelayed>)
{
}

void EmptyFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading)
{
}

void EmptyFrameLoaderClient::dispatchDidFailLoad(const ResourceError&)
{
}

void EmptyFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
}

void EmptyFrameLoaderClient::dispatchDidFinishLoad()
{
}

void EmptyFrameLoaderClient::dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>)
{
}

void EmptyFrameLoaderClient::dispatchDidReachVisuallyNonEmptyState()
{
}

Frame* EmptyFrameLoaderClient::dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy)
{
    return nullptr;
}

void EmptyFrameLoaderClient::dispatchShow()
{
}

void EmptyFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, PolicyCheckIdentifier, const String&, FramePolicyFunction&&)
{
}

void EmptyFrameLoaderClient::cancelPolicyCheck()
{
}

void EmptyFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&)
{
}

void EmptyFrameLoaderClient::revertToProvisionalState(DocumentLoader*)
{
}

void EmptyFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError&)
{
}

void EmptyFrameLoaderClient::setMainFrameDocumentReady(bool)
{
}

void EmptyFrameLoaderClient::startDownload(const ResourceRequest&, const String&)
{
}

void EmptyFrameLoaderClient::willChangeTitle(DocumentLoader*)
{
}

void EmptyFrameLoaderClient::didChangeTitle(DocumentLoader*)
{
}

void EmptyFrameLoaderClient::willReplaceMultipartContent()
{
}

void EmptyFrameLoaderClient::didReplaceMultipartContent()
{
}

void EmptyFrameLoaderClient::committedLoad(DocumentLoader*, const SharedBuffer&)
{
}

void EmptyFrameLoaderClient::finishedLoading(DocumentLoader*)
{
}

ResourceError EmptyFrameLoaderClient::cancelledError(const ResourceRequest&) const
{
    return { ResourceError::Type::Cancellation };
}

ResourceError EmptyFrameLoaderClient::blockedError(const ResourceRequest&) const
{
    return { };
}

ResourceError EmptyFrameLoaderClient::blockedByContentBlockerError(const ResourceRequest&) const
{
    return { };
}

ResourceError EmptyFrameLoaderClient::cannotShowURLError(const ResourceRequest&) const
{
    return { };
}

ResourceError EmptyFrameLoaderClient::interruptedForPolicyChangeError(const ResourceRequest&) const
{
    return { };
}

#if ENABLE(CONTENT_FILTERING)

ResourceError EmptyFrameLoaderClient::blockedByContentFilterError(const ResourceRequest&) const
{
    return { };
}

#endif

ResourceError EmptyFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse&) const
{
    return { };
}

ResourceError EmptyFrameLoaderClient::fileDoesNotExistError(const ResourceResponse&) const
{
    return { };
}

ResourceError EmptyFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse&) const
{
    return { };
}

bool EmptyFrameLoaderClient::shouldFallBack(const ResourceError&) const
{
    return false;
}

bool EmptyFrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    return false;
}

bool EmptyFrameLoaderClient::canShowMIMEType(const String&) const
{
    return false;
}

bool EmptyFrameLoaderClient::canShowMIMETypeAsHTML(const String&) const
{
    return false;
}

bool EmptyFrameLoaderClient::representationExistsForURLScheme(StringView) const
{
    return false;
}

String EmptyFrameLoaderClient::generatedMIMETypeForURLScheme(StringView) const
{
    return emptyString();
}

void EmptyFrameLoaderClient::frameLoadCompleted()
{
}

void EmptyFrameLoaderClient::restoreViewState()
{
}

void EmptyFrameLoaderClient::provisionalLoadStarted()
{
}

void EmptyFrameLoaderClient::didFinishLoad()
{
}

void EmptyFrameLoaderClient::prepareForDataSourceReplacement()
{
}

void EmptyFrameLoaderClient::updateCachedDocumentLoader(DocumentLoader&)
{
}

void EmptyFrameLoaderClient::setTitle(const StringWithDirection&, const URL&)
{
}

String EmptyFrameLoaderClient::userAgent(const URL&) const
{
    return emptyString();
}

void EmptyFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*)
{
}

void EmptyFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
}

#if PLATFORM(IOS_FAMILY)

void EmptyFrameLoaderClient::didRestoreFrameHierarchyForCachedFrame()
{
}

#endif

void EmptyFrameLoaderClient::transitionToCommittedForNewPage()
{
}


void EmptyFrameLoaderClient::didRestoreFromBackForwardCache()
{
}


void EmptyFrameLoaderClient::updateGlobalHistory()
{
}

void EmptyFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
}

bool EmptyFrameLoaderClient::shouldGoToHistoryItem(HistoryItem&) const
{
    return false;
}

void EmptyFrameLoaderClient::saveViewStateToItem(HistoryItem&)
{
}

bool EmptyFrameLoaderClient::canCachePage() const
{
    return false;
}

void EmptyFrameLoaderClient::didDisplayInsecureContent()
{
}

void EmptyFrameLoaderClient::didRunInsecureContent(SecurityOrigin&, const URL&)
{
}


ObjectContentType EmptyFrameLoaderClient::objectContentType(const URL&, const String&)
{
    return ObjectContentType::None;
}

String EmptyFrameLoaderClient::overrideMediaType() const
{
    return { };
}

void EmptyFrameLoaderClient::redirectDataToPlugin(Widget&)
{
}

void EmptyFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&)
{
}

#if PLATFORM(COCOA)

RemoteAXObjectRef EmptyFrameLoaderClient::accessibilityRemoteObject()
{
    return nullptr;
}

void EmptyFrameLoaderClient::willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier, NSCachedURLResponse *response, CompletionHandler<void(NSCachedURLResponse *)>&& completionHandler) const
{
    completionHandler(response);
}

#endif

#if USE(CFURLCONNECTION)

bool EmptyFrameLoaderClient::shouldCacheResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&, const unsigned char*, unsigned long long)
{
    return true;
}

#endif

bool EmptyFrameLoaderClient::isEmptyFrameLoaderClient() const
{
    return true;
}

void EmptyFrameLoaderClient::prefetchDNS(const String&)
{
}

#if USE(QUICK_LOOK)

RefPtr<LegacyPreviewLoaderClient> EmptyFrameLoaderClient::createPreviewLoaderClient(const String&, const String&)
{
    return nullptr;
}

#endif

#if ENABLE(TRACKING_PREVENTION)

bool EmptyFrameLoaderClient::hasFrameSpecificStorageAccess()
{
    return false;
}

#endif

inline EmptyFrameNetworkingContext::EmptyFrameNetworkingContext()
    : FrameNetworkingContext { nullptr }
{
}

Ref<FrameNetworkingContext> EmptyFrameLoaderClient::createNetworkingContext()
{
    return EmptyFrameNetworkingContext::create();
}

void EmptyFrameLoaderClient::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(makeUnexpected(internalError(url)));
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

RefPtr<StorageNamespace> EmptyStorageNamespaceProvider::sessionStorageNamespace(const SecurityOrigin&, Page& page, ShouldCreateNamespace)
{
    return adoptRef(*new EmptyStorageNamespace { page.sessionID() });
}

Ref<StorageNamespace> EmptyStorageNamespaceProvider::createLocalStorageNamespace(unsigned, PAL::SessionID sessionID)
{
    return adoptRef(*new EmptyStorageNamespace { sessionID });
}

Ref<StorageNamespace> EmptyStorageNamespaceProvider::createTransientLocalStorageNamespace(SecurityOrigin&, unsigned, PAL::SessionID sessionID)
{
    return adoptRef(*new EmptyStorageNamespace { sessionID });
}

class EmptyStorageSessionProvider final : public StorageSessionProvider {
    NetworkStorageSession* storageSession() const final { return nullptr; }
};

class EmptyMediaRecorderProvider final : public MediaRecorderProvider {
public:
    EmptyMediaRecorderProvider() = default;
private:
#if ENABLE(MEDIA_RECORDER)
    std::unique_ptr<MediaRecorderPrivate> createMediaRecorderPrivate(MediaStreamPrivate&, const MediaRecorderPrivateOptions&) final { return nullptr; }
#endif
};

class EmptyBroadcastChannelRegistry final : public BroadcastChannelRegistry {
public:
    static Ref<EmptyBroadcastChannelRegistry> create()
    {
        return adoptRef(*new EmptyBroadcastChannelRegistry);
    }
private:
    EmptyBroadcastChannelRegistry() = default;

    void registerChannel(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier) final { }
    void unregisterChannel(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier) final { }
    void postMessage(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier, Ref<SerializedScriptValue>&&, CompletionHandler<void()>&&) final { }
};

PageConfiguration pageConfigurationWithEmptyClients(PAL::SessionID sessionID)
{
    PageConfiguration pageConfiguration {
        sessionID,
        makeUniqueRef<EmptyEditorClient>(),
        SocketProvider::create(),
        WebRTCProvider::create(),
        CacheStorageProvider::create(),
        adoptRef(*new EmptyUserContentProvider),
        adoptRef(*new EmptyBackForwardClient),
        CookieJar::create(adoptRef(*new EmptyStorageSessionProvider)),
        makeUniqueRef<EmptyProgressTrackerClient>(),
        makeUniqueRef<EmptyFrameLoaderClient>(),
        makeUniqueRef<DummySpeechRecognitionProvider>(),
        makeUniqueRef<EmptyMediaRecorderProvider>(),
        EmptyBroadcastChannelRegistry::create(),
        makeUniqueRef<DummyStorageProvider>(),
        makeUniqueRef<DummyModelPlayerProvider>()
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
    pageConfiguration.dragClient = makeUnique<EmptyDragClient>();
#endif

    static NeverDestroyed<EmptyInspectorClient> dummyInspectorClient;
    pageConfiguration.inspectorClient = &dummyInspectorClient.get();

    pageConfiguration.diagnosticLoggingClient = makeUnique<EmptyDiagnosticLoggingClient>();

    pageConfiguration.applicationCacheStorage = ApplicationCacheStorage::create({ }, { });
    pageConfiguration.databaseProvider = adoptRef(*new EmptyDatabaseProvider);
    pageConfiguration.pluginInfoProvider = adoptRef(*new EmptyPluginInfoProvider);
    pageConfiguration.storageNamespaceProvider = adoptRef(*new EmptyStorageNamespaceProvider);
    pageConfiguration.visitedLinkStore = adoptRef(*new EmptyVisitedLinkStore);
    
#if ENABLE(ATTACHMENT_ELEMENT)
    pageConfiguration.attachmentElementClient = makeUnique<EmptyAttachmentElementClient>();
#endif

    return pageConfiguration;
}

DiagnosticLoggingClient& emptyDiagnosticLoggingClient()
{
    static NeverDestroyed<EmptyDiagnosticLoggingClient> client;
    return client;
}

}
