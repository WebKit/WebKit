/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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
#include "BackForwardClient.h"
#include "BadgeClient.h"
#include "BroadcastChannelRegistry.h"
#include "CacheStorageProvider.h"
#include "ColorChooser.h"
#include "ContextMenuClient.h"
#include "CookieJar.h"
#include "CredentialRequestCoordinatorClient.h"
#include "DOMPasteAccess.h"
#include "DataListSuggestionPicker.h"
#include "DatabaseProvider.h"
#include "DateTimeChooser.h"
#include "DiagnosticLoggingClient.h"
#include "DisplayRefreshMonitor.h"
#include "DisplayRefreshMonitorFactory.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DocumentSyncClient.h"
#include "DragClient.h"
#include "DummyModelPlayerProvider.h"
#include "DummySpeechRecognitionProvider.h"
#include "DummyStorageProvider.h"
#include "EditorClient.h"
#include "EmptyAttachmentElementClient.h"
#include "EmptyBadgeClient.h"
#include "EmptyFrameLoaderClient.h"
#include "FormState.h"
#include "FrameNetworkingContext.h"
#include "HTMLFormElement.h"
#include "HistoryItem.h"
#include "IDBConnectionToServer.h"
#include "IDBIndexIdentifier.h"
#include "IDBObjectStoreIdentifier.h"
#include "Icon.h"
#include "InspectorBackendClient.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "ModalContainerTypes.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PaymentCoordinatorClient.h"
#include "PluginInfoProvider.h"
#include "PopupMenu.h"
#include "ProgressTrackerClient.h"
#include "RemoteFrameClient.h"
#include "SearchPopupMenu.h"
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
#include "WebTransportConnectionInfo.h"
#include "WebTransportConnectionStats.h"
#include "WebTransportReceiveStreamStats.h"
#include "WebTransportSendStreamStats.h"
#include "WebTransportSession.h"
#include <JavaScriptCore/HeapInlines.h>
#include <pal/SessionID.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "CompiledContentExtension.h"
#endif

#if USE(QUICK_LOOK)
#include "LegacyPreviewLoaderClient.h"
#endif

#if ENABLE(WEB_AUTHN)
#include "DigitalCredentialsRequestData.h"
#include "DigitalCredentialsResponseData.h"
#include "ExceptionData.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DummyStorageProvider);
WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyChromeClient);
WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyCryptoClient);

class UserMessageHandlerDescriptor;

class EmptyBackForwardClient final : public BackForwardClient {
    void LEGACY_NODELETE addItem(Ref<HistoryItem>&&) final { }
    void LEGACY_NODELETE setChildItem(BackForwardFrameItemIdentifier, Ref<HistoryItem>&&) final { }
    void LEGACY_NODELETE goToItem(HistoryItem&) final { }
    Vector<Ref<HistoryItem>> LEGACY_NODELETE allItems(FrameIdentifier) { return { }; }
    RefPtr<HistoryItem> itemAtIndex(int, FrameIdentifier) final { return nullptr; }
    unsigned LEGACY_NODELETE backListCount() const final { return 0; }
    unsigned LEGACY_NODELETE forwardListCount() const final { return 0; }
    bool LEGACY_NODELETE containsItem(const HistoryItem&) const final { return false; }
    void LEGACY_NODELETE close() final { }
};

#if ENABLE(CONTEXT_MENUS)

class EmptyContextMenuClient final : public ContextMenuClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyContextMenuClient);

    void LEGACY_NODELETE downloadURL(const URL&) final { }
    void LEGACY_NODELETE searchWithGoogle(const LocalFrame*) final { }
    void LEGACY_NODELETE lookUpInDictionary(LocalFrame*) final { }
    bool LEGACY_NODELETE isSpeaking() const final { return false; }
    void LEGACY_NODELETE speak(const String&) final { }
    void LEGACY_NODELETE stopSpeaking() final { }

#if HAVE(TRANSLATION_UI_SERVICES)
    void LEGACY_NODELETE handleTranslation(const TranslationContextMenuInfo&) final { }
#endif

#if PLATFORM(GTK)
    void insertEmoji(LocalFrame&) final { }
#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    void LEGACY_NODELETE showContextMenu() final { }
#endif

#if ENABLE(IMAGE_ANALYSIS)
    bool LEGACY_NODELETE supportsLookUpInImages() final { return false; }
#endif

#if ENABLE(IMAGE_ANALYSIS)
    bool LEGACY_NODELETE supportsCopySubject() final { return false; }
#endif
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyContextMenuClient);

#endif // ENABLE(CONTEXT_MENUS)

class EmptyDisplayRefreshMonitor final : public DisplayRefreshMonitor {
public:
    static Ref<EmptyDisplayRefreshMonitor> create(PlatformDisplayID displayID)
    {
        return adoptRef(*new EmptyDisplayRefreshMonitor(displayID));
    }

    void LEGACY_NODELETE displayLinkFired(const DisplayUpdate&) final { }
    bool LEGACY_NODELETE requestRefreshCallback() final { return false; }
    void LEGACY_NODELETE stop() final { }

    bool LEGACY_NODELETE startNotificationMechanism() final { return true; }
    void LEGACY_NODELETE stopNotificationMechanism() final { }

private:
    explicit EmptyDisplayRefreshMonitor(PlatformDisplayID displayID)
        : DisplayRefreshMonitor(displayID)
    {
    }
};

class EmptyDisplayRefreshMonitorFactory final : public DisplayRefreshMonitorFactory {
public:
    static DisplayRefreshMonitorFactory* LEGACY_NODELETE sharedEmptyDisplayRefreshMonitorFactory()
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
    struct EmptyIDBConnectionToServerDeletegate final : public IDBClient::IDBConnectionToServerDelegate, public RefCounted<EmptyIDBConnectionToServerDeletegate> {
        static Ref<EmptyIDBConnectionToServerDeletegate> create() { return adoptRef(*new EmptyIDBConnectionToServerDeletegate); }

        void LEGACY_NODELETE ref() const final { RefCounted::ref(); }
        void deref() const final { RefCounted::deref(); }

        std::optional<IDBConnectionIdentifier> LEGACY_NODELETE identifier() const final { return std::nullopt; }
        void LEGACY_NODELETE deleteDatabase(const IDBOpenRequestData&) final { }
        void LEGACY_NODELETE openDatabase(const IDBOpenRequestData&) final { }
        void LEGACY_NODELETE abortTransaction(const IDBResourceIdentifier&) final { }
        void LEGACY_NODELETE commitTransaction(const IDBResourceIdentifier&, uint64_t) final { }
        void LEGACY_NODELETE didFinishHandlingVersionChangeTransaction(IDBDatabaseConnectionIdentifier, const IDBResourceIdentifier&) final { }
        void LEGACY_NODELETE createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&) final { }
        void LEGACY_NODELETE deleteObjectStore(const IDBRequestData&, const String&) final { }
        void LEGACY_NODELETE renameObjectStore(const IDBRequestData&, IDBObjectStoreIdentifier, const String&) final { }
        void LEGACY_NODELETE clearObjectStore(const IDBRequestData&, IDBObjectStoreIdentifier) final { }
        void LEGACY_NODELETE createIndex(const IDBRequestData&, const IDBIndexInfo&) final { }
        void LEGACY_NODELETE deleteIndex(const IDBRequestData&, IDBObjectStoreIdentifier, const String&) final { }
        void LEGACY_NODELETE renameIndex(const IDBRequestData&, IDBObjectStoreIdentifier, IDBIndexIdentifier, const String&) final { }
        void LEGACY_NODELETE putOrAdd(const IDBRequestData&, const IDBKeyData&, const IDBValue&, const IndexIDToIndexKeyMap&, const IndexedDB::ObjectStoreOverwriteMode) final { }
        void LEGACY_NODELETE getRecord(const IDBRequestData&, const IDBGetRecordData&) final { }
        void LEGACY_NODELETE getAllRecords(const IDBRequestData&, const IDBGetAllRecordsData&) final { }
        void LEGACY_NODELETE getCount(const IDBRequestData&, const IDBKeyRangeData&) final { }
        void LEGACY_NODELETE deleteRecord(const IDBRequestData&, const IDBKeyRangeData&) final { }
        void LEGACY_NODELETE openCursor(const IDBRequestData&, const IDBCursorInfo&) final { }
        void LEGACY_NODELETE iterateCursor(const IDBRequestData&, const IDBIterateCursorData&) final { }
        void LEGACY_NODELETE establishTransaction(IDBDatabaseConnectionIdentifier, const IDBTransactionInfo&) final { }
        void LEGACY_NODELETE databaseConnectionPendingClose(IDBDatabaseConnectionIdentifier) final { }
        void LEGACY_NODELETE databaseConnectionClosed(IDBDatabaseConnectionIdentifier) final { }
        void LEGACY_NODELETE abortOpenAndUpgradeNeeded(IDBDatabaseConnectionIdentifier, const std::optional<IDBResourceIdentifier>&) final { }
        void LEGACY_NODELETE didFireVersionChangeEvent(IDBDatabaseConnectionIdentifier, const IDBResourceIdentifier&, const IndexedDB::ConnectionClosedOnBehalfOfServer) final { }
        void LEGACY_NODELETE openDBRequestCancelled(const IDBOpenRequestData&) final { }
        void LEGACY_NODELETE getAllDatabaseNamesAndVersions(const IDBResourceIdentifier&, const ClientOrigin&) final { }
        void LEGACY_NODELETE didGenerateIndexKeyForRecord(const IDBResourceIdentifier&, const IDBResourceIdentifier&, const IDBIndexInfo&, const IDBKeyData&, const IndexKey&, std::optional<int64_t>) { }
    private:
        EmptyIDBConnectionToServerDeletegate() = default;
    };

    IDBClient::IDBConnectionToServer& idbConnectionToServerForSession(PAL::SessionID sessionID) final
    {
        static NeverDestroyed<Ref<EmptyIDBConnectionToServerDeletegate>> emptyDelegate = EmptyIDBConnectionToServerDeletegate::create();
        static NeverDestroyed<Ref<IDBClient::IDBConnectionToServer>> emptyConnection = IDBClient::IDBConnectionToServer::create(emptyDelegate.get(), sessionID).leakRef();
        return emptyConnection->get();
    }
};

class EmptyDiagnosticLoggingClient final : public DiagnosticLoggingClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyDiagnosticLoggingClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(EmptyDiagnosticLoggingClient);

    void LEGACY_NODELETE logDiagnosticMessage(const String&, const String&, ShouldSample) final { }
    void LEGACY_NODELETE logDiagnosticMessageWithResult(const String&, const String&, DiagnosticLoggingResultType, ShouldSample) final { }
    void logDiagnosticMessageWithValue(const String&, const String&, double, unsigned, ShouldSample) final { }
    void LEGACY_NODELETE logDiagnosticMessageWithEnhancedPrivacy(const String&, const String&, ShouldSample) final { }
    void LEGACY_NODELETE logDiagnosticMessageWithValueDictionary(const String&, const String&, const ValueDictionary&, ShouldSample) final { }
    void LEGACY_NODELETE logDiagnosticMessageWithDomain(const String&, DiagnosticLoggingDomain) final { };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyDiagnosticLoggingClient);

#if ENABLE(DRAG_SUPPORT)

class EmptyDragClient final : public DragClient {
    void LEGACY_NODELETE willPerformDragDestinationAction(DragDestinationAction, const DragData&) final { }
    void LEGACY_NODELETE willPerformDragSourceAction(DragSourceAction, const IntPoint&, DataTransfer&) final { }
    OptionSet<DragSourceAction> dragSourceActionMaskForPoint(const IntPoint&) final { return { }; }
    void startDrag(DragItem, DataTransfer&, Frame&, const std::optional<NodeIdentifier>&) final { }
};

#endif // ENABLE(DRAG_SUPPORT)

class EmptyEditorClient final : public EditorClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyEditorClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(EmptyEditorClient);
private:
    bool LEGACY_NODELETE shouldDeleteRange(const std::optional<SimpleRange>&) final { return false; }
    bool LEGACY_NODELETE smartInsertDeleteEnabled() final { return false; }
    bool LEGACY_NODELETE isSelectTrailingWhitespaceEnabled() const final { return false; }
    bool LEGACY_NODELETE isContinuousSpellCheckingEnabled() final { return false; }
    void LEGACY_NODELETE toggleContinuousSpellChecking() final { }
    bool LEGACY_NODELETE isGrammarCheckingEnabled() final { return false; }
    void LEGACY_NODELETE toggleGrammarChecking() final { }
    int LEGACY_NODELETE spellCheckerDocumentTag() final { return -1; }

    bool LEGACY_NODELETE shouldBeginEditing(const SimpleRange&) final { return false; }
    bool LEGACY_NODELETE shouldEndEditing(const SimpleRange&) final { return false; }
    bool LEGACY_NODELETE shouldInsertNode(Node&, const std::optional<SimpleRange>&, EditorInsertAction) final { return false; }
    bool LEGACY_NODELETE shouldInsertText(const String&, const std::optional<SimpleRange>&, EditorInsertAction) final { return false; }
    bool LEGACY_NODELETE shouldChangeSelectedRange(const std::optional<SimpleRange>&, const std::optional<SimpleRange>&, Affinity, bool) final { return false; }

    bool LEGACY_NODELETE shouldApplyStyle(const StyleProperties&, const std::optional<SimpleRange>&) final { return false; }
    void LEGACY_NODELETE didApplyStyle() final { }
    bool LEGACY_NODELETE shouldMoveRangeAfterDelete(const SimpleRange&, const SimpleRange&) final { return false; }

    void LEGACY_NODELETE didBeginEditing() final { }
    void LEGACY_NODELETE respondToChangedContents() final { }
    void LEGACY_NODELETE respondToChangedSelection(LocalFrame*) final { }
    void LEGACY_NODELETE updateEditorStateAfterLayoutIfEditabilityChanged() final { }
    void LEGACY_NODELETE discardedComposition(const Document&) final { }
    void LEGACY_NODELETE canceledComposition() final { }
    void LEGACY_NODELETE didUpdateComposition() final { }
    void LEGACY_NODELETE didEndEditing() final { }
    void LEGACY_NODELETE didEndUserTriggeredSelectionChanges() final { }
    void LEGACY_NODELETE willWriteSelectionToPasteboard(const std::optional<SimpleRange>&) final { }
    void LEGACY_NODELETE didWriteSelectionToPasteboard() final { }
    void LEGACY_NODELETE getClientPasteboardData(const std::optional<SimpleRange>&, Vector<std::pair<String, RefPtr<SharedBuffer>>>&) final { }
    void LEGACY_NODELETE requestCandidatesForSelection(const VisibleSelection&) final { }
    void LEGACY_NODELETE handleAcceptedCandidateWithSoftSpaces(const TextCheckingResult&) final { }

    void registerUndoStep(UndoStep&) final;
    void registerRedoStep(UndoStep&) final;
    void LEGACY_NODELETE clearUndoRedoOperations() final { }

    DOMPasteAccessResponse LEGACY_NODELETE requestDOMPasteAccess(DOMPasteAccessCategory, FrameIdentifier, const String&) final { return DOMPasteAccessResponse::DeniedForGesture; }
#if PLATFORM(COCOA)
    HashMap<FrameIdentifier, AttributedString> collectAttributedStringsForRemoteFrames(FrameIdentifier, const Vector<FrameIdentifier>&) final { return { }; }
#endif

    bool LEGACY_NODELETE canCopyCut(LocalFrame*, bool defaultValue) const final { return defaultValue; }
    bool LEGACY_NODELETE canPaste(LocalFrame*, bool defaultValue) const final { return defaultValue; }
    bool LEGACY_NODELETE canUndo() const final { return false; }
    bool LEGACY_NODELETE canRedo() const final { return false; }

    void LEGACY_NODELETE undo() final { }
    void LEGACY_NODELETE redo() final { }

    void LEGACY_NODELETE handleKeyboardEvent(KeyboardEvent&) final { }
    void LEGACY_NODELETE handleInputMethodKeydown(KeyboardEvent&) final { }

    void LEGACY_NODELETE textFieldDidBeginEditing(Element&) final { }
    void LEGACY_NODELETE textFieldDidEndEditing(Element&) final { }
    void LEGACY_NODELETE textDidChangeInTextField(Element&) final { }
    bool LEGACY_NODELETE doTextFieldCommandFromEvent(Element&, KeyboardEvent*) final { return false; }
    void LEGACY_NODELETE textWillBeDeletedInTextField(Element&) final { }
    void LEGACY_NODELETE textDidChangeInTextArea(Element&) final { }
    void LEGACY_NODELETE overflowScrollPositionChanged() final { }
    void LEGACY_NODELETE subFrameScrollPositionChanged() final { }

#if PLATFORM(IOS_FAMILY)
    void startDelayingAndCoalescingContentChangeNotifications() final { }
    void stopDelayingAndCoalescingContentChangeNotifications() final { }
    bool hasRichlyEditableSelection() final { return false; }
    int getPasteboardItemsCount() final { return 0; }
    RefPtr<DocumentFragment> documentFragmentFromDelegate(int) final { return nullptr; }
    bool performsTwoStepPaste(DocumentFragment*) final { return false; }
    void updateStringForFind(const String&) final { }
#endif

    bool LEGACY_NODELETE performTwoStepDrop(DocumentFragment&, const SimpleRange&, bool) final { return false; }

#if PLATFORM(COCOA)
    void LEGACY_NODELETE setInsertionPasteboard(const String&) final { };
#endif

#if USE(APPKIT)
    void LEGACY_NODELETE uppercaseWord() final { }
    void LEGACY_NODELETE lowercaseWord() final { }
    void LEGACY_NODELETE capitalizeWord() final { }
    bool LEGACY_NODELETE canApplyCaseTransformations(const String&) final { return true; }
    bool LEGACY_NODELETE canConvertToTraditionalChinese(const String&) final { return false; }
    bool LEGACY_NODELETE canConvertToSimplifiedChinese(const String&) final { return false; }
    void LEGACY_NODELETE convertToTraditionalChinese() final { }
    void LEGACY_NODELETE convertToSimplifiedChinese() final { }
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void LEGACY_NODELETE showSubstitutionsPanel(bool) final { }
    bool LEGACY_NODELETE substitutionsPanelIsShowing() final { return false; }
    void LEGACY_NODELETE toggleSmartInsertDelete() final { }
    bool LEGACY_NODELETE isAutomaticQuoteSubstitutionEnabled() final { return false; }
    void LEGACY_NODELETE toggleAutomaticQuoteSubstitution() final { }
    bool LEGACY_NODELETE isAutomaticLinkDetectionEnabled() final { return false; }
    void LEGACY_NODELETE toggleAutomaticLinkDetection() final { }
    bool LEGACY_NODELETE isAutomaticDashSubstitutionEnabled() final { return false; }
    void LEGACY_NODELETE toggleAutomaticDashSubstitution() final { }
    bool LEGACY_NODELETE isAutomaticTextReplacementEnabled() final { return false; }
    void LEGACY_NODELETE toggleAutomaticTextReplacement() final { }
    bool LEGACY_NODELETE isAutomaticSpellingCorrectionEnabled() final { return false; }
    void LEGACY_NODELETE toggleAutomaticSpellingCorrection() final { }
    bool LEGACY_NODELETE isSmartListsEnabled() final { return false; }
    void LEGACY_NODELETE toggleSmartLists() { }
#endif

#if PLATFORM(GTK)
    bool shouldShowUnicodeMenu() final { return false; }
#endif

    TextCheckerClient* LEGACY_NODELETE textChecker() final { return &m_textCheckerClient; }

    void LEGACY_NODELETE updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) final { }
    void LEGACY_NODELETE updateSpellingUIWithMisspelledWord(const String&) final { }
    void LEGACY_NODELETE showSpellingUI(bool) final { }
    bool LEGACY_NODELETE spellingUIIsShowing() final { return false; }

    void LEGACY_NODELETE setInputMethodState(Element*) final { }

    class EmptyTextCheckerClient final : public TextCheckerClient {
        bool LEGACY_NODELETE shouldEraseMarkersAfterChangeSelection(TextCheckingType) const final { return true; }
        void LEGACY_NODELETE ignoreWordInSpellDocument(const String&) final { }
        void LEGACY_NODELETE learnWord(const String&) final { }
        void LEGACY_NODELETE checkSpellingOfString(StringView, int*, int*) final { }
        void LEGACY_NODELETE checkGrammarOfString(StringView, Vector<GrammarDetail>&, int*, int*) final { }

#if USE(UNIFIED_TEXT_CHECKING)
        Vector<TextCheckingResult> LEGACY_NODELETE checkTextOfParagraph(StringView, OptionSet<TextCheckingType>, const VisibleSelection&) final { return { }; }
#endif

        void LEGACY_NODELETE getGuessesForWord(const String&, const String&, const VisibleSelection&, Vector<String>&) final { }
        void requestCheckingOfString(TextCheckingRequest&, const VisibleSelection&) final;
        void requestExtendedCheckingOfString(TextCheckingRequest&, const VisibleSelection&) final;
    };

    EmptyTextCheckerClient m_textCheckerClient;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyEditorClient);

class EmptyFrameNetworkingContext final : public FrameNetworkingContext {
public:
    static Ref<EmptyFrameNetworkingContext> create() { return adoptRef(*new EmptyFrameNetworkingContext); }

private:
    EmptyFrameNetworkingContext();

    bool LEGACY_NODELETE shouldClearReferrerOnHTTPSToHTTPRedirect() const { return true; }
    CookieStorageSession* LEGACY_NODELETE storageSession() const final { return nullptr; }

#if PLATFORM(COCOA)
    bool LEGACY_NODELETE localFileContentSniffingEnabled() const { return false; }
    SchedulePairHashSet* LEGACY_NODELETE scheduledRunLoopPairs() const { return nullptr; }
    RetainPtr<CFDataRef> sourceApplicationAuditData() const { return nullptr; };
#endif

#if PLATFORM(COCOA) || PLATFORM(WIN)
    ResourceError blockedError(const ResourceRequest&) const final { return { }; }
#endif
};

class EmptyInspectorBackendClient final : public InspectorBackendClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyInspectorBackendClient);
    void LEGACY_NODELETE inspectedPageDestroyed() final { }
    Inspector::FrontendChannel* LEGACY_NODELETE openLocalFrontend(PageInspectorController*) final { return nullptr; }
    void LEGACY_NODELETE bringFrontendToFront() final { }
    void LEGACY_NODELETE highlight() final { }
    void LEGACY_NODELETE hideHighlight() final { }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyInspectorBackendClient);

#if ENABLE(APPLE_PAY)

class EmptyPaymentCoordinatorClient final : public PaymentCoordinatorClient, public RefCounted<EmptyPaymentCoordinatorClient> {
    WTF_MAKE_TZONE_ALLOCATED(EmptyPaymentCoordinatorClient);
public:
    static Ref<EmptyPaymentCoordinatorClient> create()
    {
        return adoptRef(*new EmptyPaymentCoordinatorClient);
    }

    void LEGACY_NODELETE ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    EmptyPaymentCoordinatorClient() = default;

    std::optional<String> LEGACY_NODELETE validatedPaymentNetwork(const String&) const final { return std::nullopt; }
    bool LEGACY_NODELETE canMakePayments() final { return false; }
    void canMakePaymentsWithActiveCard(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTF::move(completionHandler)]() mutable { completionHandler(false); }); }
    void openPaymentSetup(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTF::move(completionHandler)]() mutable { completionHandler(false); }); }
    bool LEGACY_NODELETE showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest&) final { return false; }
    void LEGACY_NODELETE completeMerchantValidation(const PaymentMerchantSession&) final { }
    void LEGACY_NODELETE completeShippingMethodSelection(std::optional<ApplePayShippingMethodUpdate>&&) final { }
    void LEGACY_NODELETE completeShippingContactSelection(std::optional<ApplePayShippingContactUpdate>&&) final { }
    void LEGACY_NODELETE completePaymentMethodSelection(std::optional<ApplePayPaymentMethodUpdate>&&) final { }
#if ENABLE(APPLE_PAY_COUPON_CODE)
    void LEGACY_NODELETE completeCouponCodeChange(std::optional<ApplePayCouponCodeUpdate>&&) final { }
#endif
    void LEGACY_NODELETE completePaymentSession(ApplePayPaymentAuthorizationResult&&) final { }
    void LEGACY_NODELETE cancelPaymentSession() final { }
    void LEGACY_NODELETE abortPaymentSession() final { }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyPaymentCoordinatorClient);

#endif

#if ENABLE(WEB_AUTHN)
class EmptyCredentialRequestCoordinatorClient final : public CredentialRequestCoordinatorClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyCredentialRequestCoordinatorClient);
public:
    static Ref<EmptyCredentialRequestCoordinatorClient> create()
    {
        return adoptRef(*new EmptyCredentialRequestCoordinatorClient);
    }

    void showDigitalCredentialsChooser(std::optional<FrameIdentifier>, DigitalCredentialsRawRequests&&, const DigitalCredentialsRequestData&, CompletionHandler<void(std::expected<DigitalCredentialsResponseData, ExceptionData>&&)>&& completionHandler)
    {
        callOnMainThread([completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(makeUnexpected(ExceptionData { ExceptionCode::NotSupportedError, "Empty client."_s }));
        });
    }

    void dismissDigitalCredentialsChooser(CompletionHandler<void(bool)>&& completionHandler) final
    {
        callOnMainThread([completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(false);
        });
    }

    ExceptionOr<Vector<ValidatedDigitalCredentialRequest>> validateAndParseDigitalCredentialRequests(const SecurityOrigin&, const Document&, const Vector<UnvalidatedDigitalCredentialRequest>&)
    {
        return Exception { ExceptionCode::InvalidStateError };
    }

private:
    EmptyCredentialRequestCoordinatorClient() = default;
};
WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyCredentialRequestCoordinatorClient);
#endif

class EmptyPluginInfoProvider final : public PluginInfoProvider {
    void LEGACY_NODELETE refreshPlugins() final { };
    Vector<PluginInfo> LEGACY_NODELETE pluginInfo(Page&, std::optional<Vector<SupportedPluginIdentifier>>&) final { return { }; }
    Vector<PluginInfo> LEGACY_NODELETE webVisiblePluginInfo(Page&, const URL&) final { return { }; }
};

class EmptyPopupMenu : public PopupMenu {
private:
    void LEGACY_NODELETE show(const IntRect&, LocalFrameView&, int) final { }
    void LEGACY_NODELETE hide() final { }
    void LEGACY_NODELETE updateFromElement() final { }
    void LEGACY_NODELETE disconnectClient() final { }
};

class EmptyProgressTrackerClient final : public ProgressTrackerClient {
    void LEGACY_NODELETE willChangeEstimatedProgress() final { }
    void LEGACY_NODELETE didChangeEstimatedProgress() final { }
    void LEGACY_NODELETE progressStarted(LocalFrame&) final { }
    void LEGACY_NODELETE progressEstimateChanged(LocalFrame&) final { }
    void LEGACY_NODELETE progressFinished(LocalFrame&) final { }
};

class EmptySearchPopupMenu : public SearchPopupMenu {
public:
    EmptySearchPopupMenu()
        : m_popup(adoptRef(*new EmptyPopupMenu))
    {
    }

private:
    PopupMenu* LEGACY_NODELETE popupMenu() final { return m_popup.ptr(); }
    void LEGACY_NODELETE saveRecentSearches(const AtomString&, const Vector<RecentSearch>&) final { }
    void LEGACY_NODELETE loadRecentSearches(const AtomString&, Vector<RecentSearch>&) final { }
    bool LEGACY_NODELETE enabled() final { return false; }

    const Ref<EmptyPopupMenu> m_popup;
};

class EmptyStorageNamespaceProvider final : public StorageNamespaceProvider {
    struct EmptyStorageArea : public StorageArea {
        unsigned LEGACY_NODELETE length() final { return 0; }
        String LEGACY_NODELETE key(unsigned) final { return { }; }
        String LEGACY_NODELETE item(const String&) final { return { }; }
        void LEGACY_NODELETE setItem(LocalFrame&, const String&, const String&, bool&) final { }
        void LEGACY_NODELETE removeItem(LocalFrame&, const String&) final { }
        void LEGACY_NODELETE clear(LocalFrame&) final { }
        bool LEGACY_NODELETE contains(const String&) final { return false; }
        StorageType LEGACY_NODELETE storageType() const final { return StorageType::Local; }
        size_t LEGACY_NODELETE memoryBytesUsedByCache() final { return 0; }
    };

    struct EmptyStorageNamespace final : public StorageNamespace {
        explicit EmptyStorageNamespace(PAL::SessionID sessionID)
            : m_sessionID(sessionID)
        {
        }

        const SecurityOrigin* LEGACY_NODELETE topLevelOrigin() const final { return nullptr; };

    private:
        Ref<StorageArea> storageArea(const SecurityOrigin&) final { return adoptRef(*new EmptyStorageArea); }
        Ref<StorageNamespace> copy(Page&) final { return adoptRef(*new EmptyStorageNamespace { m_sessionID }); }
        PAL::SessionID LEGACY_NODELETE sessionID() const final { return m_sessionID; }
        void LEGACY_NODELETE setSessionIDForTesting(PAL::SessionID sessionID) final { m_sessionID = sessionID; };

        PAL::SessionID m_sessionID;
    };

    Ref<StorageNamespace> createLocalStorageNamespace(unsigned, PAL::SessionID) final;
    Ref<StorageNamespace> createTransientLocalStorageNamespace(SecurityOrigin&, unsigned, PAL::SessionID) final;
    RefPtr<StorageNamespace> sessionStorageNamespace(const SecurityOrigin&, Page&, ShouldCreateNamespace) final;
};

class EmptyUserContentProvider final : public UserContentProvider {
    void LEGACY_NODELETE forEachUserScript(NOESCAPE const Function<void(DOMWrapperWorld&, const UserScript&)>&) const final { }
    void LEGACY_NODELETE forEachUserStyleSheet(NOESCAPE const Function<void(const UserStyleSheet&)>&) const final { }
#if ENABLE(USER_MESSAGE_HANDLERS)
    void LEGACY_NODELETE forEachUserMessageHandler(NOESCAPE const Function<void(const UserMessageHandlerDescriptor&)>&) const final { }
#endif
    bool LEGACY_NODELETE hasBuffersForWorld(const DOMWrapperWorld&) const final { return false; }
    WebKitBuffer* LEGACY_NODELETE buffer(const DOMWrapperWorld&, const String&) const final { return nullptr; }
#if ENABLE(CONTENT_EXTENSIONS)
    const ContentExtensions::ContentExtensionsBackend& LEGACY_NODELETE userContentExtensionBackend() const final { static NeverDestroyed<ContentExtensions::ContentExtensionsBackend> backend; return backend.get(); };
#endif
};

class EmptyVisitedLinkStore final : public VisitedLinkStore {
    bool LEGACY_NODELETE isLinkVisited(Page&, SharedStringHash, const URL&, const AtomString&) final { return false; }
    void LEGACY_NODELETE addVisitedLink(Page&, SharedStringHash) final { }
};

RefPtr<Page> EmptyChromeClient::createWindow(LocalFrame&, const String&, const WindowFeatures&, const NavigationAction&)
{
    return nullptr;
}

RefPtr<PopupMenu> EmptyChromeClient::createPopupMenu(PopupMenuClient&) const
{
    return adoptRef(*new EmptyPopupMenu);
}

RefPtr<SearchPopupMenu> EmptyChromeClient::createSearchPopupMenu(PopupMenuClient&) const
{
    return adoptRef(*new EmptySearchPopupMenu);
}

RefPtr<ColorChooser> EmptyChromeClient::createColorChooser(ColorChooserClient&, const Color&)
{
    return nullptr;
}

RefPtr<DataListSuggestionPicker> EmptyChromeClient::createDataListSuggestionPicker(DataListSuggestionsClient&)
{
    return nullptr;
}

RefPtr<DateTimeChooser> EmptyChromeClient::createDateTimeChooser(DateTimeChooserClient&)
{
    return nullptr;
}

void EmptyChromeClient::setTextIndicator(RefPtr<TextIndicator>&&) const
{
}

void EmptyChromeClient::updateTextIndicator(RefPtr<TextIndicator>&&) const
{
}

DisplayRefreshMonitorFactory* EmptyChromeClient::displayRefreshMonitorFactory() const
{
    return EmptyDisplayRefreshMonitorFactory::sharedEmptyDisplayRefreshMonitorFactory();
}

void EmptyChromeClient::runOpenPanel(LocalFrame&, FileChooser&)
{
}
    
void EmptyChromeClient::showShareSheet(ShareDataWithParsedURL&&, CompletionHandler<void(bool)>&&)
{
}

RefPtr<Icon> EmptyChromeClient::createIconForFiles(const Vector<String>& /* filenames */)
{
    return nullptr;
}

// MARK: -

void EmptyFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&&)
{
}

void EmptyFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse&, FormState*, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&&)
{
}

void EmptyFrameLoaderClient::updateSandboxFlags(SandboxFlags)
{
}

void EmptyFrameLoaderClient::updateOpener(std::optional<FrameIdentifier>)
{
}

void EmptyFrameLoaderClient::setPrinting(bool, FloatSize, FloatSize, float, AdjustViewSize)
{
}

void EmptyFrameLoaderClient::dispatchWillSendSubmitEvent(Ref<FormState>&&)
{
}

void EmptyFrameLoaderClient::dispatchWillSubmitForm(FormState&, URL&&, String&&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

Ref<DocumentLoader> EmptyFrameLoaderClient::createDocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData, ResourceRequest&& originalRequest)
{
    return DocumentLoader::create(WTF::move(request), WTF::move(substituteData), WTF::move(originalRequest));
}

Ref<DocumentLoader> EmptyFrameLoaderClient::createDocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData)
{
    return DocumentLoader::create(WTF::move(request), WTF::move(substituteData), { });
}

RefPtr<LocalFrame> EmptyFrameLoaderClient::createFrame(const AtomString&, HTMLFrameOwnerElement&)
{
    return nullptr;
}

RefPtr<Widget> EmptyFrameLoaderClient::createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool)
{
    return nullptr;
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

void EmptyFrameLoaderClient::dispatchDidCommitLoad(const std::optional<BackForwardCacheCommitData>&)
{
}

void EmptyFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading, WillInternallyHandleFailure)
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

LocalFrame* EmptyFrameLoaderClient::dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy, const String&)
{
    return nullptr;
}

void EmptyFrameLoaderClient::dispatchShow()
{
}

void EmptyFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, const String&, FramePolicyFunction&&)
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

void EmptyFrameLoaderClient::startDownload(const ResourceRequest&, const String&, FromDownloadAttribute)
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

bool EmptyFrameLoaderClient::shouldFallBack(const ResourceError&) const
{
    return false;
}

void EmptyFrameLoaderClient::loadStorageAccessQuirksIfNeeded()
{
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

void EmptyFrameLoaderClient::transitionToCommittedForNewPage(InitializingIframe)
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

ShouldGoToHistoryItem EmptyFrameLoaderClient::shouldGoToHistoryItem(HistoryItem&, IsSameDocumentNavigation) const
{
    return ShouldGoToHistoryItem::No;
}

bool EmptyFrameLoaderClient::supportsAsyncShouldGoToHistoryItem() const
{
    return false;
}

void EmptyFrameLoaderClient::shouldGoToHistoryItemAsync(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}

void EmptyFrameLoaderClient::dispatchGoToBackForwardItemAtIndex(int)
{
}

void EmptyFrameLoaderClient::dispatchEnqueueHistoryTraversalDelta(int)
{
}

void EmptyFrameLoaderClient::saveViewStateToItem(HistoryItem&)
{
}

bool EmptyFrameLoaderClient::canCachePage() const
{
    return false;
}

ObjectContentType EmptyFrameLoaderClient::objectContentType(const URL&, const String&)
{
    return ObjectContentType::None;
}

AtomString EmptyFrameLoaderClient::overrideMediaType() const
{
    return nullAtom();
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

IntPoint EmptyFrameLoaderClient::accessibilityRemoteFrameOffset()
{
    return { };
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void EmptyFrameLoaderClient::setIsolatedTree(Ref<WebCore::AXIsolatedTree>&&)
{
}

RefPtr<WebCore::AXIsolatedTree> EmptyFrameLoaderClient::isolatedTree() const
{
    return nullptr;
}
#endif

void EmptyFrameLoaderClient::willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier, NSCachedURLResponse *response, CompletionHandler<void(NSCachedURLResponse *)>&& completionHandler) const
{
    completionHandler(response);
}

#endif

bool EmptyFrameLoaderClient::isEmptyFrameLoaderClient() const
{
    return true;
}

void EmptyFrameLoaderClient::prefetchDNS(const String&)
{
}

RefPtr<HistoryItem> EmptyFrameLoaderClient::createHistoryItemTree(bool, BackForwardItemIdentifier) const
{
    return nullptr;
}

#if USE(QUICK_LOOK)

RefPtr<LegacyPreviewLoaderClient> EmptyFrameLoaderClient::createPreviewLoaderClient(const String&, const String&)
{
    return nullptr;
}

#endif

bool EmptyFrameLoaderClient::hasFrameSpecificStorageAccess()
{
    return false;
}

void EmptyFrameLoaderClient::revokeFrameSpecificStorageAccess()
{
}

void EmptyFrameLoaderClient::dispatchLoadEventToOwnerElementInAnotherProcess()
{
}

inline EmptyFrameNetworkingContext::EmptyFrameNetworkingContext()
    : FrameNetworkingContext { nullptr }
{
}

Ref<FrameNetworkingContext> EmptyFrameLoaderClient::createNetworkingContext()
{
    return EmptyFrameNetworkingContext::create();
}

void EmptyFrameLoaderClient::sendH2Ping(const URL& url, CompletionHandler<void(std::expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(makeUnexpected(internalError(url)));
}

void LEGACY_NODELETE EmptyEditorClient::EmptyTextCheckerClient::requestCheckingOfString(TextCheckingRequest&, const VisibleSelection&)
{
}

void LEGACY_NODELETE EmptyEditorClient::EmptyTextCheckerClient::requestExtendedCheckingOfString(TextCheckingRequest&, const VisibleSelection&)
{
}

void LEGACY_NODELETE EmptyEditorClient::registerUndoStep(UndoStep&)
{
}

void LEGACY_NODELETE EmptyEditorClient::registerRedoStep(UndoStep&)
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
    CookieStorageSession* LEGACY_NODELETE storageSession() const final { return nullptr; }
};

class EmptyBroadcastChannelRegistry final : public BroadcastChannelRegistry {
public:
    static Ref<EmptyBroadcastChannelRegistry> create()
    {
        return adoptRef(*new EmptyBroadcastChannelRegistry);
    }
private:
    EmptyBroadcastChannelRegistry() = default;

    void LEGACY_NODELETE registerChannel(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier) final { }
    void LEGACY_NODELETE unregisterChannel(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier) final { }
    void LEGACY_NODELETE postMessage(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier, Ref<SerializedScriptValue>&&, CompletionHandler<void()>&&) final { }
};

class EmptyWebTransportSession final : public WebTransportSession, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<EmptyWebTransportSession> {
public:
    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

    Ref<WebTransportSessionInitializationPromise> initialize(ScriptExecutionContext&, const URL&, const WebTransportOptions&, const Vector<KeyValuePair<String, String>>&, const ClientOrigin&) final { return WebTransportSessionInitializationPromise::createAndReject(); }
    Ref<WebTransportSendPromise> sendDatagram(std::optional<WebTransportSendGroupIdentifier>, std::span<const uint8_t>) final { return WebTransportSendPromise::createAndReject(); }
    Ref<WebTransportStreamPromise> createOutgoingUnidirectionalStream() final { return WebTransportStreamPromise::createAndReject(); }
    Ref<WebTransportStreamPromise> createBidirectionalStream() final { return WebTransportStreamPromise::createAndReject(); }
    Ref<WebTransportSendPromise> streamSendBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool) final { return WebTransportSendPromise::createAndReject(); }
    Ref<WebTransportConnectionStatsPromise> getStats() final { return WebTransportConnectionStatsPromise::createAndReject(); }
    Ref<WebTransportSendStreamStatsPromise> getSendStreamStats(WebTransportStreamIdentifier) final { return WebTransportSendStreamStatsPromise::createAndReject(); }
    Ref<WebTransportReceiveStreamStatsPromise> getReceiveStreamStats(WebTransportStreamIdentifier) final { return WebTransportReceiveStreamStatsPromise::createAndReject(); }
    Ref<WebTransportSendStreamStatsPromise> getSendGroupStats(WebTransportSendGroupIdentifier) final { return WebTransportSendStreamStatsPromise::createAndReject(); }
    Ref<WebTransportExportKeyingMaterialPromise> exportKeyingMaterial(std::span<const uint8_t>, std::span<const uint8_t>, uint32_t) final { return WebTransportExportKeyingMaterialPromise::createAndReject(); }

    void cancelReceiveStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final { }
    void cancelSendStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final { }
    void destroyStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final { }
    void terminate(WebTransportSessionErrorCode, UTF8CString&&) final { }
    void datagramIncomingMaxAgeUpdated(std::optional<double>) final { }
    void datagramOutgoingMaxAgeUpdated(std::optional<double>) final { }
    void incomingMaxBufferedDatagramsUpdated(uint32_t) final { }
    void outgoingMaxBufferedDatagramsUpdated(uint32_t) final { }
};

class EmptySocketProvider final : public SocketProvider {
public:
    RefPtr<ThreadableWebSocketChannel> createWebSocketChannel(Document&, WebSocketChannelClient&, IsInitiatedByDedicatedWorker) final { return nullptr; }

    Ref<WebTransportSession> createWebTransportSession(ScriptExecutionContext&, WebTransportSessionClient&) { return adoptRef(*new EmptyWebTransportSession()); }
};

Ref<SocketProvider> emptySocketProvider()
{
    static NeverDestroyed<Ref<SocketProvider>> provider { adoptRef(*new EmptySocketProvider()) };
    return provider.get();
}

class EmptyHistoryItemClient final : public HistoryItemClient {
public:
    static Ref<EmptyHistoryItemClient> create() { return adoptRef(*new EmptyHistoryItemClient); }
private:
    void LEGACY_NODELETE historyItemChanged(const HistoryItem&) { }
    void LEGACY_NODELETE clearChildren(const HistoryItem&) const { }
};

PageConfiguration pageConfigurationWithEmptyClients(std::optional<PageIdentifier> identifier, PAL::SessionID sessionID)
{
    PageConfiguration pageConfiguration {
        identifier,
        sessionID,
        makeUniqueRef<EmptyEditorClient>(),
        emptySocketProvider(),
        WebRTCProvider::create(),
        CacheStorageProvider::create(),
        adoptRef(*new EmptyUserContentProvider),
        adoptRef(*new EmptyBackForwardClient),
        CookieJar::create(adoptRef(*new EmptyStorageSessionProvider)),
        makeUniqueRef<EmptyProgressTrackerClient>(),
        PageConfiguration::LocalMainFrameCreationParameters {
            CompletionHandler<UniqueRef<LocalFrameLoaderClient>(LocalFrame&, FrameLoader&)> { [] (auto&, auto& frameLoader) {
                return makeUniqueRefWithoutRefCountedCheck<EmptyFrameLoaderClient>(frameLoader);
            } },
            SandboxFlags::all(),
            ReferrerPolicy::EmptyString
        },
        generateFrameIdentifier(),
        nullptr,
        makeUniqueRef<DummySpeechRecognitionProvider>(),
        EmptyBroadcastChannelRegistry::create(),
        makeUniqueRef<DummyStorageProvider>(),
        DummyModelPlayerProvider::create(),
        EmptyBadgeClient::create(),
        EmptyHistoryItemClient::create(),
#if ENABLE(CONTEXT_MENUS)
        makeUniqueRef<EmptyContextMenuClient>(),
#endif
#if ENABLE(APPLE_PAY)
        EmptyPaymentCoordinatorClient::create(),
#endif
        makeUniqueRef<EmptyChromeClient>(),
        makeUniqueRef<EmptyCryptoClient>(),
        makeUniqueRef<DocumentSyncClient>()
#if ENABLE(WEB_AUTHN)
        , EmptyCredentialRequestCoordinatorClient::create()
#endif
    };

#if ENABLE(DRAG_SUPPORT)
    pageConfiguration.dragClient = makeUnique<EmptyDragClient>();
#endif

    pageConfiguration.inspectorBackendClient = makeUnique<EmptyInspectorBackendClient>();

    pageConfiguration.diagnosticLoggingClient = makeUnique<EmptyDiagnosticLoggingClient>();

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
