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

#include "ApplicationCacheStorage.h"
#include "BackForwardClient.h"
#include "CacheStorageProvider.h"
#include "ColorChooser.h"
#include "ContextMenuClient.h"
#include "CookieJar.h"
#include "DOMPasteAccess.h"
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
#include "IDBConnectionToServer.h"
#include "InspectorClient.h"
#include "LibWebRTCProvider.h"
#include "MediaRecorderPrivate.h"
#include "MediaRecorderProvider.h"
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
#include <pal/SessionID.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "CompiledContentExtension.h"
#endif

#if USE(QUICK_LOOK)
#include "LegacyPreviewLoaderClient.h"
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

#if PLATFORM(GTK)
    void insertEmoji(Frame&) final { }
#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    void showContextMenu() final { }
#endif
};

#endif // ENABLE(CONTEXT_MENUS)

class EmptyDatabaseProvider final : public DatabaseProvider {
#if ENABLE(INDEXED_DATABASE)
    struct EmptyIDBConnectionToServerDeletegate final : public IDBClient::IDBConnectionToServerDelegate {
        IDBConnectionIdentifier identifier() const final { return { }; }
        void deleteDatabase(const IDBRequestData&) final { }
        void openDatabase(const IDBRequestData&) final { }
        void abortTransaction(const IDBResourceIdentifier&) final { }
        void commitTransaction(const IDBResourceIdentifier&) final { }
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
        void abortOpenAndUpgradeNeeded(uint64_t, const IDBResourceIdentifier&) final { }
        void didFireVersionChangeEvent(uint64_t, const IDBResourceIdentifier&, const IndexedDB::ConnectionClosedOnBehalfOfServer) final { }
        void openDBRequestCancelled(const IDBRequestData&) final { }
        void getAllDatabaseNamesAndVersions(const IDBResourceIdentifier&, const ClientOrigin&) final { }
        ~EmptyIDBConnectionToServerDeletegate() { }
    };

    IDBClient::IDBConnectionToServer& idbConnectionToServerForSession(const PAL::SessionID&) final
    {
        static NeverDestroyed<EmptyIDBConnectionToServerDeletegate> emptyDelegate;
        static auto& emptyConnection = IDBClient::IDBConnectionToServer::create(emptyDelegate.get()).leakRef();
        return emptyConnection;
    }
#endif
};

class EmptyDiagnosticLoggingClient final : public DiagnosticLoggingClient {
    void logDiagnosticMessage(const String&, const String&, ShouldSample) final { }
    void logDiagnosticMessageWithResult(const String&, const String&, DiagnosticLoggingResultType, ShouldSample) final { }
    void logDiagnosticMessageWithValue(const String&, const String&, double, unsigned, ShouldSample) final { }
    void logDiagnosticMessageWithEnhancedPrivacy(const String&, const String&, ShouldSample) final { }
    void logDiagnosticMessageWithValueDictionary(const String&, const String&, const ValueDictionary&, ShouldSample) final { }
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
    bool shouldDeleteRange(const Optional<SimpleRange>&) final { return false; }
    bool smartInsertDeleteEnabled() final { return false; }
    bool isSelectTrailingWhitespaceEnabled() const final { return false; }
    bool isContinuousSpellCheckingEnabled() final { return false; }
    void toggleContinuousSpellChecking() final { }
    bool isGrammarCheckingEnabled() final { return false; }
    void toggleGrammarChecking() final { }
    int spellCheckerDocumentTag() final { return -1; }

    bool shouldBeginEditing(const SimpleRange&) final { return false; }
    bool shouldEndEditing(const SimpleRange&) final { return false; }
    bool shouldInsertNode(Node&, const Optional<SimpleRange>&, EditorInsertAction) final { return false; }
    bool shouldInsertText(const String&, const Optional<SimpleRange>&, EditorInsertAction) final { return false; }
    bool shouldChangeSelectedRange(const Optional<SimpleRange>&, const Optional<SimpleRange>&, EAffinity, bool) final { return false; }

    bool shouldApplyStyle(const StyleProperties&, const Optional<SimpleRange>&) final { return false; }
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
    void willWriteSelectionToPasteboard(const Optional<SimpleRange>&) final { }
    void didWriteSelectionToPasteboard() final { }
    void getClientPasteboardData(const Optional<SimpleRange>&, Vector<String>&, Vector<RefPtr<SharedBuffer>>&) final { }
    void requestCandidatesForSelection(const VisibleSelection&) final { }
    void handleAcceptedCandidateWithSoftSpaces(TextCheckingResult) final { }

    void registerUndoStep(UndoStep&) final;
    void registerRedoStep(UndoStep&) final;
    void clearUndoRedoOperations() final { }

    DOMPasteAccessResponse requestDOMPasteAccess(const String&) final { return DOMPasteAccessResponse::DeniedForGesture; }

    bool canCopyCut(Frame*, bool defaultValue) const final { return defaultValue; }
    bool canPaste(Frame*, bool defaultValue) const final { return defaultValue; }
    bool canUndo() const final { return false; }
    bool canRedo() const final { return false; }

    void undo() final { }
    void redo() final { }

    void handleKeyboardEvent(KeyboardEvent&) final { }
    void handleInputMethodKeydown(KeyboardEvent&) final { }

    void textFieldDidBeginEditing(Element*) final { }
    void textFieldDidEndEditing(Element*) final { }
    void textDidChangeInTextField(Element*) final { }
    bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) final { return false; }
    void textWillBeDeletedInTextField(Element*) final { }
    void textDidChangeInTextArea(Element*) final { }
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

    bool canShowFontPanel() const final { return false; }

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
    Optional<String> validatedPaymentNetwork(const String&) final { return WTF::nullopt; }
    bool canMakePayments() final { return false; }
    void canMakePaymentsWithActiveCard(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTFMove(completionHandler)]() mutable { completionHandler(false); }); }
    void openPaymentSetup(const String&, const String&, CompletionHandler<void(bool)>&& completionHandler) final { callOnMainThread([completionHandler = WTFMove(completionHandler)]() mutable { completionHandler(false); }); }
    bool showPaymentUI(const URL&, const Vector<URL>&, const ApplePaySessionPaymentRequest&) final { return false; }
    void completeMerchantValidation(const PaymentMerchantSession&) final { }
    void completeShippingMethodSelection(Optional<ShippingMethodUpdate>&&) final { }
    void completeShippingContactSelection(Optional<ShippingContactUpdate>&&) final { }
    void completePaymentMethodSelection(Optional<PaymentMethodUpdate>&&) final { }
    void completePaymentSession(Optional<PaymentAuthorizationResult>&&) final { }
    void cancelPaymentSession() final { }
    void abortPaymentSession() final { }
    void paymentCoordinatorDestroyed() final { }
    bool supportsUnrestrictedApplePay() const final { return false; }
};

#endif

class EmptyPluginInfoProvider final : public PluginInfoProvider {
    void refreshPlugins() final { };
    Vector<PluginInfo> pluginInfo(Page&, Optional<Vector<SupportedPluginIdentifier>>&) final { return { }; }
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
        void setItem(Frame*, const String&, const String&, bool&) final { }
        void removeItem(Frame*, const String&) final { }
        void clear(Frame*) final { }
        bool contains(const String&) final { return false; }
        StorageType storageType() const final { return StorageType::Local; }
        size_t memoryBytesUsedByCache() final { return 0; }
    };

    struct EmptyStorageNamespace final : public StorageNamespace {
        explicit EmptyStorageNamespace(PAL::SessionID sessionID)
            : m_sessionID(sessionID)
        {
        }
    private:
        Ref<StorageArea> storageArea(const SecurityOriginData&) final { return adoptRef(*new EmptyStorageArea); }
        Ref<StorageNamespace> copy(Page&) final { return adoptRef(*new EmptyStorageNamespace { m_sessionID }); }
        PAL::SessionID sessionID() const final { return m_sessionID; }
        void setSessionIDForTesting(PAL::SessionID sessionID) final { m_sessionID = sessionID; };

        PAL::SessionID m_sessionID;
    };

    Ref<StorageNamespace> createSessionStorageNamespace(Page&, unsigned) final;
    Ref<StorageNamespace> createLocalStorageNamespace(unsigned, PAL::SessionID) final;
    Ref<StorageNamespace> createTransientLocalStorageNamespace(SecurityOrigin&, unsigned, PAL::SessionID) final;

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

void EmptyChromeClient::runOpenPanel(Frame&, FileChooser&)
{
}
    
void EmptyChromeClient::showShareSheet(ShareDataWithParsedURL&, CompletionHandler<void(bool)>&&)
{
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

RefPtr<Frame> EmptyFrameLoaderClient::createFrame(const String&, HTMLFrameOwnerElement&)
{
    return nullptr;
}

RefPtr<Widget> EmptyFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement&, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    return nullptr;
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

Ref<StorageNamespace> EmptyStorageNamespaceProvider::createSessionStorageNamespace(Page& page, unsigned)
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
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    std::unique_ptr<MediaRecorderPrivate> createMediaRecorderPrivate(MediaStreamPrivate&) final { return nullptr; }
#endif
};

PageConfiguration pageConfigurationWithEmptyClients(PAL::SessionID sessionID)
{
    PageConfiguration pageConfiguration {
        sessionID,
        makeUniqueRef<EmptyEditorClient>(),
        SocketProvider::create(),
        LibWebRTCProvider::create(),
        CacheStorageProvider::create(),
        adoptRef(*new EmptyBackForwardClient),
        CookieJar::create(adoptRef(*new EmptyStorageSessionProvider)),
        makeUniqueRef<EmptyProgressTrackerClient>(),
        makeUniqueRef<EmptyFrameLoaderClient>(),
        makeUniqueRef<EmptyMediaRecorderProvider>()
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
