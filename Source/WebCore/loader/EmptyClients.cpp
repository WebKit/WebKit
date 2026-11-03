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
    NODELETE void addItem(Ref<HistoryItem>&&) final { }
    NODELETE void setChildItem(BackForwardFrameItemIdentifier, Ref<HistoryItem>&&) final { }
    NODELETE void goToItem(HistoryItem&) final { }
    NODELETE Vector<Ref<HistoryItem>> allItems(FrameIdentifier) { return { }; }
    RefPtr<HistoryItem> itemAtIndex(int, FrameIdentifier) final { return nullptr; }
    NODELETE unsigned backListCount() const final { return 0; }
    NODELETE unsigned forwardListCount() const final { return 0; }
    NODELETE bool containsItem(const HistoryItem&) const final { return false; }
    NODELETE void close() final { }
};

#if ENABLE(CONTEXT_MENUS)

class EmptyContextMenuClient final : public ContextMenuClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyContextMenuClient);

    NODELETE void downloadURL(const URL&) final { }
    NODELETE void searchWithGoogle(const LocalFrame*) final { }
    NODELETE void lookUpInDictionary(LocalFrame*) final { }
    NODELETE bool isSpeaking() const final { return false; }
    NODELETE void speak(const String&) final { }
    NODELETE void stopSpeaking() final { }

#if HAVE(TRANSLATION_UI_SERVICES)
    NODELETE void handleTranslation(const TranslationContextMenuInfo&) final { }
#endif

#if PLATFORM(GTK)
    void insertEmoji(LocalFrame&) final { }
#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    NODELETE void showContextMenu() final { }
#endif

#if ENABLE(IMAGE_ANALYSIS)
    NODELETE bool supportsLookUpInImages() final { return false; }
#endif

#if ENABLE(IMAGE_ANALYSIS)
    NODELETE bool supportsCopySubject() final { return false; }
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

    NODELETE void displayLinkFired(const DisplayUpdate&) final { }
    NODELETE bool requestRefreshCallback() final { return false; }
    NODELETE void stop() final { }

    NODELETE bool startNotificationMechanism() final { return true; }
    NODELETE void stopNotificationMechanism() final { }

private:
    explicit EmptyDisplayRefreshMonitor(PlatformDisplayID displayID)
        : DisplayRefreshMonitor(displayID)
    {
    }
};

class EmptyDisplayRefreshMonitorFactory final : public DisplayRefreshMonitorFactory {
public:
    NODELETE static DisplayRefreshMonitorFactory* sharedEmptyDisplayRefreshMonitorFactory()
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
        NODELETE static Ref<EmptyIDBConnectionToServerDeletegate> create() { return adoptRef(*new EmptyIDBConnectionToServerDeletegate); }

        NODELETE void ref() const final { RefCounted::ref(); }
        void deref() const final { RefCounted::deref(); }

        NODELETE std::optional<IDBConnectionIdentifier> identifier() const final { return std::nullopt; }
        NODELETE void deleteDatabase(const IDBOpenRequestData&) final { }
        NODELETE void openDatabase(const IDBOpenRequestData&) final { }
        NODELETE void abortTransaction(const IDBResourceIdentifier&) final { }
        NODELETE void commitTransaction(const IDBResourceIdentifier&, uint64_t) final { }
        NODELETE void didFinishHandlingVersionChangeTransaction(IDBDatabaseConnectionIdentifier, const IDBResourceIdentifier&) final { }
        NODELETE void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&) final { }
        NODELETE void deleteObjectStore(const IDBRequestData&, const String&) final { }
        NODELETE void renameObjectStore(const IDBRequestData&, IDBObjectStoreIdentifier, const String&) final { }
        NODELETE void clearObjectStore(const IDBRequestData&, IDBObjectStoreIdentifier) final { }
        NODELETE void createIndex(const IDBRequestData&, const IDBIndexInfo&) final { }
        NODELETE void deleteIndex(const IDBRequestData&, IDBObjectStoreIdentifier, const String&) final { }
        NODELETE void renameIndex(const IDBRequestData&, IDBObjectStoreIdentifier, IDBIndexIdentifier, const String&) final { }
        NODELETE void putOrAdd(const IDBRequestData&, const IDBKeyData&, const IDBValue&, const IndexIDToIndexKeyMap&, const IndexedDB::ObjectStoreOverwriteMode) final { }
        NODELETE void getRecord(const IDBRequestData&, const IDBGetRecordData&) final { }
        NODELETE void getAllRecords(const IDBRequestData&, const IDBGetAllRecordsData&) final { }
        NODELETE void getCount(const IDBRequestData&, const IDBKeyRangeData&) final { }
        NODELETE void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&) final { }
        NODELETE void openCursor(const IDBRequestData&, const IDBCursorInfo&) final { }
        NODELETE void iterateCursor(const IDBRequestData&, const IDBIterateCursorData&) final { }
        NODELETE void establishTransaction(IDBDatabaseConnectionIdentifier, const IDBTransactionInfo&) final { }
        NODELETE void databaseConnectionPendingClose(IDBDatabaseConnectionIdentifier) final { }
        NODELETE void databaseConnectionClosed(IDBDatabaseConnectionIdentifier) final { }
        NODELETE void abortOpenAndUpgradeNeeded(IDBDatabaseConnectionIdentifier, const std::optional<IDBResourceIdentifier>&) final { }
        NODELETE void didFireVersionChangeEvent(IDBDatabaseConnectionIdentifier, const IDBResourceIdentifier&, const IndexedDB::ConnectionClosedOnBehalfOfServer) final { }
        NODELETE void openDBRequestCancelled(const IDBOpenRequestData&) final { }
        NODELETE void getAllDatabaseNamesAndVersions(const IDBResourceIdentifier&, const ClientOrigin&) final { }
        NODELETE void didGenerateIndexKeyForRecord(const IDBResourceIdentifier&, const IDBResourceIdentifier&, const IDBIndexInfo&, const IDBKeyData&, const IndexKey&, std::optional<int64_t>) { }
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

    NODELETE void logDiagnosticMessage(const String&, const String&, ShouldSample) final { }
    NODELETE void logDiagnosticMessageWithResult(const String&, const String&, DiagnosticLoggingResultType, ShouldSample) final { }
    void logDiagnosticMessageWithValue(const String&, const String&, double, unsigned, ShouldSample) final { }
    NODELETE void logDiagnosticMessageWithEnhancedPrivacy(const String&, const String&, ShouldSample) final { }
    NODELETE void logDiagnosticMessageWithValueDictionary(const String&, const String&, const ValueDictionary&, ShouldSample) final { }
    NODELETE void logDiagnosticMessageWithDomain(const String&, DiagnosticLoggingDomain) final { };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyDiagnosticLoggingClient);

#if ENABLE(DRAG_SUPPORT)

class EmptyDragClient final : public DragClient {
    NODELETE void willPerformDragDestinationAction(DragDestinationAction, const DragData&) final { }
    NODELETE void willPerformDragSourceAction(DragSourceAction, const IntPoint&, DataTransfer&) final { }
    OptionSet<DragSourceAction> dragSourceActionMaskForPoint(const IntPoint&) final { return { }; }
    void startDrag(DragItem, DataTransfer&, Frame&, const std::optional<NodeIdentifier>&) final { }
};

#endif // ENABLE(DRAG_SUPPORT)

class EmptyEditorClient final : public EditorClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyEditorClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(EmptyEditorClient);
private:
    NODELETE bool shouldDeleteRange(const std::optional<SimpleRange>&) final { return false; }
    NODELETE bool smartInsertDeleteEnabled() final { return false; }
    NODELETE bool isSelectTrailingWhitespaceEnabled() const final { return false; }
    NODELETE bool isContinuousSpellCheckingEnabled() final { return false; }
    NODELETE void toggleContinuousSpellChecking() final { }
    NODELETE bool isGrammarCheckingEnabled() final { return false; }
    NODELETE void toggleGrammarChecking() final { }
    NODELETE int spellCheckerDocumentTag() final { return -1; }

    NODELETE bool shouldBeginEditing(const SimpleRange&) final { return false; }
    NODELETE bool shouldEndEditing(const SimpleRange&) final { return false; }
    NODELETE bool shouldInsertNode(Node&, const std::optional<SimpleRange>&, EditorInsertAction) final { return false; }
    NODELETE bool shouldInsertText(const String&, const std::optional<SimpleRange>&, EditorInsertAction) final { return false; }
    NODELETE bool shouldChangeSelectedRange(const std::optional<SimpleRange>&, const std::optional<SimpleRange>&, Affinity, bool) final { return false; }

    NODELETE bool shouldApplyStyle(const StyleProperties&, const std::optional<SimpleRange>&) final { return false; }
    NODELETE void didApplyStyle() final { }
    NODELETE bool shouldMoveRangeAfterDelete(const SimpleRange&, const SimpleRange&) final { return false; }

    NODELETE void didBeginEditing() final { }
    NODELETE void respondToChangedContents() final { }
    NODELETE void respondToChangedSelection(LocalFrame*) final { }
    NODELETE void updateEditorStateAfterLayoutIfEditabilityChanged() final { }
    NODELETE void discardedComposition(const Document&) final { }
    NODELETE void canceledComposition() final { }
    NODELETE void didUpdateComposition() final { }
    NODELETE void didEndEditing() final { }
    NODELETE void didEndUserTriggeredSelectionChanges() final { }
    NODELETE void willWriteSelectionToPasteboard(const std::optional<SimpleRange>&) final { }
    NODELETE void didWriteSelectionToPasteboard() final { }
    NODELETE void getClientPasteboardData(const std::optional<SimpleRange>&, Vector<std::pair<String, RefPtr<SharedBuffer>>>&) final { }
    NODELETE void requestCandidatesForSelection(const VisibleSelection&) final { }
    NODELETE void handleAcceptedCandidateWithSoftSpaces(const TextCheckingResult&) final { }

    void registerUndoStep(UndoStep&) final;
    void registerRedoStep(UndoStep&) final;
    NODELETE void clearUndoRedoOperations() final { }

    NODELETE DOMPasteAccessResponse requestDOMPasteAccess(DOMPasteAccessCategory, FrameIdentifier, const String&) final { return DOMPasteAccessResponse::DeniedForGesture; }
#if PLATFORM(COCOA)
    HashMap<FrameIdentifier, AttributedString> collectAttributedStringsForRemoteFrames(FrameIdentifier, const Vector<FrameIdentifier>&) final { return { }; }
#endif

    NODELETE bool canCopyCut(LocalFrame*, bool defaultValue) const final { return defaultValue; }
    NODELETE bool canPaste(LocalFrame*, bool defaultValue) const final { return defaultValue; }
    NODELETE bool canUndo() const final { return false; }
    NODELETE bool canRedo() const final { return false; }

    NODELETE void undo() final { }
    NODELETE void redo() final { }

    NODELETE void handleKeyboardEvent(KeyboardEvent&) final { }
    NODELETE void handleInputMethodKeydown(KeyboardEvent&) final { }

    NODELETE void textFieldDidBeginEditing(Element&) final { }
    NODELETE void textFieldDidEndEditing(Element&) final { }
    NODELETE void textDidChangeInTextField(Element&) final { }
    NODELETE bool doTextFieldCommandFromEvent(Element&, KeyboardEvent*) final { return false; }
    NODELETE void textWillBeDeletedInTextField(Element&) final { }
    NODELETE void textDidChangeInTextArea(Element&) final { }
    NODELETE void overflowScrollPositionChanged() final { }
    NODELETE void subFrameScrollPositionChanged() final { }

#if PLATFORM(IOS_FAMILY)
    void startDelayingAndCoalescingContentChangeNotifications() final { }
    void stopDelayingAndCoalescingContentChangeNotifications() final { }
    bool hasRichlyEditableSelection() final { return false; }
    int getPasteboardItemsCount() final { return 0; }
    RefPtr<DocumentFragment> documentFragmentFromDelegate(int) final { return nullptr; }
    bool performsTwoStepPaste(DocumentFragment*) final { return false; }
    void updateStringForFind(const String&) final { }
#endif

    NODELETE bool performTwoStepDrop(DocumentFragment&, const SimpleRange&, bool) final { return false; }

#if PLATFORM(COCOA)
    NODELETE void setInsertionPasteboard(const String&) final { };
#endif

#if USE(APPKIT)
    NODELETE void uppercaseWord() final { }
    NODELETE void lowercaseWord() final { }
    NODELETE void capitalizeWord() final { }
    NODELETE bool canApplyCaseTransformations(const String&) final { return true; }
    NODELETE bool canConvertToTraditionalChinese(const String&) final { return false; }
    NODELETE bool canConvertToSimplifiedChinese(const String&) final { return false; }
    NODELETE void convertToTraditionalChinese() final { }
    NODELETE void convertToSimplifiedChinese() final { }
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    NODELETE void showSubstitutionsPanel(bool) final { }
    NODELETE bool substitutionsPanelIsShowing() final { return false; }
    NODELETE void toggleSmartInsertDelete() final { }
    NODELETE bool isAutomaticQuoteSubstitutionEnabled() final { return false; }
    NODELETE void toggleAutomaticQuoteSubstitution() final { }
    NODELETE bool isAutomaticLinkDetectionEnabled() final { return false; }
    NODELETE void toggleAutomaticLinkDetection() final { }
    NODELETE bool isAutomaticDashSubstitutionEnabled() final { return false; }
    NODELETE void toggleAutomaticDashSubstitution() final { }
    NODELETE bool isAutomaticTextReplacementEnabled() final { return false; }
    NODELETE void toggleAutomaticTextReplacement() final { }
    NODELETE bool isAutomaticSpellingCorrectionEnabled() final { return false; }
    NODELETE void toggleAutomaticSpellingCorrection() final { }
    NODELETE bool isSmartListsEnabled() final { return false; }
    NODELETE void toggleSmartLists() { }
#endif

#if PLATFORM(GTK)
    bool shouldShowUnicodeMenu() final { return false; }
#endif

    NODELETE TextCheckerClient* textChecker() final { return &m_textCheckerClient; }

    NODELETE void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) final { }
    NODELETE void updateSpellingUIWithMisspelledWord(const String&) final { }
    NODELETE void showSpellingUI(bool) final { }
    NODELETE bool spellingUIIsShowing() final { return false; }

    NODELETE void setInputMethodState(Element*) final { }

    class EmptyTextCheckerClient final : public TextCheckerClient {
        NODELETE bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const final { return true; }
        NODELETE void ignoreWordInSpellDocument(const String&) final { }
        NODELETE void learnWord(const String&) final { }
        NODELETE void checkSpellingOfString(StringView, int*, int*) final { }
        NODELETE void checkGrammarOfString(StringView, Vector<GrammarDetail>&, int*, int*) final { }

#if USE(UNIFIED_TEXT_CHECKING)
        NODELETE Vector<TextCheckingResult> checkTextOfParagraph(StringView, OptionSet<TextCheckingType>, const VisibleSelection&) final { return { }; }
#endif

        NODELETE void getGuessesForWord(const String&, const String&, const VisibleSelection&, Vector<String>&) final { }
        void requestCheckingOfString(TextCheckingRequest&, const VisibleSelection&) final;
        void requestExtendedCheckingOfString(TextCheckingRequest&, const VisibleSelection&) final;
    };

    EmptyTextCheckerClient m_textCheckerClient;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyEditorClient);

class EmptyFrameNetworkingContext final : public FrameNetworkingContext {
public:
    NODELETE static Ref<EmptyFrameNetworkingContext> create() { return adoptRef(*new EmptyFrameNetworkingContext); }

private:
    EmptyFrameNetworkingContext();

    NODELETE bool shouldClearReferrerOnHTTPSToHTTPRedirect() const { return true; }
    NODELETE CookieStorageSession* storageSession() const final { return nullptr; }

#if PLATFORM(COCOA)
    NODELETE bool localFileContentSniffingEnabled() const { return false; }
    NODELETE SchedulePairHashSet* scheduledRunLoopPairs() const { return nullptr; }
    RetainPtr<CFDataRef> sourceApplicationAuditData() const { return nullptr; };
#endif

#if PLATFORM(COCOA) || PLATFORM(WIN)
    ResourceError blockedError(const ResourceRequest&) const final { return { }; }
#endif
};

class EmptyInspectorBackendClient final : public InspectorBackendClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyInspectorBackendClient);
    NODELETE void inspectedPageDestroyed() final { }
    NODELETE Inspector::FrontendChannel* openLocalFrontend(PageInspectorController*) final { return nullptr; }
    NODELETE void bringFrontendToFront() final { }
    NODELETE void highlight() final { }
    NODELETE void hideHighlight() final { }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyInspectorBackendClient);

#if ENABLE(APPLE_PAY)

class EmptyPaymentCoordinatorClient final : public PaymentCoordinatorClient, public RefCounted<EmptyPaymentCoordinatorClient> {
    WTF_MAKE_TZONE_ALLOCATED(EmptyPaymentCoordinatorClient);
public:
    NODELETE static Ref<EmptyPaymentCoordinatorClient> create()
    {
        return adoptRef(*new EmptyPaymentCoordinatorClient);
    }

    NODELETE void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    EmptyPaymentCoordinatorClient() = default;

    NODELETE std::optional<String> validatedPaymentNetwork(const String&) const final { return std::nullopt; }
    NODELETE bool canMakePayments() final { return false; }
    void canMakePaymentsWithActiveCard(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTF::move(completionHandler)]() mutable { completionHandler(false); }); }
    void openPaymentSetup(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTF::move(completionHandler)]() mutable { completionHandler(false); }); }
    NODELETE bool showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest&) final { return false; }
    NODELETE void completeMerchantValidation(const PaymentMerchantSession&) final { }
    NODELETE void completeShippingMethodSelection(std::optional<ApplePayShippingMethodUpdate>&&) final { }
    NODELETE void completeShippingContactSelection(std::optional<ApplePayShippingContactUpdate>&&) final { }
    NODELETE void completePaymentMethodSelection(std::optional<ApplePayPaymentMethodUpdate>&&) final { }
#if ENABLE(APPLE_PAY_COUPON_CODE)
    NODELETE void completeCouponCodeChange(std::optional<ApplePayCouponCodeUpdate>&&) final { }
#endif
    NODELETE void completePaymentSession(ApplePayPaymentAuthorizationResult&&) final { }
    NODELETE void cancelPaymentSession() final { }
    NODELETE void abortPaymentSession() final { }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyPaymentCoordinatorClient);

#endif

#if ENABLE(WEB_AUTHN)
class EmptyCredentialRequestCoordinatorClient final : public CredentialRequestCoordinatorClient {
    WTF_MAKE_TZONE_ALLOCATED(EmptyCredentialRequestCoordinatorClient);
public:
    NODELETE static Ref<EmptyCredentialRequestCoordinatorClient> create()
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

    ExceptionOr<Vector<std::optional<ValidatedDigitalCredentialRequest>>> validateAndParseDigitalCredentialRequests(const SecurityOrigin&, const Document&, const Vector<UnvalidatedDigitalCredentialRequest>&)
    {
        return Exception { ExceptionCode::InvalidStateError };
    }

private:
    EmptyCredentialRequestCoordinatorClient() = default;
};
WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyCredentialRequestCoordinatorClient);
#endif

class EmptyPluginInfoProvider final : public PluginInfoProvider {
    NODELETE void refreshPlugins() final { };
    NODELETE Vector<PluginInfo> pluginInfo(Page&, std::optional<Vector<SupportedPluginIdentifier>>&) final { return { }; }
    NODELETE Vector<PluginInfo> webVisiblePluginInfo(Page&, const URL&) final { return { }; }
};

class EmptyPopupMenu : public PopupMenu {
private:
    NODELETE void show(const IntRect&, LocalFrameView&, int) final { }
    NODELETE void hide() final { }
    NODELETE void updateFromElement() final { }
    NODELETE void disconnectClient() final { }
};

class EmptyProgressTrackerClient final : public ProgressTrackerClient {
    NODELETE void willChangeEstimatedProgress() final { }
    NODELETE void didChangeEstimatedProgress() final { }
    NODELETE void progressStarted(LocalFrame&) final { }
    NODELETE void progressEstimateChanged(LocalFrame&) final { }
    NODELETE void progressFinished(LocalFrame&) final { }
};

class EmptySearchPopupMenu : public SearchPopupMenu {
public:
    EmptySearchPopupMenu()
        : m_popup(adoptRef(*new EmptyPopupMenu))
    {
    }

private:
    NODELETE PopupMenu* popupMenu() final { return m_popup.ptr(); }
    NODELETE void saveRecentSearches(const AtomString&, const Vector<RecentSearch>&) final { }
    NODELETE void loadRecentSearches(const AtomString&, Vector<RecentSearch>&) final { }
    NODELETE bool enabled() final { return false; }

    const Ref<EmptyPopupMenu> m_popup;
};

class EmptyStorageNamespaceProvider final : public StorageNamespaceProvider {
    struct EmptyStorageArea : public StorageArea {
        NODELETE unsigned length() final { return 0; }
        NODELETE String key(unsigned) final { return { }; }
        NODELETE String item(const String&) final { return { }; }
        NODELETE void setItem(LocalFrame&, const String&, const String&, bool&) final { }
        NODELETE void removeItem(LocalFrame&, const String&) final { }
        NODELETE void clear(LocalFrame&) final { }
        NODELETE bool contains(const String&) final { return false; }
        NODELETE StorageType storageType() const final { return StorageType::Local; }
        NODELETE size_t memoryBytesUsedByCache() final { return 0; }
    };

    struct EmptyStorageNamespace final : public StorageNamespace {
        explicit EmptyStorageNamespace(PAL::SessionID sessionID)
            : m_sessionID(sessionID)
        {
        }

        NODELETE const SecurityOrigin* topLevelOrigin() const final { return nullptr; };

    private:
        Ref<StorageArea> storageArea(const SecurityOrigin&) final { return adoptRef(*new EmptyStorageArea); }
        Ref<StorageNamespace> copy(Page&) final { return adoptRef(*new EmptyStorageNamespace { m_sessionID }); }
        NODELETE PAL::SessionID sessionID() const final { return m_sessionID; }
        NODELETE void setSessionIDForTesting(PAL::SessionID sessionID) final { m_sessionID = sessionID; };

        PAL::SessionID m_sessionID;
    };

    Ref<StorageNamespace> createLocalStorageNamespace(unsigned, PAL::SessionID) final;
    Ref<StorageNamespace> createTransientLocalStorageNamespace(SecurityOrigin&, unsigned, PAL::SessionID) final;
    RefPtr<StorageNamespace> sessionStorageNamespace(const SecurityOrigin&, Page&, ShouldCreateNamespace) final;
};

class EmptyUserContentProvider final : public UserContentProvider {
    NODELETE void forEachUserScript(NOESCAPE const Function<void(DOMWrapperWorld&, const UserScript&)>&) const final { }
    NODELETE void forEachUserStyleSheet(NOESCAPE const Function<void(const UserStyleSheet&)>&) const final { }
#if ENABLE(USER_MESSAGE_HANDLERS)
    NODELETE void forEachUserMessageHandler(NOESCAPE const Function<void(const UserMessageHandlerDescriptor&)>&) const final { }
#endif
    NODELETE bool hasBuffersForWorld(const DOMWrapperWorld&) const final { return false; }
    NODELETE WebKitBuffer* buffer(const DOMWrapperWorld&, const String&) const final { return nullptr; }
#if ENABLE(CONTENT_EXTENSIONS)
    NODELETE const ContentExtensions::ContentExtensionsBackend& userContentExtensionBackend() const final { static NeverDestroyed<ContentExtensions::ContentExtensionsBackend> backend; return backend.get(); };
#endif
};

class EmptyVisitedLinkStore final : public VisitedLinkStore {
    NODELETE bool isLinkVisited(Page&, SharedStringHash, const URL&, const AtomString&) final { return false; }
    NODELETE void addVisitedLink(Page&, SharedStringHash) final { }
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

void EmptyFrameLoaderClient::dispatchEnqueueHistoryTraversalToItem(BackForwardFrameItemIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(false);
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

NODELETE void EmptyEditorClient::EmptyTextCheckerClient::requestCheckingOfString(TextCheckingRequest&, const VisibleSelection&)
{
}

NODELETE void EmptyEditorClient::EmptyTextCheckerClient::requestExtendedCheckingOfString(TextCheckingRequest&, const VisibleSelection&)
{
}

NODELETE void EmptyEditorClient::registerUndoStep(UndoStep&)
{
}

NODELETE void EmptyEditorClient::registerRedoStep(UndoStep&)
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
    NODELETE CookieStorageSession* storageSession() const final { return nullptr; }
};

class EmptyBroadcastChannelRegistry final : public BroadcastChannelRegistry {
public:
    NODELETE static Ref<EmptyBroadcastChannelRegistry> create()
    {
        return adoptRef(*new EmptyBroadcastChannelRegistry);
    }
private:
    EmptyBroadcastChannelRegistry() = default;

    NODELETE void registerChannel(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier) final { }
    NODELETE void unregisterChannel(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier) final { }
    NODELETE void postMessage(const PartitionedSecurityOrigin&, const String&, BroadcastChannelIdentifier, Ref<SerializedScriptValue>&&, CompletionHandler<void()>&&) final { }
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
    RefPtr<ThreadableWebSocketChannel> createWebSocketChannel(Document&, WebSocketChannelClient&, IsInitiatedByWorker) final { return nullptr; }

    Ref<WebTransportSession> createWebTransportSession(ScriptExecutionContext&, WebTransportSessionClient&) { return adoptRef(*new EmptyWebTransportSession()); }
};

Ref<SocketProvider> emptySocketProvider()
{
    static NeverDestroyed<Ref<SocketProvider>> provider { adoptRef(*new EmptySocketProvider()) };
    return provider.get();
}

class EmptyHistoryItemClient final : public HistoryItemClient {
public:
    NODELETE static Ref<EmptyHistoryItemClient> create() { return adoptRef(*new EmptyHistoryItemClient); }
private:
    NODELETE void historyItemChanged(const HistoryItem&) { }
    NODELETE void clearChildren(const HistoryItem&) const { }
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
