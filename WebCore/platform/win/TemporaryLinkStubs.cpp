/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#define WIN32_COMPILE_HACK

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "AXObjectCache.h"
#include "CachedResource.h"
#include "ChromeClientWin.h"
#include "ContextMenu.h"
#include "ContextMenuClientWin.h"
#include "CookieJar.h"
#include "CString.h"
#include "Cursor.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "EditorClientWin.h"
#include "EventHandler.h"
#include "FileChooser.h"
#include "Font.h"
#include "FrameLoader.h"
#include "FrameLoaderClientWin.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "GraphicsContext.h"
#include "History.h"
#include "Icon.h"
#include "IconLoader.h"
#include "IconDatabase.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "loader.h"
#include "LocalizedStrings.h"
#include "Node.h"
#include "Page.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PlatformScrollBar.h"
#include "PlugInInfoStore.h"
#include "PopupMenu.h"
#include "RenderTheme.h"
#include "ResourceLoader.h"
#include "RenderThemeWin.h"
#include "Screen.h"
#include "ScrollBar.h"
#include "TextBoundaries.h"
#include "TextField.h"
#include "Widget.h"

#define notImplemented() do { \
    char buf[256] = {0}; \
    _snprintf(buf, sizeof(buf), "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); \
    OutputDebugStringA(buf); \
} while (0)

namespace WebCore {

static int frameNumber = 0;
static Cursor localCursor;

time_t CacheObjectExpiresTime(DocLoader*, PlatformResponse) { notImplemented(); return 0; }
void CheckCacheObjectStatus(DocLoader*, CachedResource*) { notImplemented(); }
bool CheckIfReloading(DocLoader*) { notImplemented(); return false; }
String defaultLanguage() { notImplemented(); return "en"; }
String fileButtonChooseFileLabel() { notImplemented(); return "Browse..."; }
String fileButtonNoFileSelectedLabel() { notImplemented(); return String(); }
int findNextSentenceFromIndex(UChar const*, int, int, bool) { notImplemented(); return 0; }
int findNextWordFromIndex(UChar const*, int, int, bool) { notImplemented(); return 0; }
void findSentenceBoundary(UChar const*, int, int, int*, int*) { notImplemented(); }
void findWordBoundary(UChar const* str, int len, int position, int* start, int* end) { notImplemented(); *start = position; *end = position; }
Color focusRingColor() { notImplemented(); return 0xFF0000FF; }
bool historyContains(DeprecatedString const&) { notImplemented(); return false; }
String inputElementAltText() { notImplemented(); return DeprecatedString(); }
bool IsResponseURLEqualToURL(PlatformResponse , const String&) { notImplemented(); return false; }
const Cursor& aliasCursor() { notImplemented(); return localCursor; }
const Cursor& cellCursor() { notImplemented(); return localCursor; }
const Cursor& contextMenuCursor() { notImplemented(); return localCursor; }
const Cursor& moveCursor() { notImplemented(); return localCursor; }
const Cursor& noDropCursor() { notImplemented(); return localCursor; }
const Cursor& progressCursor() { notImplemented(); return localCursor; }
const Cursor& verticalTextCursor() { notImplemented(); return localCursor; }
void refreshPlugins(bool) { notImplemented(); }
String resetButtonDefaultLabel() { notImplemented(); return "Reset"; }
bool ResponseIsMultipart(PlatformResponse) { notImplemented(); return false; }
DeprecatedString ResponseMIMEType(PlatformResponse) { notImplemented(); return DeprecatedString(); }
DeprecatedString ResponseURL(PlatformResponse) { notImplemented(); return DeprecatedString(); }
int screenDepthPerComponent(Widget*) { notImplemented(); return 8; }
bool screenIsMonochrome(Widget*) { notImplemented(); return false; }
String searchableIndexIntroduction() { notImplemented(); return String(); }
Vector<char> ServeSynchronousRequest(Loader*, DocLoader*, const ResourceRequest&, ResourceResponse&) { notImplemented(); return Vector<char>(); }
void setFocusRingColorChangeFunction(void (*)()) { notImplemented(); }
String submitButtonDefaultLabel() { notImplemented(); return "Submit"; }

bool AXObjectCache::gAccessibilityEnabled = false;

void CachedResource::setPlatformResponse(PlatformResponse) { notImplemented(); }
void CachedResource::setAllData(PlatformData) { notImplemented(); }

void ChromeClientWin::setWindowRect(const FloatRect&) { notImplemented(); }
FloatRect ChromeClientWin::windowRect() { notImplemented(); return FloatRect(); }
FloatRect ChromeClientWin::pageRect() { notImplemented(); return FloatRect(); }
float ChromeClientWin::scaleFactor() { notImplemented(); return 0.0; }
void ChromeClientWin::focus() { notImplemented(); }
void ChromeClientWin::unfocus() { notImplemented(); }
Page* ChromeClientWin::createWindow(const FrameLoadRequest&) { notImplemented(); return 0; }
Page* ChromeClientWin::createModalDialog(const FrameLoadRequest&) { notImplemented(); return 0; }
void ChromeClientWin::show() { notImplemented(); }
bool ChromeClientWin::canRunModal() { notImplemented(); return false; }
void ChromeClientWin::runModal() { notImplemented(); }
void ChromeClientWin::setToolbarsVisible(bool) { notImplemented(); }
bool ChromeClientWin::toolbarsVisible() { notImplemented(); return false; }
void ChromeClientWin::setStatusbarVisible(bool) { notImplemented(); }
bool ChromeClientWin::statusbarVisible() { notImplemented(); return false; }
void ChromeClientWin::setScrollbarsVisible(bool) { notImplemented(); }
bool ChromeClientWin::scrollbarsVisible() { notImplemented(); return false; }
void ChromeClientWin::setMenubarVisible(bool) { notImplemented(); }
bool ChromeClientWin::menubarVisible() { notImplemented(); return false; }
void ChromeClientWin::setResizable(bool) { notImplemented(); }

void ContextMenu::appendItem(ContextMenuItem item) { notImplemented(); }
void ContextMenu::show() { notImplemented(); }

void ContextMenuClientWin::addCustomContextMenuItems(ContextMenu*) { notImplemented(); }
void ContextMenuClientWin::copyLinkToClipboard(HitTestResult) { notImplemented(); }
void ContextMenuClientWin::downloadURL(KURL) { notImplemented(); }
void ContextMenuClientWin::copyImageToClipboard(HitTestResult) { notImplemented(); }
void ContextMenuClientWin::lookUpInDictionary(Frame*) { notImplemented(); }

void DocumentLoader::setFrame(Frame*) { notImplemented(); }
FrameLoader* DocumentLoader::frameLoader() const { notImplemented(); return m_frame->loader(); }
KURL DocumentLoader::URL() const { notImplemented(); return KURL(); }
bool DocumentLoader::isStopping() const { notImplemented(); return false; }
void DocumentLoader::stopLoading() { notImplemented(); }
void DocumentLoader::setLoading(bool) { notImplemented(); }
void DocumentLoader::updateLoading() { notImplemented(); }
void DocumentLoader::setupForReplaceByMIMEType(const String& newMIMEType) { notImplemented(); }
bool DocumentLoader::isLoadingInAPISense() const { notImplemented(); return false; }
void DocumentLoader::stopRecordingResponses() { notImplemented(); }

bool EditorClientWin::shouldDeleteRange(Range*) { notImplemented(); return false; }
bool EditorClientWin::shouldShowDeleteInterface(HTMLElement*) { notImplemented(); return false; }
bool EditorClientWin::isContinuousSpellCheckingEnabled() { notImplemented(); return false; }
bool EditorClientWin::isGrammarCheckingEnabled() { notImplemented(); return false; }
int EditorClientWin::spellCheckerDocumentTag() { notImplemented(); return 0; }
bool EditorClientWin::selectWordBeforeMenuEvent() { notImplemented(); return false; }
bool EditorClientWin::isEditable() { notImplemented(); return false; }
bool EditorClientWin::shouldBeginEditing(Range*) { notImplemented(); return false; }
bool EditorClientWin::shouldEndEditing(Range*) { notImplemented(); return false; }
bool EditorClientWin::shouldInsertText(String, Range*, EditorInsertAction) { notImplemented(); return false; }
bool EditorClientWin::shouldApplyStyle(CSSStyleDeclaration*, Range*) { notImplemented(); return false; }
void EditorClientWin::didBeginEditing() { notImplemented(); }
void EditorClientWin::respondToChangedContents() { notImplemented(); }
void EditorClientWin::didEndEditing() { notImplemented(); }
void EditorClientWin::registerCommandForUndo(PassRefPtr<EditCommand>) { notImplemented(); }
void EditorClientWin::registerCommandForRedo(PassRefPtr<EditCommand>) { notImplemented(); }
void EditorClientWin::clearUndoRedoOperations() { notImplemented(); }
bool EditorClientWin::canUndo() const { notImplemented(); return false; }
bool EditorClientWin::canRedo() const { notImplemented(); return false; }
void EditorClientWin::undo() { notImplemented(); }
void EditorClientWin::redo() { notImplemented(); }

void EventHandler::focusDocumentView() { notImplemented(); }
bool EventHandler::handleDrag(const MouseEventWithHitTestResults&) { notImplemented(); return false; }
bool EventHandler::handleMouseUp(const MouseEventWithHitTestResults&) { notImplemented(); return false; }
bool EventHandler::lastEventIsMouseUp() const { notImplemented(); return false; }
bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe) { notImplemented(); return true; }
bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe) { notImplemented(); return true; }
bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe) { notImplemented(); return true; }
bool EventHandler::passWheelEventToSubframe(PlatformWheelEvent& e, Frame* subframe) { notImplemented(); return false; }
bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&) { notImplemented(); return false; }
bool EventHandler::passMousePressEventToScrollbar(MouseEventWithHitTestResults&, PlatformScrollbar*) { notImplemented(); return false; }
bool EventHandler::shouldDragAutoNode(Node*, const IntPoint&) const { notImplemented(); return false; }
bool EventHandler::tabsToAllControls(KeyboardEvent*) const { notImplemented(); return false; }
bool EventHandler::tabsToLinks(KeyboardEvent*) const { notImplemented(); return false; }

FileChooser::FileChooser(Document*, RenderFileUploadControl*) { notImplemented(); }
FileChooser::~FileChooser() { notImplemented(); }
PassRefPtr<FileChooser> FileChooser::create(Document*, RenderFileUploadControl*) { notImplemented(); return 0; }
void FileChooser::openFileChooser() { notImplemented(); }
String FileChooser::basenameForWidth(int width) const { notImplemented(); return String(); }
void FileChooser::disconnectUploadControl() { notImplemented(); }
void FileChooser::chooseFile(const String& filename) { notImplemented(); }

void Frame::setNeedsReapplyStyles() { notImplemented(); }

void FrameLoader::didFirstLayout() { notImplemented(); }
String FrameLoader::overrideMediaType() const { notImplemented(); return String(); }
Widget* FrameLoader::createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>&) { notImplemented(); return 0; }
void FrameLoader::redirectDataToPlugin(Widget* pluginWidget) { notImplemented(); }
int FrameLoader::getHistoryLength() { notImplemented(); return 0; }
void FrameLoader::setTitle(const String& title) { notImplemented(); }
String FrameLoader::referrer() const { notImplemented(); return String(); }
void FrameLoader::saveDocumentState() { notImplemented(); }
void FrameLoader::restoreDocumentState() { notImplemented(); }
void FrameLoader::goBackOrForward(int distance) { notImplemented(); }
KURL FrameLoader::historyURL(int distance) { notImplemented(); return KURL();}
void FrameLoader::urlSelected(const FrameLoadRequest&, Event*) { notImplemented(); }
Frame* FrameLoader::createFrame(KURL const&, String const&, Element*, String const&) { notImplemented(); return 0; }
void FrameLoader::submitForm(const FrameLoadRequest&, Event*) { notImplemented(); }
void FrameLoader::partClearedInBegin() { notImplemented(); }
KURL FrameLoader::originalRequestURL() const { notImplemented(); return KURL(); }
bool FrameLoader::canGoBackOrForward(int) const { notImplemented(); return false; }
ObjectContentType FrameLoader::objectContentType(const KURL&, const String&) { notImplemented(); return ObjectContentNone; }
Widget* FrameLoader::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&) { notImplemented(); return 0; }
void FrameLoader::detachFromParent() { notImplemented(); }
void FrameLoader::checkLoadCompleteForThisFrame() { notImplemented(); }
void FrameLoader::reload() { notImplemented(); }

bool FrameLoaderClientWin::hasWebView() const { notImplemented(); return false; }
bool FrameLoaderClientWin::hasFrameView() const { notImplemented(); return false; }
bool FrameLoaderClientWin::hasBackForwardList() const { notImplemented(); return false; }
void FrameLoaderClientWin::resetBackForwardList() { notImplemented(); }
bool FrameLoaderClientWin::provisionalItemIsTarget() const { notImplemented(); return false; }
bool FrameLoaderClientWin::loadProvisionalItemFromPageCache() { notImplemented(); return false; }
void FrameLoaderClientWin::invalidateCurrentItemPageCache() { notImplemented(); }
bool FrameLoaderClientWin::privateBrowsingEnabled() const { notImplemented(); return false; }
void FrameLoaderClientWin::makeDocumentView() { notImplemented(); }
void FrameLoaderClientWin::makeRepresentation(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientWin::forceLayout() { notImplemented(); }
void FrameLoaderClientWin::forceLayoutForNonHTML() { notImplemented(); }
void FrameLoaderClientWin::updateHistoryForCommit() { notImplemented(); }
void FrameLoaderClientWin::updateHistoryForBackForwardNavigation() { notImplemented(); }
void FrameLoaderClientWin::updateHistoryForReload() { notImplemented(); }
void FrameLoaderClientWin::updateHistoryForStandardLoad() { notImplemented(); }
void FrameLoaderClientWin::updateHistoryForInternalLoad() { notImplemented(); }
void FrameLoaderClientWin::updateHistoryAfterClientRedirect() { notImplemented(); }
void FrameLoaderClientWin::setCopiesOnScroll() { notImplemented(); }
LoadErrorResetToken* FrameLoaderClientWin::tokenForLoadErrorReset() { notImplemented(); return 0; }
void FrameLoaderClientWin::resetAfterLoadError(LoadErrorResetToken*) { notImplemented(); }
void FrameLoaderClientWin::doNotResetAfterLoadError(LoadErrorResetToken*) { notImplemented(); }
void FrameLoaderClientWin::willCloseDocument() { notImplemented(); }
void FrameLoaderClientWin::detachedFromParent1() { notImplemented(); }
void FrameLoaderClientWin::detachedFromParent2() { notImplemented(); }
void FrameLoaderClientWin::detachedFromParent3() { notImplemented(); }
void FrameLoaderClientWin::detachedFromParent4() { notImplemented(); }
void FrameLoaderClientWin::loadedFromPageCache() { notImplemented(); }
void FrameLoaderClientWin::dispatchDidHandleOnloadEvents() { notImplemented(); }
void FrameLoaderClientWin::dispatchDidReceiveServerRedirectForProvisionalLoad() { notImplemented(); }
void FrameLoaderClientWin::dispatchDidCancelClientRedirect() { notImplemented(); }
void FrameLoaderClientWin::dispatchWillPerformClientRedirect(const KURL&, double, double) { notImplemented(); }
void FrameLoaderClientWin::dispatchDidChangeLocationWithinPage() { notImplemented(); }
void FrameLoaderClientWin::dispatchWillClose() { notImplemented(); }
void FrameLoaderClientWin::dispatchDidReceiveIcon() { notImplemented(); }
void FrameLoaderClientWin::dispatchDidStartProvisionalLoad() { notImplemented(); }
void FrameLoaderClientWin::dispatchDidReceiveTitle(const String&) { notImplemented(); }
void FrameLoaderClientWin::dispatchDidCommitLoad() { notImplemented(); }
void FrameLoaderClientWin::dispatchDidFinishLoad() { notImplemented(); }
void FrameLoaderClientWin::dispatchDidFirstLayout() { notImplemented(); }
void FrameLoaderClientWin::dispatchShow() { notImplemented(); }
void FrameLoaderClientWin::cancelPolicyCheck() { notImplemented(); }
void FrameLoaderClientWin::dispatchWillSubmitForm(FramePolicyFunction, PassRefPtr<FormState>) { notImplemented(); }
void FrameLoaderClientWin::dispatchDidLoadMainResource(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientWin::clearLoadingFromPageCache(DocumentLoader*) { notImplemented(); }
bool FrameLoaderClientWin::isLoadingFromPageCache(DocumentLoader*) { notImplemented(); return false; }
void FrameLoaderClientWin::revertToProvisionalState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientWin::clearUnarchivingState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientWin::progressStarted() { notImplemented(); }
void FrameLoaderClientWin::progressCompleted() { notImplemented(); }
void FrameLoaderClientWin::setMainFrameDocumentReady(bool) { notImplemented(); }
void FrameLoaderClientWin::willChangeTitle(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientWin::didChangeTitle(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientWin::finishedLoading(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientWin::finalSetupForReplace(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientWin::setDefersLoading(bool) { notImplemented(); }
bool FrameLoaderClientWin::isArchiveLoadPending(ResourceLoader*) const { notImplemented(); return false; }
void FrameLoaderClientWin::cancelPendingArchiveLoad(ResourceLoader*) { notImplemented(); }
void FrameLoaderClientWin::clearArchivedResources() { notImplemented(); }
bool FrameLoaderClientWin::canShowMIMEType(const String&) const { notImplemented(); return false; }
bool FrameLoaderClientWin::representationExistsForURLScheme(const String&) const { notImplemented(); return false; }
String FrameLoaderClientWin::generatedMIMETypeForURLScheme(const String&) const { notImplemented(); return String(); }
void FrameLoaderClientWin::frameLoadCompleted() { notImplemented(); }
void FrameLoaderClientWin::restoreScrollPositionAndViewState() { notImplemented(); }
void FrameLoaderClientWin::provisionalLoadStarted() { notImplemented(); }
bool FrameLoaderClientWin::shouldTreatURLAsSameAsCurrent(const KURL&) const { notImplemented(); return false; }
void FrameLoaderClientWin::addHistoryItemForFragmentScroll() { notImplemented(); }
void FrameLoaderClientWin::didFinishLoad() { notImplemented(); }
void FrameLoaderClientWin::prepareForDataSourceReplacement() { notImplemented(); }
void FrameLoaderClientWin::setTitle(const String&, const KURL&) { notImplemented(); }
String FrameLoaderClientWin::userAgent() { notImplemented(); return String(); }

void FrameView::updateBorder() { notImplemented(); }

void FrameWin::focusWindow() { notImplemented(); }
void FrameWin::unfocusWindow() { notImplemented(); }
bool FrameWin::locationbarVisible() { notImplemented(); return 0; }
void FrameWin::issueRedoCommand(void) { notImplemented(); }
KJS::Bindings::Instance* FrameWin::getObjectInstanceForWidget(Widget*) { notImplemented(); return 0; }
KJS::Bindings::Instance* FrameWin::getEmbedInstanceForWidget(Widget*) { notImplemented(); return 0; }
KJS::Bindings::RootObject* FrameWin::bindingRootObject() { notImplemented(); return 0; }
bool FrameWin::canRedo() const { notImplemented(); return 0; }
bool FrameWin::canUndo() const { notImplemented(); return 0; }
void FrameWin::registerCommandForUndo(PassRefPtr<WebCore::EditCommand>) { notImplemented(); }
void FrameWin::registerCommandForRedo(PassRefPtr<WebCore::EditCommand>) { notImplemented(); }
bool FrameWin::runJavaScriptPrompt(String const&, String const&, String &) { notImplemented(); return 0; }
bool FrameWin::shouldInterruptJavaScript() { notImplemented(); return false; }
void FrameWin::print() { notImplemented(); }
KJS::Bindings::Instance* FrameWin::getAppletInstanceForWidget(Widget*) { notImplemented(); return 0; }
bool FrameWin::passMouseDownEventToWidget(Widget*) { notImplemented(); return 0; }
void FrameWin::issueCutCommand() { notImplemented(); }
void FrameWin::issueCopyCommand() { notImplemented(); }
bool FrameWin::passWheelEventToChildWidget(Node*) { notImplemented(); return 0; }
void FrameWin::issueUndoCommand() { notImplemented(); }
String FrameWin::mimeTypeForFileName(String const&) const { notImplemented(); return String(); }
void FrameWin::issuePasteCommand() { notImplemented(); }
void FrameWin::scheduleClose() { notImplemented(); }
void FrameWin::markMisspellings(const WebCore::Selection&) { notImplemented(); }
bool FrameWin::menubarVisible() { notImplemented(); return 0; }
bool FrameWin::personalbarVisible() { notImplemented(); return 0; }
bool FrameWin::statusbarVisible() { notImplemented(); return 0; }
bool FrameWin::toolbarVisible() { notImplemented(); return 0; }
void FrameWin::issueTransposeCommand() { notImplemented(); }
bool FrameWin::canPaste() const { notImplemented(); return 0; }
void FrameWin::issuePasteAndMatchStyleCommand() { notImplemented(); }
bool FrameWin::isLoadTypeReload() { notImplemented(); return false; }
Range* FrameWin::markedTextRange() const { notImplemented(); return 0; }
bool FrameWin::passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame*) { notImplemented(); return false; }
bool FrameWin::lastEventIsMouseUp() const { notImplemented(); return false; }
void FrameWin::addMessageToConsole(String const&, unsigned int, String const&) { notImplemented(); }
bool FrameWin::shouldChangeSelection(const Selection&, const Selection&, EAffinity, bool) const { notImplemented(); return true; }
void FrameWin::respondToChangedSelection(const Selection&, bool) { notImplemented(); }
void FrameWin::clearUndoRedoOperations(void) { notImplemented(); }
void FrameWin::markMisspellingsInAdjacentWords(VisiblePosition const&) { notImplemented(); }
void FrameWin::respondToChangedContents(const Selection&) { notImplemented(); }
void FrameWin::ignoreSpelling() { notImplemented(); }
void FrameWin::learnSpelling() { notImplemented(); }

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight) { notImplemented(); }
void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness) { notImplemented(); }
void GraphicsContext::setShadow(IntSize const&,int,Color const&) { notImplemented(); }
void GraphicsContext::clearShadow() { notImplemented(); }
void GraphicsContext::beginTransparencyLayer(float) { notImplemented(); }
void GraphicsContext::endTransparencyLayer() { notImplemented(); }
void GraphicsContext::clearRect(const FloatRect&) { notImplemented(); }
void GraphicsContext::strokeRect(const FloatRect&, float) { notImplemented(); }
void GraphicsContext::setLineWidth(float) { notImplemented(); }
void GraphicsContext::setLineCap(LineCap) { notImplemented(); }
void GraphicsContext::setLineJoin(LineJoin) { notImplemented(); }
void GraphicsContext::setMiterLimit(float) { notImplemented(); }
void GraphicsContext::setAlpha(float) { notImplemented(); }
void GraphicsContext::setCompositeOperation(CompositeOperator) { notImplemented(); }
void GraphicsContext::clip(const Path&) { notImplemented(); }
void GraphicsContext::rotate(float) { notImplemented(); }
void GraphicsContext::scale(const FloatSize&) { notImplemented(); }

Icon::Icon() { notImplemented(); }
Icon::~Icon() { notImplemented(); }
PassRefPtr<Icon> Icon::newIconForFile(const String& filename) { notImplemented(); return PassRefPtr<Icon>(new Icon()); }
void Icon::paint(GraphicsContext*, const IntRect&) { notImplemented(); }

bool IconDatabase::isIconExpiredForIconURL(const String& url) { notImplemented(); return false; }
bool IconDatabase::hasEntryForIconURL(const String& url) { notImplemented(); return false; }
IconDatabase* IconDatabase::sharedIconDatabase() { notImplemented(); return 0; }
bool IconDatabase::setIconURLForPageURL(const String& iconURL, const String& pageURL) { notImplemented(); return false; }
void IconDatabase::setIconDataForIconURL(const void* data, int size, const String&) { notImplemented(); }

void Image::drawTiled(GraphicsContext*, const FloatRect&, const FloatRect&, TileRule, TileRule, CompositeOperator) { notImplemented(); }
bool Image::getHBITMAP(HBITMAP) { notImplemented(); return false; }

HINSTANCE Page::s_instanceHandle = 0;

Path::Path() { notImplemented(); }
Path::~Path() { notImplemented(); }
Path::Path(const Path&) { notImplemented(); }
bool Path::contains(const FloatPoint&, WindRule rule) const { notImplemented(); return false; }
void Path::translate(const FloatSize&) { notImplemented(); }
FloatRect Path::boundingRect() const { notImplemented(); return FloatRect(); }
Path& Path::operator=(const Path&) { notImplemented(); return*this; }
void Path::clear() { notImplemented(); }
void Path::moveTo(const FloatPoint&) { notImplemented(); }
void Path::addLineTo(const FloatPoint&) { notImplemented(); }
void Path::addQuadCurveTo(const FloatPoint&, const FloatPoint&) { notImplemented(); }
void Path::addBezierCurveTo(const FloatPoint&, const FloatPoint&, const FloatPoint&) { notImplemented(); }
void Path::addArcTo(const FloatPoint&, const FloatPoint&, float) { notImplemented(); }
void Path::closeSubpath() { notImplemented(); }
void Path::addArc(const FloatPoint&, float, float, float, bool) { notImplemented(); }
void Path::addRect(const FloatRect&) { notImplemented(); }
void Path::addEllipse(const FloatRect&) { notImplemented(); }
void Path::transform(const AffineTransform&) { notImplemented(); }

PlatformMouseEvent::PlatformMouseEvent(const CurrentEventTag&) { notImplemented(); }

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize) : Scrollbar(client, orientation, controlSize) { notImplemented(); }
PlatformScrollbar::~PlatformScrollbar() { notImplemented(); }
int PlatformScrollbar::width() const { notImplemented(); return 0; }
int PlatformScrollbar::height() const { notImplemented(); return 0; }
void PlatformScrollbar::setEnabled(bool) { notImplemented(); }
void PlatformScrollbar::paint(GraphicsContext*, const IntRect& damageRect) { notImplemented(); }
void PlatformScrollbar::updateThumbPosition() { notImplemented(); }
void PlatformScrollbar::updateThumbProportion() { notImplemented(); }
void PlatformScrollbar::setRect(const IntRect&) { notImplemented(); }

PluginInfo* PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplemented(); return 0;}
unsigned PlugInInfoStore::pluginCount() const { notImplemented(); return 0; }
bool PlugInInfoStore::supportsMIMEType(const String&) { notImplemented(); return false; }

PolicyCheck::PolicyCheck() { notImplemented(); }
void PolicyCheck::clear() { notImplemented(); }
void PolicyCheck::clearRequest() { notImplemented(); }
void PolicyCheck::call() { notImplemented(); }
void PolicyCheck::call(PolicyAction) { notImplemented(); }

PopupMenu::PopupMenu(RenderMenuList*) { notImplemented(); }
PopupMenu::~PopupMenu() { notImplemented(); }
void PopupMenu::show(const IntRect&, FrameView*, int index) { notImplemented(); }
void PopupMenu::hide() { notImplemented(); }
void PopupMenu::updateFromElement() { notImplemented(); }

void RenderThemeWin::systemFont(int propId, FontDescription& fontDescription) const { notImplemented(); }
bool RenderThemeWin::paintMenuList(RenderObject *, const RenderObject::PaintInfo&, const IntRect&) { notImplemented(); return false; }
void RenderThemeWin::adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const { notImplemented(); }

void ResourceLoader::cancel() { notImplemented(); }
bool ResourceLoader::loadsBlocked() { notImplemented(); return false; }

void ScrollView::addChild(Widget*) { notImplemented(); }
void ScrollView::removeChild(Widget*) { notImplemented(); }
void ScrollView::scrollPointRecursively(int x, int y) { notImplemented(); }
bool ScrollView::inWindow() const { notImplemented(); return true; }
void ScrollView::paint(GraphicsContext*, const IntRect&) { notImplemented(); }
void ScrollView::wheelEvent(PlatformWheelEvent&) { notImplemented(); }
void ScrollView::themeChanged() { notImplemented(); }
IntPoint ScrollView::convertChildToSelf(const Widget*, const IntPoint& p) const { notImplemented(); return p; }
IntPoint ScrollView::convertSelfToChild(const Widget*, const IntPoint& p) const { notImplemented(); return p; }
void ScrollView::geometryChanged() const { notImplemented(); };
PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent) { notImplemented(); return 0; }
void ScrollView::setFrameGeometry(const IntRect& r) { notImplemented(); Widget::setFrameGeometry(r); }
IntRect ScrollView::windowResizerRect() { notImplemented(); return IntRect(); }
bool ScrollView::resizerOverlapsContent() const { notImplemented(); return false; }

void TextField::selectAll() { notImplemented(); }
void TextField::addSearchResult() { notImplemented(); }
int TextField::selectionStart() const { notImplemented(); return 0; }
bool TextField::hasSelectedText() const { notImplemented(); return 0; }
String TextField::selectedText() const { notImplemented(); return String(); }
void TextField::setAutoSaveName(String const&) { notImplemented(); }
bool TextField::checksDescendantsForFocus() const { notImplemented(); return false; }
void TextField::setSelection(int,int) { notImplemented(); }
void TextField::setMaxResults(int) { notImplemented(); }
bool TextField::edited() const { notImplemented(); return 0; }
Widget::FocusPolicy TextField::focusPolicy() const { notImplemented(); return NoFocus; }
TextField::TextField() { notImplemented(); }
TextField::~TextField() { notImplemented(); }
void TextField::setFont(WebCore::Font const&) { notImplemented(); }
void TextField::setAlignment(HorizontalAlignment) { notImplemented(); }
void TextField::setWritingDirection(TextDirection) { notImplemented(); }
int TextField::maxLength() const { notImplemented(); return 0; }
void TextField::setMaxLength(int) { notImplemented(); }
String TextField::text() const { notImplemented(); return String(); }
void TextField::setText(String const&) { notImplemented(); }
int TextField::cursorPosition() const { notImplemented(); return 0; }
void TextField::setCursorPosition(int) { notImplemented(); }
void TextField::setEdited(bool) { notImplemented(); }
void TextField::setReadOnly(bool) { notImplemented(); }
void TextField::setPlaceholderString(String const&) { notImplemented(); }
void TextField::setColors(Color const&,Color const&) { notImplemented(); }
IntSize TextField::sizeForCharacterWidth(int) const { notImplemented(); return IntSize(); }
int TextField::baselinePosition(int) const { notImplemented(); return 0; }
void TextField::setLiveSearch(bool) { notImplemented(); }

void Widget::enableFlushDrawing() { notImplemented(); }
bool Widget::isEnabled() const { notImplemented(); return 0; }
Widget::FocusPolicy Widget::focusPolicy() const { notImplemented(); return NoFocus; }
void Widget::disableFlushDrawing() { notImplemented(); }
void Widget::removeFromParent() { notImplemented(); }
GraphicsContext* Widget::lockDrawingFocus() { notImplemented(); return 0; }
void Widget::unlockDrawingFocus(GraphicsContext*) { notImplemented(); }
bool Widget::capturingMouse() const { notImplemented(); return false; }
void Widget::setCapturingMouse(bool capturingMouse) { notImplemented(); }
Widget* Widget::capturingTarget() { notImplemented(); return this; }
Widget* Widget::capturingChild() { notImplemented(); return 0; }
void Widget::setCapturingChild(Widget* w) { notImplemented(); }
IntPoint Widget::convertChildToSelf(const Widget*, const IntPoint& p) const { notImplemented(); return p; }
IntPoint Widget::convertSelfToChild(const Widget*, const IntPoint& p) const { notImplemented(); return p; }
void Widget::setParent(ScrollView*) { notImplemented(); }
ScrollView* Widget::parent() const { notImplemented(); return 0; }
void Widget::setEnabled(bool) { notImplemented(); }
void Widget::paint(GraphicsContext*,IntRect const&) { notImplemented(); }
void Widget::setIsSelected(bool) { notImplemented(); }
void Widget::invalidate() { notImplemented(); }
void Widget::invalidateRect(const IntRect& r) { notImplemented(); }

} // namespace WebCore
