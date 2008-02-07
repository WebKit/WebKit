/*
 * Copyright (C) 2006, 2007 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "Page.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "ContextMenuClient.h"
#include "ContextMenuController.h"
#include "EditorClient.h"
#include "DragController.h"
#include "FileSystem.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "InspectorController.h"
#include "Logging.h"
#include "ProgressTracker.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "Settings.h"
#include "StringHash.h"
#include "TextResourceDecoder.h"
#include "Widget.h"
#include <kjs/collector.h>
#include <kjs/JSLock.h>
#include <wtf/HashMap.h>

using namespace KJS;

namespace WebCore {

static HashSet<Page*>* allPages;
static HashMap<String, HashSet<Page*>*>* frameNamespaces;

#ifndef NDEBUG
WTFLogChannel LogWebCorePageLeaks =  { 0x00000000, "", WTFLogChannelOn };

struct PageCounter { 
    static int count; 
    ~PageCounter() 
    { 
        if (count)
            LOG(WebCorePageLeaks, "LEAK: %d Page\n", count);
    }
};
int PageCounter::count = 0;
static PageCounter pageCounter;
#endif

Page::Page(ChromeClient* chromeClient, ContextMenuClient* contextMenuClient, EditorClient* editorClient, DragClient* dragClient, InspectorClient* inspectorClient)
    : m_chrome(new Chrome(this, chromeClient))
    , m_dragCaretController(new SelectionController(0, true))
    , m_dragController(new DragController(this, dragClient))
    , m_focusController(new FocusController(this))
    , m_contextMenuController(new ContextMenuController(this, contextMenuClient))
    , m_inspectorController(new InspectorController(this, inspectorClient))
    , m_settings(new Settings(this))
    , m_progress(new ProgressTracker)
    , m_backForwardList(new BackForwardList(this))
    , m_editorClient(editorClient)
    , m_frameCount(0)
    , m_tabKeyCyclesThroughElements(true)
    , m_defersLoading(false)
    , m_inLowQualityInterpolationMode(false)
    , m_parentInspectorController(0)
    , m_didLoadUserStyleSheet(false)
    , m_userStyleSheetModificationTime(0)
{
    if (!allPages) {
        allPages = new HashSet<Page*>;
        setFocusRingColorChangeFunction(setNeedsReapplyStyles);
    }

    ASSERT(!allPages->contains(this));
    allPages->add(this);

#ifndef NDEBUG
    ++PageCounter::count;
#endif
}

Page::~Page()
{
    m_mainFrame->setView(0);
    setGroupName(String());
    allPages->remove(this);
    
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->pageDestroyed();
    m_editorClient->pageDestroyed();
    m_inspectorController->pageDestroyed();

    m_backForwardList->close();

#ifndef NDEBUG
    --PageCounter::count;

    // Cancel keepAlive timers, to ensure we release all Frames before exiting.
    // It's safe to do this because we prohibit closing a Page while JavaScript
    // is executing.
    Frame::cancelAllKeepAlive();
#endif
}

void Page::setMainFrame(PassRefPtr<Frame> mainFrame)
{
    ASSERT(!m_mainFrame); // Should only be called during initialization
    m_mainFrame = mainFrame;
}

BackForwardList* Page::backForwardList()
{
    return m_backForwardList.get();
}

bool Page::goBack()
{
    HistoryItem* item = m_backForwardList->backItem();
    
    if (item) {
        goToItem(item, FrameLoadTypeBack);
        return true;
    }
    return false;
}

bool Page::goForward()
{
    HistoryItem* item = m_backForwardList->forwardItem();
    
    if (item) {
        goToItem(item, FrameLoadTypeForward);
        return true;
    }
    return false;
}

void Page::goToItem(HistoryItem* item, FrameLoadType type)
{
    // Abort any current load if we're going to a history item
    m_mainFrame->loader()->stopAllLoaders();
    m_mainFrame->loader()->goToItem(item, type);
}

void Page::setGroupName(const String& name)
{
    if (frameNamespaces && !m_groupName.isEmpty()) {
        HashSet<Page*>* oldNamespace = frameNamespaces->get(m_groupName);
        if (oldNamespace) {
            oldNamespace->remove(this);
            if (oldNamespace->isEmpty()) {
                frameNamespaces->remove(m_groupName);
                delete oldNamespace;
            }
        }
    }
    m_groupName = name;
    if (!name.isEmpty()) {
        if (!frameNamespaces)
            frameNamespaces = new HashMap<String, HashSet<Page*>*>;
        HashSet<Page*>* newNamespace = frameNamespaces->get(name);
        if (!newNamespace) {
            newNamespace = new HashSet<Page*>;
            frameNamespaces->add(name, newNamespace);
        }
        newNamespace->add(this);
    }
}

const HashSet<Page*>* Page::frameNamespace() const
{
    return (frameNamespaces && !m_groupName.isEmpty()) ? frameNamespaces->get(m_groupName) : 0;
}

const HashSet<Page*>* Page::frameNamespace(const String& groupName)
{
    return (frameNamespaces && !groupName.isEmpty()) ? frameNamespaces->get(groupName) : 0;
}

void Page::setNeedsReapplyStyles()
{
    if (!allPages)
        return;
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frame->setNeedsReapplyStyles();
}

static Frame* incrementFrame(Frame* curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree()->traverseNextWithWrap(wrapFlag)
        : curr->tree()->traversePreviousWithWrap(wrapFlag);
}

bool Page::findString(const String& target, TextCaseSensitivity caseSensitivity, FindDirection direction, bool shouldWrap)
{
    if (target.isEmpty() || !mainFrame())
        return false;

    Frame* frame = focusController()->focusedOrMainFrame();
    Frame* startFrame = frame;
    do {
        if (frame->findString(target, direction == FindDirectionForward, caseSensitivity == TextCaseSensitive, false, true)) {
            if (frame != startFrame)
                startFrame->selectionController()->clear();
            focusController()->setFocusedFrame(frame);
            return true;
        }
        frame = incrementFrame(frame, direction == FindDirectionForward, shouldWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (shouldWrap && !startFrame->selectionController()->isNone()) {
        bool found = startFrame->findString(target, direction == FindDirectionForward, caseSensitivity == TextCaseSensitive, true, true);
        focusController()->setFocusedFrame(frame);
        return found;
    }

    return false;
}

unsigned int Page::markAllMatchesForText(const String& target, TextCaseSensitivity caseSensitivity, bool shouldHighlight, unsigned limit)
{
    if (target.isEmpty() || !mainFrame())
        return 0;

    unsigned matches = 0;

    Frame* frame = mainFrame();
    do {
        frame->setMarkedTextMatchesAreHighlighted(shouldHighlight);
        matches += frame->markAllMatchesForText(target, caseSensitivity == TextCaseSensitive, (limit == 0) ? 0 : (limit - matches));
        frame = incrementFrame(frame, true, false);
    } while (frame);

    return matches;
}

void Page::unmarkAllTextMatches()
{
    if (!mainFrame())
        return;

    Frame* frame = mainFrame();
    do {
        if (Document* document = frame->document())
            document->removeMarkers(DocumentMarker::TextMatch);
        frame = incrementFrame(frame, true, false);
    } while (frame);
}

const Selection& Page::selection() const
{
    return focusController()->focusedOrMainFrame()->selectionController()->selection();
}

void Page::setDefersLoading(bool defers)
{
    if (defers == m_defersLoading)
        return;

    m_defersLoading = defers;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->loader()->setDefersLoading(defers);
}

void Page::clearUndoRedoOperations()
{
    m_editorClient->clearUndoRedoOperations();
}

bool Page::inLowQualityImageInterpolationMode() const
{
    return m_inLowQualityInterpolationMode;
}

void Page::setInLowQualityImageInterpolationMode(bool mode)
{
    m_inLowQualityInterpolationMode = mode;
}

void Page::userStyleSheetLocationChanged()
{
#if !FRAME_LOADS_USER_STYLESHEET
    // FIXME: We should provide a way to load other types of URLs than just
    // file: (e.g., http:, data:).
    if (m_settings->userStyleSheetLocation().isLocalFile())
        m_userStyleSheetPath = m_settings->userStyleSheetLocation().fileSystemPath();
    else
        m_userStyleSheetPath = String();

    m_didLoadUserStyleSheet = false;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = 0;
#endif
}

const String& Page::userStyleSheet() const
{
    if (m_userStyleSheetPath.isEmpty()) {
        ASSERT(m_userStyleSheet.isEmpty());
        return m_userStyleSheet;
    }

    time_t modTime;
    if (!getFileModificationTime(m_userStyleSheetPath, modTime)) {
        // The stylesheet either doesn't exist, was just deleted, or is
        // otherwise unreadable. If we've read the stylesheet before, we should
        // throw away that data now as it no longer represents what's on disk.
        m_userStyleSheet = String();
        return m_userStyleSheet;
    }

    // If the stylesheet hasn't changed since the last time we read it, we can
    // just return the old data.
    if (m_didLoadUserStyleSheet && modTime <= m_userStyleSheetModificationTime)
        return m_userStyleSheet;

    m_didLoadUserStyleSheet = true;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = modTime;

    // FIXME: It would be better to load this asynchronously to avoid blocking
    // the process, but we will first need to create an asynchronous loading
    // mechanism that is not tied to a particular Frame. We will also have to
    // determine what our behavior should be before the stylesheet is loaded
    // and what should happen when it finishes loading, especially with respect
    // to when the load event fires, when Document::close is called, and when
    // layout/paint are allowed to happen.
    RefPtr<SharedBuffer> data = SharedBuffer::createWithContentsOfFile(m_userStyleSheetPath);
    if (!data)
        return m_userStyleSheet;

    m_userStyleSheet = TextResourceDecoder("text/css").decode(data->data(), data->size());

    return m_userStyleSheet;
}

} // namespace WebCore
