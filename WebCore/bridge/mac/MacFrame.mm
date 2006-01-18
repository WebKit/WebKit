/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "MacFrame.h"

#import "Cache.h"
#import "DOMInternal.h"
#import "EventNames.h"
#import "FramePrivate.h"
#import "FrameView.h"
#import "HTMLFormElementImpl.h"
#import "HTMLGenericFormElementImpl.h"
#import "InlineTextBox.h"
#import "KWQAccObjectCache.h"
#import "KWQClipboard.h"
#import "KWQEditCommand.h"
#import "KWQExceptions.h"
#import "KWQFormData.h"
#import "KWQFoundationExtras.h"
#import "KWQKJobClasses.h"
#import "KWQLogging.h"
#import "KWQPageState.h"
#import "KWQPrinter.h"
#import "KWQRegExp.h"
#import "KWQScrollBar.h"
#import "KWQTextCodec.h"
#import "KWQWindowWidget.h"
#import "SelectionController.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreGraphicsBridge.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreViewFactory.h"
#import "WebDashboardRegion.h"
#import "css_computedstyle.h"
#import "csshelper.h"
#import "dom2_eventsimpl.h"
#import "dom2_rangeimpl.h"
#import "dom_position.h"
#import "dom_textimpl.h"
#import "html_documentimpl.h"
#import "html_tableimpl.h"
#import "kjs_binding.h"
#import "kjs_window.h"
#import "render_canvas.h"
#import "render_frames.h"
#import "render_image.h"
#import "render_list.h"
#import "render_style.h"
#import "render_table.h"
#import "render_theme.h"
#import "visible_position.h"
#import "visible_text.h"
#import "visible_units.h"
#import <JavaScriptCore/NP_jsobject.h>
#import <JavaScriptCore/WebScriptObjectPrivate.h>
#import <JavaScriptCore/identifier.h>
#import <JavaScriptCore/interpreter.h>
#import <JavaScriptCore/npruntime_impl.h>
#import <JavaScriptCore/property_map.h>
#import <JavaScriptCore/runtime.h>
#import <JavaScriptCore/runtime_root.h>

#undef _KWQ_TIMING

using namespace WebCore;
using namespace EventNames;
using namespace HTMLNames;

using namespace KJS;
using namespace Bindings;

using namespace KIO;

using namespace KParts;

NSEvent *MacFrame::_currentEvent = nil;

void Frame::completed()
{
    Mac(this)->_completed.call();
}

void Frame::completed(bool arg)
{
    Mac(this)->_completed.call(arg);
}

void Frame::setStatusBarText(const QString &status)
{
    Mac(this)->setStatusBarText(status);
}

void Frame::started(Job *j)
{
    Mac(this)->_started.call(j);
}

bool KHTMLView::isKHTMLView() const
{
    return true;
}

static void redirectionTimerMonitor(void *context)
{
    MacFrame *kwq = static_cast<MacFrame *>(context);
    kwq->redirectionTimerStartedOrStopped();
}

MacFrame::MacFrame()
    : _started(this, SIGNAL(started(KIO::Job *)))
    , _completed(this, SIGNAL(completed()))
    , _completedWithBool(this, SIGNAL(completed(bool)))
    , _mouseDownView(nil)
    , _sendingEventToSubview(false)
    , _mouseDownMayStartDrag(false)
    , _mouseDownMayStartSelect(false)
    , _activationEventNumber(0)
    , _formValuesAboutToBeSubmitted(nil)
    , _formAboutToBeSubmitted(nil)
    , _windowWidget(NULL)
    , _bindingRoot(0)
    , _windowScriptObject(0)
    , _windowScriptNPObject(0)
    , _dragClipboard(0)
{
    // Must init the cache before connecting to any signals
    Cache::init();

    // The widget is made outside this class in our case.
    Frame::init(0);

    mutableInstances().prepend(this);
    d->m_redirectionTimer.setMonitor(redirectionTimerMonitor, this);
}

MacFrame::~MacFrame()
{
    setView(0);
    mutableInstances().remove(this);
    freeClipboard();
    clearRecordedFormValues();    
    
    delete _windowWidget;
}

void MacFrame::freeClipboard()
{
    if (_dragClipboard) {
        _dragClipboard->setAccessPolicy(KWQClipboard::Numb);
        _dragClipboard->deref();
        _dragClipboard = 0;
    }
}

QString MacFrame::generateFrameName()
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([_bridge generateFrameName]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

bool MacFrame::openURL(const KURL &url)
{
    KWQ_BLOCK_EXCEPTIONS;

    // FIXME: The lack of args here to get the reload flag from
    // indicates a problem in how we use Frame::processObjectRequest,
    // where we are opening the URL before the args are set up.
    [_bridge loadURL:url.getNSURL()
            referrer:[_bridge referrer]
              reload:NO
         userGesture:userGestureHint()
              target:nil
     triggeringEvent:nil
                form:nil
          formValues:nil];

    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
}

void MacFrame::openURLRequest(const KURL &url, const URLArgs &args)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSString *referrer;
    QString argsReferrer = args.metaData()["referrer"];
    if (!argsReferrer.isEmpty()) {
        referrer = argsReferrer.getNSString();
    } else {
        referrer = [_bridge referrer];
    }

    [_bridge loadURL:url.getNSURL()
            referrer:referrer
              reload:args.reload
         userGesture:userGestureHint()
              target:args.frameName.getNSString()
     triggeringEvent:nil
                form:nil
          formValues:nil];

    KWQ_UNBLOCK_EXCEPTIONS;
}


// Either get cached regexp or build one that matches any of the labels.
// The regexp we build is of the form:  (STR1|STR2|STRN)
QRegExp *regExpForLabels(NSArray *labels)
{
    // All the ObjC calls in this method are simple array and string
    // calls which we can assume do not raise exceptions


    // Parallel arrays that we use to cache regExps.  In practice the number of expressions
    // that the app will use is equal to the number of locales is used in searching.
    static const unsigned int regExpCacheSize = 4;
    static NSMutableArray *regExpLabels = nil;
    static QPtrList <QRegExp> regExps;
    static QRegExp wordRegExp = QRegExp("\\w");

    QRegExp *result;
    if (!regExpLabels) {
        regExpLabels = [[NSMutableArray alloc] initWithCapacity:regExpCacheSize];
    }
    unsigned int cacheHit = [regExpLabels indexOfObject:labels];
    if (cacheHit != NSNotFound) {
        result = regExps.at(cacheHit);
    } else {
        QString pattern("(");
        unsigned int numLabels = [labels count];
        unsigned int i;
        for (i = 0; i < numLabels; i++) {
            QString label = QString::fromNSString((NSString *)[labels objectAtIndex:i]);

            bool startsWithWordChar = false;
            bool endsWithWordChar = false;
            if (label.length() != 0) {
                startsWithWordChar = wordRegExp.search(label.at(0)) >= 0;
                endsWithWordChar = wordRegExp.search(label.at(label.length() - 1)) >= 0;
            }
            
            if (i != 0) {
                pattern.append("|");
            }
            // Search for word boundaries only if label starts/ends with "word characters".
            // If we always searched for word boundaries, this wouldn't work for languages
            // such as Japanese.
            if (startsWithWordChar) {
                pattern.append("\\b");
            }
            pattern.append(label);
            if (endsWithWordChar) {
                pattern.append("\\b");
            }
        }
        pattern.append(")");
        result = new QRegExp(pattern, false);
    }

    // add regexp to the cache, making sure it is at the front for LRU ordering
    if (cacheHit != 0) {
        if (cacheHit != NSNotFound) {
            // remove from old spot
            [regExpLabels removeObjectAtIndex:cacheHit];
            regExps.remove(cacheHit);
        }
        // add to start
        [regExpLabels insertObject:labels atIndex:0];
        regExps.insert(0, result);
        // trim if too big
        if ([regExpLabels count] > regExpCacheSize) {
            [regExpLabels removeObjectAtIndex:regExpCacheSize];
            QRegExp *last = regExps.last();
            regExps.removeLast();
            delete last;
        }
    }
    return result;
}

NSString *MacFrame::searchForLabelsAboveCell(QRegExp *regExp, HTMLTableCellElementImpl *cell)
{
    RenderTableCell *cellRenderer = static_cast<RenderTableCell *>(cell->renderer());

    if (cellRenderer) {
        RenderTableCell *cellAboveRenderer = cellRenderer->table()->cellAbove(cellRenderer);

        if (cellAboveRenderer) {
            HTMLTableCellElementImpl *aboveCell =
                static_cast<HTMLTableCellElementImpl *>(cellAboveRenderer->element());

            if (aboveCell) {
                // search within the above cell we found for a match
                for (NodeImpl *n = aboveCell->firstChild(); n; n = n->traverseNextNode(aboveCell)) {
                    if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE)
                    {
                        // For each text chunk, run the regexp
                        QString nodeString = n->nodeValue().qstring();
                        int pos = regExp->searchRev(nodeString);
                        if (pos >= 0) {
                            return nodeString.mid(pos, regExp->matchedLength()).getNSString();
                        }
                    }
                }
            }
        }
    }
    // Any reason in practice to search all cells in that are above cell?
    return nil;
}

NSString *MacFrame::searchForLabelsBeforeElement(NSArray *labels, ElementImpl *element)
{
    QRegExp *regExp = regExpForLabels(labels);
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElementImpl *startingTableCell = 0;
    bool searchedCellAbove = false;

    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    NodeImpl *n;
    for (n = element->traversePreviousNode();
         n && lengthSearched < charsSearchedThreshold;
         n = n->traversePreviousNode())
    {
        if (n->hasTagName(formTag)
            || (n->isHTMLElement()
                && static_cast<HTMLElementImpl *>(n)->isGenericFormElement()))
        {
            // We hit another form element or the start of the form - bail out
            break;
        } else if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = static_cast<HTMLTableCellElementImpl *>(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            NSString *result = searchForLabelsAboveCell(regExp, startingTableCell);
            if (result) {
                return result;
            }
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE)
        {
            // For each text chunk, run the regexp
            QString nodeString = n->nodeValue().qstring();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched) {
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            }
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0) {
                return nodeString.mid(pos, regExp->matchedLength()).getNSString();
            } else {
                lengthSearched += nodeString.length();
            }
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
         return searchForLabelsAboveCell(regExp, startingTableCell);
    } else {
        return nil;
    }
}

NSString *MacFrame::matchLabelsAgainstElement(NSArray *labels, ElementImpl *element)
{
    QString name = element->getAttribute(nameAttr).qstring();
    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    name.replace(QRegExp("[[:digit:]]"), " ");
    name.replace('_', ' ');
    
    QRegExp *regExp = regExpForLabels(labels);
    // Use the largest match we can find in the whole name string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->search(name, start);
        if (pos != -1) {
            length = regExp->matchedLength();
            if (length >= bestLength) {
                bestPos = pos;
                bestLength = length;
            }
            start = pos+1;
        }
    } while (pos != -1);

    if (bestPos != -1) {
        return name.mid(bestPos, bestLength).getNSString();
    }
    return nil;
}

// Searches from the beginning of the document if nothing is selected.
bool MacFrame::findString(NSString *string, bool forward, bool caseFlag, bool wrapFlag)
{
    QString target = QString::fromNSString(string);
    if (target.isEmpty()) {
        return false;
    }
    
    // Initially search from the start (if forward) or end (if backward) of the selection, and search to edge of document.
    RefPtr<RangeImpl> searchRange(rangeOfContents(xmlDocImpl()));
    if (selection().start().node()) {
        if (forward) {
            setStart(searchRange.get(), VisiblePosition(selection().start(), selection().affinity()));
        } else {
            setEnd(searchRange.get(), VisiblePosition(selection().end(), selection().affinity()));
        }
    }
    RefPtr<RangeImpl> resultRange(findPlainText(searchRange.get(), target, forward, caseFlag));
    
    // If we re-found the (non-empty) selected range, then search again starting just past the selected range.
    if (selection().start().node() && *resultRange == *selection().toRange()) {
        searchRange = rangeOfContents(xmlDocImpl());
        if (forward) {
            setStart(searchRange.get(), VisiblePosition(selection().end(), selection().affinity()));
        } else {
            setEnd(searchRange.get(), VisiblePosition(selection().start(), selection().affinity()));
        }
        resultRange = findPlainText(searchRange.get(), target, forward, caseFlag);
    }
    
    int exception = 0;
    
    // if we didn't find anything and we're wrapping, search again in the entire document (this will
    // redundantly re-search the area already searched in some cases).
    if (resultRange->collapsed(exception) && wrapFlag) {
        searchRange = rangeOfContents(xmlDocImpl());
        resultRange = findPlainText(searchRange.get(), target, forward, caseFlag);
        // We used to return false here if we ended up with the same range that we started with
        // (e.g., the selection was already the only instance of this text). But we decided that
        // this should be a success case instead, so we'll just fall through in that case.
    }

    if (resultRange->collapsed(exception)) {
        return false;
    }

    setSelection(SelectionController(resultRange.get(), DOWNSTREAM));
    revealSelection();
    return true;
}

void MacFrame::clearRecordedFormValues()
{
    // It's safe to assume that our own classes and Foundation data
    // structures won't raise exceptions in dealloc

    KWQRelease(_formValuesAboutToBeSubmitted);
    _formValuesAboutToBeSubmitted = nil;
    KWQRelease(_formAboutToBeSubmitted);
    _formAboutToBeSubmitted = nil;
}

void MacFrame::recordFormValue(const QString &name, const QString &value, HTMLFormElementImpl *element)
{
    // It's safe to assume that our own classes and basic Foundation
    // data structures won't raise exceptions

    if (!_formValuesAboutToBeSubmitted) {
        _formValuesAboutToBeSubmitted = KWQRetainNSRelease([[NSMutableDictionary alloc] init]);
        ASSERT(!_formAboutToBeSubmitted);
        _formAboutToBeSubmitted = KWQRetain([DOMElement _elementWithImpl:element]);
    } else {
        ASSERT([_formAboutToBeSubmitted _elementImpl] == element);
    }
    [_formValuesAboutToBeSubmitted setObject:value.getNSString() forKey:name.getNSString()];
}

void MacFrame::submitForm(const KURL &url, const URLArgs &args)
{
    KWQ_BLOCK_EXCEPTIONS;

    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.
    // We do not want to submit more than one form from the same page,
    // nor do we want to submit a single form more than once.
    // This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.
    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset _submittedFormURL on each mouse or key down event.
    WebCoreFrameBridge *target = args.frameName.isEmpty() ? _bridge : [_bridge findFrameNamed:args.frameName.getNSString()];
    Frame *targetPart = [target part];
    bool willReplaceThisFrame = false;
    for (Frame *p = this; p; p = p->parentFrame()) {
        if (p == targetPart) {
            willReplaceThisFrame = true;
            break;
        }
    }
    if (willReplaceThisFrame) {
        if (_submittedFormURL == url) {
            return;
        }
        _submittedFormURL = url;
    }

    if (!args.doPost()) {
        [_bridge loadURL:url.getNSURL()
                referrer:[_bridge referrer] 
                  reload:args.reload
             userGesture:true
                  target:args.frameName.getNSString()
         triggeringEvent:_currentEvent
                    form:_formAboutToBeSubmitted
              formValues:_formValuesAboutToBeSubmitted];
    } else {
        ASSERT(args.contentType().startsWith("Content-Type: "));
        [_bridge postWithURL:url.getNSURL()
                    referrer:[_bridge referrer] 
                      target:args.frameName.getNSString()
                        data:arrayFromFormData(args.postData)
                 contentType:args.contentType().mid(14).getNSString()
             triggeringEvent:_currentEvent
                        form:_formAboutToBeSubmitted
                  formValues:_formValuesAboutToBeSubmitted];
    }
    clearRecordedFormValues();

    KWQ_UNBLOCK_EXCEPTIONS;
}

void Frame::frameDetached()
{
    // FIXME: This should be a virtual function, with the first part in MacFrame, and the second
    // part in Frame, so it works for KHTML too.

    KWQ_BLOCK_EXCEPTIONS;
    [Mac(this)->bridge() frameDetached];
    KWQ_UNBLOCK_EXCEPTIONS;

    Frame *parent = parentFrame();
    if (parent) {
        FrameList& parentFrames = parent->d->m_frames;
        FrameIt end = parentFrames.end();
        for (FrameIt it = parentFrames.begin(); it != end; ++it) {
            ChildFrame &child = *it;
            if (child.m_frame == this) {
                parent->disconnectChild(&child);
                parentFrames.remove(it);
                deref();
                break;
            }
        }
    }
}

void MacFrame::urlSelected(const KURL &url, int button, int state, const URLArgs &args)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSString *referrer;
    QString argsReferrer = args.metaData()["referrer"];
    if (!argsReferrer.isEmpty()) {
        referrer = argsReferrer.getNSString();
    } else {
        referrer = [_bridge referrer];
    }

    [_bridge loadURL:url.getNSURL()
            referrer:referrer
              reload:args.reload
         userGesture:true
              target:args.frameName.getNSString()
     triggeringEvent:_currentEvent
                form:nil
          formValues:nil];

    KWQ_UNBLOCK_EXCEPTIONS;
}

class KWQPluginPart : public ObjectContents
{
    virtual bool openURL(const KURL &) { return true; }
    virtual bool closeURL() { return true; }
};

ObjectContents *MacFrame::createPart(const ChildFrame &child, const KURL &url, const QString &mimeType)
{
    ObjectContents *part;
    KWQ_BLOCK_EXCEPTIONS;

    ObjectElementType objectType = ObjectElementFrame;
    if (child.m_type == ChildFrame::Object)
        objectType = [_bridge determineObjectFromMIMEType:mimeType.getNSString() URL:url.getNSURL()];
    
    if (objectType == ObjectElementNone) {
        if (child.m_hasFallbackContent)
            return NULL;
        objectType = ObjectElementPlugin; // Since no fallback content exists, we'll make a plugin and show the error dialog.
    }

    if (objectType == ObjectElementPlugin) {
        KWQPluginPart *newPart = new KWQPluginPart;
        newPart->setWidget(new QWidget([_bridge viewForPluginWithURL:url.getNSURL()
                                                      attributeNames:child.m_paramNames.getNSArray()
                                                     attributeValues:child.m_paramValues.getNSArray()
                                                             MIMEType:child.m_args.serviceType.getNSString()]));
        part = newPart;
    } else {
        LOG(Frames, "name %s", child.m_name.ascii());
        BOOL allowsScrolling = YES;
        int marginWidth = -1;
        int marginHeight = -1;
        if (child.m_type != ChildFrame::Object) {
            HTMLFrameElementImpl *o = static_cast<HTMLFrameElementImpl *>(child.m_renderer->element());
            allowsScrolling = o->scrollingMode() != QScrollView::AlwaysOff;
            marginWidth = o->getMarginWidth();
            marginHeight = o->getMarginHeight();
        }
        WebCoreFrameBridge *childBridge = [_bridge createChildFrameNamed:child.m_name.getNSString()
                                                            withURL:url.getNSURL()
                                                           referrer:child.m_args.metaData()["referrer"].getNSString()
                                                         renderPart:child.m_renderer
                                                    allowsScrolling:allowsScrolling
                                                        marginWidth:marginWidth
                                                       marginHeight:marginHeight];
        // This call needs to return an object with a ref, since the caller will expect to own it.
        // childBridge owns the only ref so far.
        part = [childBridge part];
        if (part)
            part->ref();
    }

    return part;

    KWQ_UNBLOCK_EXCEPTIONS;

    return NULL;
}

void MacFrame::setView(KHTMLView *view)
{
    // Detach the document now, so any onUnload handlers get run - if
    // we wait until the view is destroyed, then things won't be
    // hooked up enough for some JavaScript calls to work.
    if (d->m_doc && view == 0)
        d->m_doc->detach();
    
    if (view)
        view->ref();
    if (d->m_view)
        d->m_view->deref();
    d->m_view = view;
    setWidget(view);
    
    // Delete old PlugIn data structures
    cleanupPluginRootObjects();
    _bindingRoot = 0;
    KWQRelease(_windowScriptObject);
    _windowScriptObject = 0;

    if (_windowScriptNPObject) {
        // Call _NPN_DeallocateObject() instead of _NPN_ReleaseObject() so that we don't leak if a plugin fails to release the window
        // script object properly.
        // This shouldn't cause any problems for plugins since they should have already been stopped and destroyed at this point.
        _NPN_DeallocateObject(_windowScriptNPObject);
        _windowScriptNPObject = 0;
    }

    // Only one form submission is allowed per view of a part.
    // Since this part may be getting reused as a result of being
    // pulled from the back/forward cache, reset this flag.
    _submittedFormURL = KURL();
}

void MacFrame::setTitle(const DOMString &title)
{
    QString text = title.qstring();
    text.replace(QChar('\\'), backslashAsCurrencySymbol());

    KWQ_BLOCK_EXCEPTIONS;
    [_bridge setTitle:text.getNSString()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void MacFrame::setStatusBarText(const QString &status)
{
    QString text = status;
    text.replace(QChar('\\'), backslashAsCurrencySymbol());

    KWQ_BLOCK_EXCEPTIONS;
    [_bridge setStatusText:text.getNSString()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void MacFrame::scheduleClose()
{
    KWQ_BLOCK_EXCEPTIONS;
    [_bridge closeWindowSoon];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void MacFrame::unfocusWindow()
{
    KWQ_BLOCK_EXCEPTIONS;
    [_bridge unfocusWindow];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QString MacFrame::advanceToNextMisspelling(bool startBeforeSelection)
{
    int exception = 0;

    // The basic approach is to search in two phases - from the selection end to the end of the doc, and
    // then we wrap and search from the doc start to (approximately) where we started.
    
    // Start at the end of the selection, search to edge of document.  Starting at the selection end makes
    // repeated "check spelling" commands work.
    RefPtr<RangeImpl> searchRange(rangeOfContents(xmlDocImpl()));
    bool startedWithSelection = false;
    if (selection().start().node()) {
        startedWithSelection = true;
        if (startBeforeSelection) {
            VisiblePosition start(selection().start(), selection().affinity());
            // We match AppKit's rule: Start 1 character before the selection.
            VisiblePosition oneBeforeStart = start.previous();
            setStart(searchRange.get(), oneBeforeStart.isNotNull() ? oneBeforeStart : start);
        } else {
            setStart(searchRange.get(), VisiblePosition(selection().end(), selection().affinity()));
        }
    }

    // If we're not in an editable node, try to find one, make that our range to work in
    NodeImpl *editableNodeImpl = searchRange->startContainer(exception);
    if (!editableNodeImpl->isContentEditable()) {
        editableNodeImpl = editableNodeImpl->nextEditable();
        if (!editableNodeImpl) {
            return QString();
        }
        searchRange->setStartBefore(editableNodeImpl, exception);
        startedWithSelection = false;   // won't need to wrap
    }
    
    // topNode defines the whole range we want to operate on 
    NodeImpl *topNode = editableNodeImpl->rootEditableElement();
    searchRange->setEndAfter(topNode, exception);

    // Make sure start of searchRange is not in the middle of a word.  Jumping back a char and then
    // forward by a word happens to do the trick.
    if (startedWithSelection) {
        VisiblePosition oneBeforeStart = startVisiblePosition(searchRange.get(), DOWNSTREAM).previous();
        if (oneBeforeStart.isNotNull()) {
            setStart(searchRange.get(), endOfWord(oneBeforeStart));
        } // else we were already at the start of the editable node
    }
    
    if (searchRange->collapsed(exception)) {
        return QString();       // nothing to search in
    }
    
    // Get the spell checker if it is available
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (checker == nil)
        return QString();
        
    WordAwareIterator it(searchRange.get());
    bool wrapped = false;
    
    // We go to the end of our first range instead of the start of it, just to be sure
    // we don't get foiled by any word boundary problems at the start.  It means we might
    // do a tiny bit more searching.
    NodeImpl *searchEndAfterWrapNode = it.range()->endContainer(exception);
    int searchEndAfterWrapOffset = it.range()->endOffset(exception);

    while (1) {
        if (!it.atEnd()) {      // we may be starting at the end of the doc, and already by atEnd
            const QChar *chars = it.characters();
            int len = it.length();
            if (len > 1 || !chars[0].isSpace()) {
                NSString *chunk = [[NSString alloc] initWithCharactersNoCopy:(unichar *)chars length:len freeWhenDone:NO];
                NSRange misspelling = [checker checkSpellingOfString:chunk startingAt:0 language:nil wrap:NO inSpellDocumentWithTag:[_bridge spellCheckerDocumentTag] wordCount:NULL];
                [chunk release];
                if (misspelling.length > 0) {
                    // Build up result range and string.  Note the misspelling may span many text nodes,
                    // but the CharIterator insulates us from this complexity
                    RefPtr<RangeImpl> misspellingRange(rangeOfContents(xmlDocImpl()));
                    CharacterIterator chars(it.range().get());
                    chars.advance(misspelling.location);
                    misspellingRange->setStart(chars.range()->startContainer(exception), chars.range()->startOffset(exception), exception);
                    QString result = chars.string(misspelling.length);
                    misspellingRange->setEnd(chars.range()->startContainer(exception), chars.range()->startOffset(exception), exception);

                    setSelection(SelectionController(misspellingRange.get(), DOWNSTREAM));
                    revealSelection();
                    // Mark misspelling in document.
                    xmlDocImpl()->addMarker(misspellingRange.get(), DocumentMarker::Spelling);
                    return result;
                }
            }
        
            it.advance();
        }
        if (it.atEnd()) {
            if (wrapped || !startedWithSelection) {
                break;      // finished the second range, or we did the whole doc with the first range
            } else {
                // we've gone from the selection to the end of doc, now wrap around
                wrapped = YES;
                searchRange->setStartBefore(topNode, exception);
                // going until the end of the very first chunk we tested is far enough
                searchRange->setEnd(searchEndAfterWrapNode, searchEndAfterWrapOffset, exception);
                it = WordAwareIterator(searchRange.get());
            }
        }   
    }
    
    return QString();
}

bool MacFrame::wheelEvent(NSEvent *event)
{
    KHTMLView *v = d->m_view;

    if (v) {
        NSEvent *oldCurrentEvent = _currentEvent;
        _currentEvent = KWQRetain(event);

        QWheelEvent qEvent(event);
        v->viewportWheelEvent(&qEvent);

        ASSERT(_currentEvent == event);
        KWQRelease(event);
        _currentEvent = oldCurrentEvent;

        if (qEvent.isAccepted())
            return true;
    }

    // FIXME: The scrolling done here should be done in the default handlers
    // of the elements rather than here in the part.

    KWQScrollDirection direction;
    float multiplier;
    float deltaX = [event deltaX];
    float deltaY = [event deltaY];
    if (deltaX < 0) {
        direction = KWQScrollRight;
        multiplier = -deltaX;
    } else if (deltaX > 0) {
        direction = KWQScrollLeft;
        multiplier = deltaX;
    } else if (deltaY < 0) {
        direction = KWQScrollDown;
        multiplier = -deltaY;
    }  else if (deltaY > 0) {
        direction = KWQScrollUp;
        multiplier = deltaY;
    } else {
        return false;
    }

    RenderObject *r = renderer();
    if (r == 0) {
        return false;
    }
    
    NSPoint point = [d->m_view->getDocumentView() convertPoint:[event locationInWindow] fromView:nil];
    RenderObject::NodeInfo nodeInfo(true, true);
    r->layer()->hitTest(nodeInfo, (int)point.x, (int)point.y);    
    
    NodeImpl *node = nodeInfo.innerNode();
    if (node == 0) {
        return false;
    }
    
    r = node->renderer();
    if (r == 0) {
        return false;
    }
    
    return r->scroll(direction, KWQScrollWheel, multiplier);
}

void MacFrame::redirectionTimerStartedOrStopped()
{
    // Don't report history navigations, just actual redirection.
    if (d->m_scheduledRedirection == historyNavigationScheduled) {
        return;
    }
    
    KWQ_BLOCK_EXCEPTIONS;
    if (d->m_redirectionTimer.isActive()) {
        NSDate *fireDate = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:d->m_redirectionTimer.fireDate()];
        [_bridge reportClientRedirectToURL:KURL(d->m_redirectURL).getNSURL()
                                     delay:d->m_delayRedirect
                                  fireDate:fireDate
                               lockHistory:d->m_redirectLockHistory
                               isJavaScriptFormAction:d->m_executingJavaScriptFormAction];
        [fireDate release];
    } else {
        [_bridge reportClientRedirectCancelled:d->m_cancelWithLoadInProgress];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

QString MacFrame::userAgent() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([_bridge userAgentForURL:m_url.getNSURL()]);
    KWQ_UNBLOCK_EXCEPTIONS;
         
    return QString();
}

QString MacFrame::mimeTypeForFileName(const QString &fileName) const
{
    NSString *ns = fileName.getNSString();

    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([_bridge MIMETypeForPath:ns]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

NSView *MacFrame::nextKeyViewInFrame(NodeImpl *node, KWQSelectionDirection direction)
{
    DocumentImpl *doc = xmlDocImpl();
    if (!doc) {
        return nil;
    }
    for (;;) {
        node = direction == KWQSelectingNext
            ? doc->nextFocusNode(node) : doc->previousFocusNode(node);
        if (!node) {
            return nil;
        }
        RenderObject *renderer = node->renderer();
        if (renderer->isWidget()) {
            RenderWidget *renderWidget = static_cast<RenderWidget *>(renderer);
            QWidget *widget = renderWidget->widget();
            KHTMLView *childFrameWidget = widget->isKHTMLView() ? static_cast<KHTMLView *>(widget) : 0;
            NSView *view = nil;
            if (childFrameWidget) {
                view = Mac(childFrameWidget->frame())->nextKeyViewInFrame(0, direction);
            } else if (widget) {
                view = widget->getView();
            }
            if (view) {
                return view;
            }
        } else {
            static_cast<ElementImpl *>(node)->focus(); 
        } 
        [_bridge makeFirstResponder:[_bridge documentView]];
        return [_bridge documentView];
    }
}

NSView *MacFrame::nextKeyViewInFrameHierarchy(NodeImpl *node, KWQSelectionDirection direction)
{
    NSView *next = nextKeyViewInFrame(node, direction);
    if (!next) {
        MacFrame *parent = Mac(parentFrame());
        if (parent) {
            next = parent->nextKeyViewInFrameHierarchy(parent->childFrame(this)->m_renderer->element(), direction);
        }
    }
    
    // remove focus from currently focused node if we're giving focus to another view
    if (next && (next != [_bridge documentView])) {
        DocumentImpl *doc = xmlDocImpl();
        if (doc) {
            doc->setFocusNode(0);
        }            
    }    
    
    return next;
}

NSView *MacFrame::nextKeyView(NodeImpl *node, KWQSelectionDirection direction)
{
    NSView * next;
    KWQ_BLOCK_EXCEPTIONS;

    next = nextKeyViewInFrameHierarchy(node, direction);
    if (next) {
        return next;
    }

    // Look at views from the top level part up, looking for a next key view that we can use.

    next = direction == KWQSelectingNext
        ? [_bridge nextKeyViewOutsideWebFrameViews]
        : [_bridge previousKeyViewOutsideWebFrameViews];

    if (next) {
        return next;
    }

    KWQ_UNBLOCK_EXCEPTIONS;
    
    // If all else fails, make a loop by starting from 0.
    return nextKeyViewInFrameHierarchy(0, direction);
}

NSView *MacFrame::nextKeyViewForWidget(QWidget *startingWidget, KWQSelectionDirection direction)
{
    // Use the event filter object to figure out which RenderWidget owns this QWidget and get to the DOM.
    // Then get the next key view in the order determined by the DOM.
    NodeImpl *node = nodeForWidget(startingWidget);
    ASSERT(node);
    return Mac(frameForNode(node))->nextKeyView(node, direction);
}

bool MacFrame::currentEventIsMouseDownInWidget(QWidget *candidate)
{
    KWQ_BLOCK_EXCEPTIONS;
    switch ([[NSApp currentEvent] type]) {
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
            break;
        default:
            return NO;
    }
    KWQ_UNBLOCK_EXCEPTIONS;
    
    NodeImpl *node = nodeForWidget(candidate);
    ASSERT(node);
    return frameForNode(node)->d->m_view->nodeUnderMouse() == node;
}

bool MacFrame::currentEventIsKeyboardOptionTab()
{
    KWQ_BLOCK_EXCEPTIONS;
    NSEvent *evt = [NSApp currentEvent];
    if ([evt type] != NSKeyDown) {
        return NO;
    }

    if (([evt modifierFlags] & NSAlternateKeyMask) == 0) {
        return NO;
    }
    
    NSString *chars = [evt charactersIgnoringModifiers];
    if ([chars length] != 1)
        return NO;
    
    const unichar tabKey = 0x0009;
    const unichar shiftTabKey = 0x0019;
    unichar c = [chars characterAtIndex:0];
    if (c != tabKey && c != shiftTabKey)
        return NO;
    
    KWQ_UNBLOCK_EXCEPTIONS;
    return YES;
}

bool MacFrame::handleKeyboardOptionTabInView(NSView *view)
{
    if (MacFrame::currentEventIsKeyboardOptionTab()) {
        if (([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask) != 0) {
            [[view window] selectKeyViewPrecedingView:view];
        } else {
            [[view window] selectKeyViewFollowingView:view];
        }
        return YES;
    }
    
    return NO;
}

bool MacFrame::tabsToLinks() const
{
    if ([_bridge keyboardUIMode] & WebCoreKeyboardAccessTabsToLinks)
        return !MacFrame::currentEventIsKeyboardOptionTab();
    else
        return MacFrame::currentEventIsKeyboardOptionTab();
}

bool MacFrame::tabsToAllControls() const
{
    WebCoreKeyboardUIMode keyboardUIMode = [_bridge keyboardUIMode];
    BOOL handlingOptionTab = MacFrame::currentEventIsKeyboardOptionTab();

    // If tab-to-links is off, option-tab always highlights all controls
    if ((keyboardUIMode & WebCoreKeyboardAccessTabsToLinks) == 0 && handlingOptionTab) {
        return YES;
    }
    
    // If system preferences say to include all controls, we always include all controls
    if (keyboardUIMode & WebCoreKeyboardAccessFull) {
        return YES;
    }
    
    // Otherwise tab-to-links includes all controls, unless the sense is flipped via option-tab.
    if (keyboardUIMode & WebCoreKeyboardAccessTabsToLinks) {
        return !handlingOptionTab;
    }
    
    return handlingOptionTab;
}

KJS::Bindings::RootObject *MacFrame::executionContextForDOM()
{
    return bindingRootObject();
}

KJS::Bindings::RootObject *MacFrame::bindingRootObject()
{
    assert(d->m_bJScriptEnabled);
    if (!_bindingRoot) {
        JSLock lock;
        _bindingRoot = new KJS::Bindings::RootObject(0);    // The root gets deleted by JavaScriptCore.
        KJS::JSObject *win = KJS::Window::retrieveWindow(this);
        _bindingRoot->setRootObjectImp (win);
        _bindingRoot->setInterpreter(jScript()->interpreter());
        addPluginRootObject (_bindingRoot);
    }
    return _bindingRoot;
}

WebScriptObject *MacFrame::windowScriptObject()
{
    if (!d->m_bJScriptEnabled)
        return 0;

    if (!_windowScriptObject) {
        KJS::JSLock lock;
        KJS::JSObject *win = KJS::Window::retrieveWindow(this);
        _windowScriptObject = KWQRetainNSRelease([[WebScriptObject alloc] _initWithJSObject:win originExecutionContext:bindingRootObject() executionContext:bindingRootObject()]);
    }

    return _windowScriptObject;
}

NPObject *MacFrame::windowScriptNPObject()
{
    if (!d->m_bJScriptEnabled)
        return 0;

    if (!_windowScriptNPObject) {
        KJS::JSObject *win = KJS::Window::retrieveWindow(this);
        
        // The window script object can be 0 if JavaScript is disabled.  However, callers (like plugins) expect us to
        // always return a window script object here.  By substituting a plain JSObject for the window's JSObject,
        // we can satisfy callers' assumptions and let them try to manipulate the dummy object when JavaScript is
        // disabled.
        if (!win) {
            JSLock lock;
            win = new KJS::JSObject();
        }
        
        _windowScriptNPObject = _NPN_CreateScriptObject (0, win, bindingRootObject(), bindingRootObject());
    }

    return _windowScriptNPObject;
}

void MacFrame::partClearedInBegin()
{
    if (d->m_bJScriptEnabled)
        [_bridge windowObjectCleared];
}

void MacFrame::openURLFromPageCache(KWQPageState *state)
{
    // It's safe to assume none of the KWQPageState methods will raise
    // exceptions, since KWQPageState is implemented by WebCore and
    // does not throw

    DocumentImpl *doc = [state document];
    NodeImpl *mousePressNode = [state mousePressNode];
    KURL *url = [state URL];
    SavedProperties *windowProperties = [state windowProperties];
    SavedProperties *locationProperties = [state locationProperties];
    SavedBuiltins *interpreterBuiltins = [state interpreterBuiltins];
    PausedTimeouts *timeouts = [state pausedTimeouts];
    
    cancelRedirection();

    // We still have to close the previous part page.
    if (!d->m_restored)
        closeURL();
            
    d->m_bComplete = false;
    
    // Don't re-emit the load event.
    d->m_bLoadEventEmitted = true;
    
    // delete old status bar msg's from kjs (if it _was_ activated on last URL)
    if (d->m_bJScriptEnabled) {
        d->m_kjsStatusBarText = QString::null;
        d->m_kjsDefaultStatusBarText = QString::null;
    }

    ASSERT(url);
    
    m_url = *url;
    
    // initializing m_url to the new url breaks relative links when opening such a link after this call and _before_ begin() is called (when the first
    // data arrives) (Simon)
    if (m_url.protocol().startsWith("http") && !m_url.host().isEmpty() && m_url.path().isEmpty())
        m_url.setPath("/");
    
    // copy to m_workingURL after fixing m_url above
    d->m_workingURL = m_url;
        
    emit started(NULL);
    
    // -----------begin-----------
    clear();

    doc->setInPageCache(NO);

    d->m_bCleared = false;
    d->m_bComplete = false;
    d->m_bLoadEventEmitted = false;
    d->m_referrer = m_url.url();
    
    setView(doc->view());
    
    d->m_doc = doc;
    d->m_doc->ref();
    
    d->m_mousePressNode = mousePressNode;
    
    Decoder *decoder = doc->decoder();
    if (decoder)
        decoder->ref();
    if (d->m_decoder)
        d->m_decoder->deref();
    d->m_decoder = decoder;

    updatePolicyBaseURL();

    { // scope the lock
        JSLock lock;
        restoreWindowProperties(windowProperties);
        restoreLocationProperties(locationProperties);
        restoreInterpreterBuiltins(*interpreterBuiltins);
    }

    resumeTimeouts(timeouts);
    
    checkCompleted();
}

WebCoreFrameBridge *MacFrame::bridgeForWidget(const QWidget *widget)
{
    ASSERT_ARG(widget, widget);
    
    MacFrame *frame = Mac(frameForWidget(widget));
    ASSERT(frame);
    return frame->bridge();
}

NSView *MacFrame::documentViewForNode(NodeImpl *node)
{
    WebCoreFrameBridge *bridge = Mac(frameForNode(node))->bridge();
    return [bridge documentView];
}

void MacFrame::saveDocumentState()
{
    // Do not save doc state if the page has a password field and a form that would be submitted
    // via https
    if (!(d->m_doc && d->m_doc->hasPasswordField() && d->m_doc->hasSecureForm())) {
        KWQ_BLOCK_EXCEPTIONS;
        [_bridge saveDocumentState];
        KWQ_UNBLOCK_EXCEPTIONS;
    }
}

void MacFrame::restoreDocumentState()
{
    KWQ_BLOCK_EXCEPTIONS;
    [_bridge restoreDocumentState];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QString MacFrame::requestedURLString() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([_bridge requestedURLString]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

QString MacFrame::incomingReferrer() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([_bridge incomingReferrer]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

void MacFrame::runJavaScriptAlert(const DOMString& message)
{
    DOMString text = message;
    text.replace(QChar('\\'), backslashAsCurrencySymbol());
    KWQ_BLOCK_EXCEPTIONS;
    [_bridge runJavaScriptAlertPanelWithMessage:text];
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool MacFrame::runJavaScriptConfirm(const DOMString& message)
{
    DOMString text = message;
    text.replace(QChar('\\'), backslashAsCurrencySymbol());

    KWQ_BLOCK_EXCEPTIONS;
    return [_bridge runJavaScriptConfirmPanelWithMessage:text];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

bool MacFrame::runJavaScriptPrompt(const DOMString& prompt, const DOMString& defaultValue, DOMString& result)
{
    DOMString promptText = prompt;
    promptText.replace(QChar('\\'), backslashAsCurrencySymbol());
    DOMString defaultValueText = defaultValue;
    defaultValueText.replace(QChar('\\'), backslashAsCurrencySymbol());

    bool ok;
    KWQ_BLOCK_EXCEPTIONS;
    NSString *returnedText = nil;

    ok = [_bridge runJavaScriptTextInputPanelWithPrompt:prompt
        defaultText:defaultValue returningText:&returnedText];

    if (ok) {
        result = DOMString(returnedText);
        result.replace(backslashAsCurrencySymbol(), QChar('\\'));
    }

    return ok;
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return false;
}

bool MacFrame::locationbarVisible()
{
    return [_bridge areToolbarsVisible];
}

bool MacFrame::menubarVisible()
{
    // The menubar is always on in Mac OS X UI
    return true;
}

bool MacFrame::personalbarVisible()
{
    return [_bridge areToolbarsVisible];
}

bool MacFrame::statusbarVisible()
{
    return [_bridge isStatusbarVisible];
}

bool MacFrame::toolbarVisible()
{
    return [_bridge areToolbarsVisible];
}

void MacFrame::addMessageToConsole(const DOMString &message, unsigned lineNumber, const DOMString &sourceURL)
{
    NSDictionary *dictionary = [NSDictionary dictionaryWithObjectsAndKeys:
        (NSString *)message, @"message",
        [NSNumber numberWithInt: lineNumber], @"lineNumber",
        (NSString *)sourceURL, @"sourceURL",
        NULL];
    [_bridge addMessageToConsole:dictionary];
}

void MacFrame::createEmptyDocument()
{
    // Although it's not completely clear from the name of this function,
    // it does nothing if we already have a document, and just creates an
    // empty one if we have no document at all.
    if (!d->m_doc) {
        KWQ_BLOCK_EXCEPTIONS;
        [_bridge loadEmptyDocumentSynchronously];
        KWQ_UNBLOCK_EXCEPTIONS;

        if (parentFrame() && (parentFrame()->childFrame(this)->m_type == ChildFrame::IFrame ||
                parentFrame()->childFrame(this)->m_type == ChildFrame::Object)) {
            d->m_doc->setBaseURL(parentFrame()->d->m_doc->baseURL());
        }
    }
}

bool MacFrame::keyEvent(NSEvent *event)
{
    bool result;
    KWQ_BLOCK_EXCEPTIONS;

    ASSERT([event type] == NSKeyDown || [event type] == NSKeyUp);

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    DocumentImpl *doc = xmlDocImpl();
    if (!doc) {
        return false;
    }
    NodeImpl *node = doc->focusNode();
    if (!node) {
        node = doc->body();
        if (!node) {
            return false;
        }
    }
    
    if ([event type] == NSKeyDown) {
        prepareForUserAction();
    }

    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = KWQRetain(event);

    QKeyEvent qEvent(event);
    result = !node->dispatchKeyEvent(&qEvent);

    // We want to send both a down and a press for the initial key event.
    // To get KHTML to do this, we send a second KeyPress QKeyEvent with "is repeat" set to true,
    // which causes it to send a press to the DOM.
    // That's not a great hack; it would be good to do this in a better way.
    if ([event type] == NSKeyDown && ![event isARepeat]) {
        QKeyEvent repeatEvent(event, true);
        if (!node->dispatchKeyEvent(&repeatEvent)) {
            result = true;
        }
    }

    ASSERT(_currentEvent == event);
    KWQRelease(event);
    _currentEvent = oldCurrentEvent;

    return result;

    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void MacFrame::khtmlMousePressEvent(MousePressEvent *event)
{
    bool singleClick = [_currentEvent clickCount] <= 1;

    // If we got the event back, that must mean it wasn't prevented,
    // so it's allowed to start a drag or selection.
    _mouseDownMayStartSelect = canMouseDownStartSelect(event->innerNode());
    
    // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    _mouseDownMayStartDrag = singleClick;

    d->m_mousePressNode = event->innerNode();
    
    if (!passWidgetMouseDownEventToWidget(event)) {
        // We don't do this at the start of mouse down handling (before calling into WebCore),
        // because we don't want to do it until we know we didn't hit a widget.
        NSView *view = d->m_view->getDocumentView();

        if (singleClick) {
            KWQ_BLOCK_EXCEPTIONS;
            if ([_bridge firstResponder] != view) {
                [_bridge makeFirstResponder:view];
            }
            KWQ_UNBLOCK_EXCEPTIONS;
        }

        Frame::khtmlMousePressEvent(event);
    }
}

bool MacFrame::passMouseDownEventToWidget(QWidget* widget)
{
    // FIXME: this method always returns true

    if (!widget) {
        ERROR("hit a RenderWidget without a corresponding QWidget, means a frame is half-constructed");
        return true;
    }

    KWQ_BLOCK_EXCEPTIONS;
    
    NSView *nodeView = widget->getView();
    ASSERT(nodeView);
    ASSERT([nodeView superview]);
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:[_currentEvent locationInWindow] fromView:nil]];
    if (view == nil) {
        ERROR("KHTML says we hit a RenderWidget, but AppKit doesn't agree we hit the corresponding NSView");
        return true;
    }
    
    if ([_bridge firstResponder] == view) {
        // In the case where we just became first responder, we should send the mouseDown:
        // to the NSTextField, not the NSTextField's editor. This code makes sure that happens.
        // If we don't do this, we see a flash of selected text when clicking in a text field.
        if (![_bridge wasFirstResponderAtMouseDownTime:view] && [view isKindOfClass:[NSTextView class]]) {
            NSView *superview = view;
            while (superview != nodeView) {
                superview = [superview superview];
                ASSERT(superview);
                if ([superview isKindOfClass:[NSControl class]]) {
                    NSControl *control = static_cast<NSControl *>(superview);
                    if ([control currentEditor] == view) {
                        view = superview;
                    }
                    break;
                }
            }
        }
    } else {
        // Normally [NSWindow sendEvent:] handles setting the first responder.
        // But in our case, the event was sent to the view representing the entire web page.
        if ([_currentEvent clickCount] <= 1 && [view acceptsFirstResponder] && [view needsPanelToBecomeKey]) {
            [_bridge makeFirstResponder:view];
        }
    }

    // We need to "defer loading" and defer timers while we are tracking the mouse.
    // That's because we don't want the new page to load while the user is holding the mouse down.
    
    BOOL wasDeferringLoading = [_bridge defersLoading];
    if (!wasDeferringLoading) {
        [_bridge setDefersLoading:YES];
    }
    BOOL wasDeferringTimers = QObject::defersTimers();
    if (!wasDeferringTimers) {
        QObject::setDefersTimers(true);
    }
    ASSERT(!_sendingEventToSubview);
    _sendingEventToSubview = true;
    [view mouseDown:_currentEvent];
    _sendingEventToSubview = false;
    
    if (!wasDeferringTimers) {
        QObject::setDefersTimers(false);
    }
    if (!wasDeferringLoading) {
        [_bridge setDefersLoading:NO];
    }

    // Remember which view we sent the event to, so we can direct the release event properly.
    _mouseDownView = view;
    _mouseDownWasInSubframe = false;

    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
}

bool MacFrame::lastEventIsMouseUp() const
{
    // Many AK widgets run their own event loops and consume events while the mouse is down.
    // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
    // the khtml state with this mouseUp, which khtml never saw.  This method lets us detect
    // that state.

    KWQ_BLOCK_EXCEPTIONS;
    NSEvent *currentEventAfterHandlingMouseDown = [NSApp currentEvent];
    if (_currentEvent != currentEventAfterHandlingMouseDown) {
        if ([currentEventAfterHandlingMouseDown type] == NSLeftMouseUp) {
            return true;
        }
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}
    
// Note that this does the same kind of check as [target isDescendantOf:superview].
// There are two differences: This is a lot slower because it has to walk the whole
// tree, and this works in cases where the target has already been deallocated.
static bool findViewInSubviews(NSView *superview, NSView *target)
{
    KWQ_BLOCK_EXCEPTIONS;
    NSEnumerator *e = [[superview subviews] objectEnumerator];
    NSView *subview;
    while ((subview = [e nextObject])) {
        if (subview == target || findViewInSubviews(subview, target)) {
            return true;
        }
    }
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return false;
}

NSView *MacFrame::mouseDownViewIfStillGood()
{
    // Since we have no way of tracking the lifetime of _mouseDownView, we have to assume that
    // it could be deallocated already. We search for it in our subview tree; if we don't find
    // it, we set it to nil.
    NSView *mouseDownView = _mouseDownView;
    if (!mouseDownView) {
        return nil;
    }
    KHTMLView *topKHTMLView = d->m_view;
    NSView *topView = topKHTMLView ? topKHTMLView->getView() : nil;
    if (!topView || !findViewInSubviews(topView, mouseDownView)) {
        _mouseDownView = nil;
        return nil;
    }
    return mouseDownView;
}

bool MacFrame::eventMayStartDrag(NSEvent *event) const
{
    // This is a pre-flight check of whether the event might lead to a drag being started.  Be careful
    // that its logic needs to stay in sync with khtmlMouseMoveEvent() and the way we set
    // _mouseDownMayStartDrag in khtmlMousePressEvent
    
    if ([event type] != NSLeftMouseDown || [event clickCount] != 1) {
        return false;
    }
    
    BOOL DHTMLFlag, UAFlag;
    [_bridge allowDHTMLDrag:&DHTMLFlag UADrag:&UAFlag];
    if (!DHTMLFlag && !UAFlag) {
        return false;
    }

    NSPoint loc = [event locationInWindow];
    int mouseDownX, mouseDownY;
    d->m_view->viewportToContents((int)loc.x, (int)loc.y, mouseDownX, mouseDownY);
    RenderObject::NodeInfo nodeInfo(true, false);
    renderer()->layer()->hitTest(nodeInfo, mouseDownX, mouseDownY);
    bool srcIsDHTML;
    return nodeInfo.innerNode()->renderer()->draggableNode(DHTMLFlag, UAFlag, mouseDownX, mouseDownY, srcIsDHTML);
}

// The link drag hysteresis is much larger than the others because there
// needs to be enough space to cancel the link press without starting a link drag,
// and because dragging links is rare.
const float LinkDragHysteresis = 40.0;
const float ImageDragHysteresis = 5.0;
const float TextDragHysteresis = 3.0;
const float GeneralDragHysteresis = 3.0;
const float TextDragDelay = 0.15;

bool MacFrame::dragHysteresisExceeded(float dragLocationX, float dragLocationY) const
{
    int dragX, dragY;
    d->m_view->viewportToContents((int)dragLocationX, (int)dragLocationY, dragX, dragY);
    float deltaX = QABS(dragX - _mouseDownX);
    float deltaY = QABS(dragY - _mouseDownY);
    
    float threshold = GeneralDragHysteresis;
    if (_dragSrcIsImage) {
        threshold = ImageDragHysteresis;
    } else if (_dragSrcIsLink) {
        threshold = LinkDragHysteresis;
    } else if (_dragSrcInSelection) {
        threshold = TextDragHysteresis;
    }
    return deltaX >= threshold || deltaY >= threshold;
}

void MacFrame::khtmlMouseMoveEvent(MouseMoveEvent *event)
{
    KWQ_BLOCK_EXCEPTIONS;

    if ([_currentEvent type] == NSLeftMouseDragged) {
        NSView *view = mouseDownViewIfStillGood();

        if (view) {
            _sendingEventToSubview = true;
            [view mouseDragged:_currentEvent];
            _sendingEventToSubview = false;
            return;
        }

        // Careful that the drag starting logic stays in sync with eventMayStartDrag()
    
        if (_mouseDownMayStartDrag && !_dragSrc) {
            BOOL tempFlag1, tempFlag2;
            [_bridge allowDHTMLDrag:&tempFlag1 UADrag:&tempFlag2];
            _dragSrcMayBeDHTML = tempFlag1;
            _dragSrcMayBeUA = tempFlag2;
            if (!_dragSrcMayBeDHTML && !_dragSrcMayBeUA) {
                _mouseDownMayStartDrag = false;     // no element is draggable
            }
        }
        
        if (_mouseDownMayStartDrag && !_dragSrc) {
            // try to find an element that wants to be dragged
            RenderObject::NodeInfo nodeInfo(true, false);
            renderer()->layer()->hitTest(nodeInfo, _mouseDownX, _mouseDownY);
            NodeImpl *node = nodeInfo.innerNode();
            _dragSrc = (node && node->renderer()) ? node->renderer()->draggableNode(_dragSrcMayBeDHTML, _dragSrcMayBeUA, _mouseDownX, _mouseDownY, _dragSrcIsDHTML) : 0;
            if (!_dragSrc) {
                _mouseDownMayStartDrag = false;     // no element is draggable
            } else {
                // remember some facts about this source, while we have a NodeInfo handy
                node = nodeInfo.URLElement();
                _dragSrcIsLink = node && node->isLink();

                node = nodeInfo.innerNonSharedNode();
                _dragSrcIsImage = node && node->renderer() && node->renderer()->isImage();

                _dragSrcInSelection = isPointInsideSelection(_mouseDownX, _mouseDownY);
            }                
        }
        
        // For drags starting in the selection, the user must wait between the mousedown and mousedrag,
        // or else we bail on the dragging stuff and allow selection to occur
        if (_mouseDownMayStartDrag && _dragSrcInSelection && [_currentEvent timestamp] - _mouseDownTimestamp < TextDragDelay) {
            _mouseDownMayStartDrag = false;
            // ...but if this was the first click in the window, we don't even want to start selection
            if (_activationEventNumber == [_currentEvent eventNumber]) {
                _mouseDownMayStartSelect = false;
            }
        }

        if (_mouseDownMayStartDrag) {
            // We are starting a text/image/url drag, so the cursor should be an arrow
            d->m_view->viewport()->setCursor(QCursor());
            
            NSPoint dragLocation = [_currentEvent locationInWindow];
            if (dragHysteresisExceeded(dragLocation.x, dragLocation.y)) {
                
                // Once we're past the hysteresis point, we don't want to treat this gesture as a click
                d->m_view->invalidateClick();

                NSImage *dragImage = nil;       // we use these values if WC is out of the loop
                NSPoint dragLoc = NSZeroPoint;
                NSDragOperation srcOp = NSDragOperationNone;                
                BOOL wcWrotePasteboard = NO;
                if (_dragSrcMayBeDHTML) {
                    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
                    // Must be done before ondragstart adds types and data to the pboard,
                    // also done for security, as it erases data from the last drag
                    [pasteboard declareTypes:[NSArray array] owner:nil];
                    
                    freeClipboard();    // would only happen if we missed a dragEnd.  Do it anyway, just
                                        // to make sure it gets numbified
                    _dragClipboard = new KWQClipboard(true, pasteboard, KWQClipboard::Writable, this);
                    _dragClipboard->ref();
                    
                    // If this is drag of an element, get set up to generate a default image.  Otherwise
                    // WebKit will generate the default, the element doesn't override.
                    if (_dragSrcIsDHTML) {
                        int srcX, srcY;
                        _dragSrc->renderer()->absolutePosition(srcX, srcY);
                        _dragClipboard->setDragImageElement(_dragSrc.get(), IntPoint(_mouseDownX - srcX, _mouseDownY - srcY));
                    }
                    
                    _mouseDownMayStartDrag = dispatchDragSrcEvent(dragstartEvent, IntPoint(_mouseDownWinX, _mouseDownWinY));
                    // Invalidate clipboard here against anymore pasteboard writing for security.  The drag
                    // image can still be changed as we drag, but not the pasteboard data.
                    _dragClipboard->setAccessPolicy(KWQClipboard::ImageWritable);
                    
                    if (_mouseDownMayStartDrag) {
                        // gather values from DHTML element, if it set any
                        _dragClipboard->sourceOperation(&srcOp);

                        NSArray *types = [pasteboard types];
                        wcWrotePasteboard = types && [types count] > 0;

                        if (_dragSrcMayBeDHTML) {
                            dragImage = _dragClipboard->dragNSImage(&dragLoc);
                        }
                        
                        // Yuck, dragSourceMovedTo() can be called as a result of kicking off the drag with
                        // dragImage!  Because of that dumb reentrancy, we may think we've not started the
                        // drag when that happens.  So we have to assume it's started before we kick it off.
                        _dragClipboard->setDragHasStarted();
                    }
                }
                
                if (_mouseDownMayStartDrag) {
                    BOOL startedDrag = [_bridge startDraggingImage:dragImage at:dragLoc operation:srcOp event:_currentEvent sourceIsDHTML:_dragSrcIsDHTML DHTMLWroteData:wcWrotePasteboard];
                    if (!startedDrag && _dragSrcMayBeDHTML) {
                        // WebKit canned the drag at the last minute - we owe _dragSrc a DRAGEND event
                        dispatchDragSrcEvent(dragendEvent, IntPoint(dragLocation));
                        _mouseDownMayStartDrag = false;
                    }
                } 

                if (!_mouseDownMayStartDrag) {
                    // something failed to start the drag, cleanup
                    freeClipboard();
                    _dragSrc = 0;
                }
            }

            // No more default handling (like selection), whether we're past the hysteresis bounds or not
            return;
        }
        if (!_mouseDownMayStartSelect) {
            return;
        }

        // Don't allow dragging or click handling after we've started selecting.
        _mouseDownMayStartDrag = false;
        d->m_view->invalidateClick();

        // We use khtml's selection but our own autoscrolling.
        [_bridge handleAutoscrollForMouseDragged:_currentEvent];
    } else {
        // If we allowed the other side of the bridge to handle a drag
        // last time, then m_bMousePressed might still be set. So we
        // clear it now to make sure the next move after a drag
        // doesn't look like a drag.
        d->m_bMousePressed = false;
    }

    Frame::khtmlMouseMoveEvent(event);

    KWQ_UNBLOCK_EXCEPTIONS;
}

// Returns whether caller should continue with "the default processing", which is the same as 
// the event handler NOT setting the return value to false
bool MacFrame::dispatchCPPEvent(const AtomicString &eventType, KWQClipboard::AccessPolicy policy)
{
    NodeImpl *target = d->m_selection.start().element();
    if (!target && xmlDocImpl()) {
        target = xmlDocImpl()->body();
    }
    if (!target) {
        return true;
    }

    KWQClipboard *clipboard = new KWQClipboard(false, [NSPasteboard generalPasteboard], (KWQClipboard::AccessPolicy)policy);
    clipboard->ref();

    int exceptioncode = 0;
    EventImpl *evt = new ClipboardEventImpl(eventType, true, true, clipboard);
    evt->ref();
    target->dispatchEvent(evt, exceptioncode, true);
    bool noDefaultProcessing = evt->defaultPrevented();
    evt->deref();

    // invalidate clipboard here for security
    clipboard->setAccessPolicy(KWQClipboard::Numb);
    clipboard->deref();

    return !noDefaultProcessing;
}

// WinIE uses onbeforecut and onbeforepaste to enables the cut and paste menu items.  They
// also send onbeforecopy, apparently for symmetry, but it doesn't affect the menu items.
// We need to use onbeforecopy as a real menu enabler because we allow elements that are not
// normally selectable to implement copy/paste (like divs, or a document body).

bool MacFrame::mayCut()
{
    return !dispatchCPPEvent(beforecutEvent, KWQClipboard::Numb);
}

bool MacFrame::mayCopy()
{
    return !dispatchCPPEvent(beforecopyEvent, KWQClipboard::Numb);
}

bool MacFrame::mayPaste()
{
    return !dispatchCPPEvent(beforepasteEvent, KWQClipboard::Numb);
}

bool MacFrame::tryCut()
{
    // Must be done before oncut adds types and data to the pboard,
    // also done for security, as it erases data from the last copy/paste.
    [[NSPasteboard generalPasteboard] declareTypes:[NSArray array] owner:nil];

    return !dispatchCPPEvent(cutEvent, KWQClipboard::Writable);
}

bool MacFrame::tryCopy()
{
    // Must be done before oncopy adds types and data to the pboard,
    // also done for security, as it erases data from the last copy/paste.
    [[NSPasteboard generalPasteboard] declareTypes:[NSArray array] owner:nil];

    return !dispatchCPPEvent(copyEvent, KWQClipboard::Writable);
}

bool MacFrame::tryPaste()
{
    return !dispatchCPPEvent(pasteEvent, KWQClipboard::Readable);
}

void MacFrame::khtmlMouseReleaseEvent(MouseReleaseEvent *event)
{
    NSView *view = mouseDownViewIfStillGood();
    if (!view) {
        // If this was the first click in the window, we don't even want to clear the selection.
        // This case occurs when the user clicks on a draggable element, since we have to process
        // the mouse down and drag events to see if we might start a drag.  For other first clicks
        // in a window, we just don't acceptFirstMouse, and the whole down-drag-up sequence gets
        // ignored upstream of this layer.
        if (_activationEventNumber != [_currentEvent eventNumber]) {
            Frame::khtmlMouseReleaseEvent(event);
        }
        return;
    }
    
    _sendingEventToSubview = true;
    KWQ_BLOCK_EXCEPTIONS;
    [view mouseUp:_currentEvent];
    KWQ_UNBLOCK_EXCEPTIONS;
    _sendingEventToSubview = false;
}

bool MacFrame::passSubframeEventToSubframe(NodeImpl::MouseEvent &event)
{
    KWQ_BLOCK_EXCEPTIONS;

    switch ([_currentEvent type]) {
        case NSMouseMoved: {
            NodeImpl *node = event.innerNode.get();
            if (!node)
                return false;
            RenderObject *renderer = node->renderer();
            if (!renderer || !renderer->isWidget())
                return false;
            QWidget *widget = static_cast<RenderWidget *>(renderer)->widget();
            if (!widget || !widget->isKHTMLView())
                return false;
            Frame *subframePart = static_cast<KHTMLView *>(widget)->frame();
            if (!subframePart)
                return false;
            [Mac(subframePart)->bridge() mouseMoved:_currentEvent];
            return true;
        }
        
        case NSLeftMouseDown: {
            NodeImpl *node = event.innerNode.get();
            if (!node) {
                return false;
            }
            RenderObject *renderer = node->renderer();
            if (!renderer || !renderer->isWidget()) {
                return false;
            }
            QWidget *widget = static_cast<RenderWidget *>(renderer)->widget();
            if (!widget || !widget->isKHTMLView())
                return false;
            if (!passWidgetMouseDownEventToWidget(static_cast<RenderWidget *>(renderer))) {
                return false;
            }
            _mouseDownWasInSubframe = true;
            return true;
        }
        case NSLeftMouseUp: {
            if (!_mouseDownWasInSubframe) {
                return false;
            }
            NSView *view = mouseDownViewIfStillGood();
            if (!view) {
                return false;
            }
            ASSERT(!_sendingEventToSubview);
            _sendingEventToSubview = true;
            [view mouseUp:_currentEvent];
            _sendingEventToSubview = false;
            return true;
        }
        case NSLeftMouseDragged: {
            if (!_mouseDownWasInSubframe) {
                return false;
            }
            NSView *view = mouseDownViewIfStillGood();
            if (!view) {
                return false;
            }
            ASSERT(!_sendingEventToSubview);
            _sendingEventToSubview = true;
            [view mouseDragged:_currentEvent];
            _sendingEventToSubview = false;
            return true;
        }
        default:
            return false;
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

bool MacFrame::passWheelEventToChildWidget(NodeImpl *node)
{
    KWQ_BLOCK_EXCEPTIONS;
        
    if ([_currentEvent type] != NSScrollWheel || _sendingEventToSubview || !node) 
        return false;
    else {
        RenderObject *renderer = node->renderer();
        if (!renderer || !renderer->isWidget())
            return false;
        QWidget *widget = static_cast<RenderWidget *>(renderer)->widget();
        if (!widget)
            return false;
            
        NSView *nodeView = widget->getView();
        ASSERT(nodeView);
        ASSERT([nodeView superview]);
        NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:[_currentEvent locationInWindow] fromView:nil]];
    
        ASSERT(view);
        _sendingEventToSubview = true;
        [view scrollWheel:_currentEvent];
        _sendingEventToSubview = false;
        return true;
    }
            
    KWQ_UNBLOCK_EXCEPTIONS;
    return false;
}

void MacFrame::mouseDown(NSEvent *event)
{
    KHTMLView *v = d->m_view;
    if (!v || _sendingEventToSubview) {
        return;
    }

    KWQ_BLOCK_EXCEPTIONS;

    prepareForUserAction();

    _mouseDownView = nil;
    _dragSrc = 0;
    
    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = KWQRetain(event);
    NSPoint loc = [event locationInWindow];
    _mouseDownWinX = (int)loc.x;
    _mouseDownWinY = (int)loc.y;
    d->m_view->viewportToContents(_mouseDownWinX, _mouseDownWinY, _mouseDownX, _mouseDownY);
    _mouseDownTimestamp = [event timestamp];

    _mouseDownMayStartDrag = false;
    _mouseDownMayStartSelect = false;

    QMouseEvent kEvent(QEvent::MouseButtonPress, event);
    v->viewportMousePressEvent(&kEvent);
    
    ASSERT(_currentEvent == event);
    KWQRelease(event);
    _currentEvent = oldCurrentEvent;

    KWQ_UNBLOCK_EXCEPTIONS;
}

void MacFrame::mouseDragged(NSEvent *event)
{
    KHTMLView *v = d->m_view;
    if (!v || _sendingEventToSubview) {
        return;
    }

    KWQ_BLOCK_EXCEPTIONS;

    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = KWQRetain(event);

    QMouseEvent kEvent(QEvent::MouseMove, event);
    v->viewportMouseMoveEvent(&kEvent);
    
    ASSERT(_currentEvent == event);
    KWQRelease(event);
    _currentEvent = oldCurrentEvent;

    KWQ_UNBLOCK_EXCEPTIONS;
}

void MacFrame::mouseUp(NSEvent *event)
{
    KHTMLView *v = d->m_view;
    if (!v || _sendingEventToSubview) {
        return;
    }

    KWQ_BLOCK_EXCEPTIONS;

    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = KWQRetain(event);

    // Our behavior here is a little different that Qt. Qt always sends
    // a mouse release event, even for a double click. To correct problems
    // in khtml's DOM click event handling we do not send a release here
    // for a double click. Instead we send that event from KHTMLView's
    // viewportMouseDoubleClickEvent. Note also that the third click of
    // a triple click is treated as a single click, but the fourth is then
    // treated as another double click. Hence the "% 2" below.
    int clickCount = [event clickCount];
    if (clickCount > 0 && clickCount % 2 == 0) {
        QMouseEvent doubleClickEvent(QEvent::MouseButtonDblClick, event);
        v->viewportMouseDoubleClickEvent(&doubleClickEvent);
    } else {
        QMouseEvent releaseEvent(QEvent::MouseButtonRelease, event);
        v->viewportMouseReleaseEvent(&releaseEvent);
    }
    
    ASSERT(_currentEvent == event);
    KWQRelease(event);
    _currentEvent = oldCurrentEvent;
    
    _mouseDownView = nil;

    KWQ_UNBLOCK_EXCEPTIONS;
}

/*
 A hack for the benefit of AK's PopUpButton, which uses the Carbon menu manager, which thus
 eats all subsequent events after it is starts its modal tracking loop.  After the interaction
 is done, this routine is used to fix things up.  When a mouse down started us tracking in
 the widget, we post a fake mouse up to balance the mouse down we started with. When a 
 key down started us tracking in the widget, we post a fake key up to balance things out.
 In addition, we post a fake mouseMoved to get the cursor in sync with whatever we happen to 
 be over after the tracking is done.
 */
void MacFrame::sendFakeEventsAfterWidgetTracking(NSEvent *initiatingEvent)
{
    KWQ_BLOCK_EXCEPTIONS;

    _sendingEventToSubview = false;
    int eventType = [initiatingEvent type];
    if (eventType == NSLeftMouseDown || eventType == NSKeyDown) {
        NSEvent *fakeEvent = nil;
        if (eventType == NSLeftMouseDown) {
            fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                    location:[initiatingEvent locationInWindow]
                                modifierFlags:[initiatingEvent modifierFlags]
                                    timestamp:[initiatingEvent timestamp]
                                windowNumber:[initiatingEvent windowNumber]
                                        context:[initiatingEvent context]
                                    eventNumber:[initiatingEvent eventNumber]
                                    clickCount:[initiatingEvent clickCount]
                                    pressure:[initiatingEvent pressure]];
        
            mouseUp(fakeEvent);
        }
        else { // eventType == NSKeyDown
            fakeEvent = [NSEvent keyEventWithType:NSKeyUp
                                    location:[initiatingEvent locationInWindow]
                               modifierFlags:[initiatingEvent modifierFlags]
                                   timestamp:[initiatingEvent timestamp]
                                windowNumber:[initiatingEvent windowNumber]
                                     context:[initiatingEvent context]
                                  characters:[initiatingEvent characters] 
                 charactersIgnoringModifiers:[initiatingEvent charactersIgnoringModifiers] 
                                   isARepeat:[initiatingEvent isARepeat] 
                                     keyCode:[initiatingEvent keyCode]];
            keyEvent(fakeEvent);
        }
        // FIXME:  We should really get the current modifierFlags here, but there's no way to poll
        // them in Cocoa, and because the event stream was stolen by the Carbon menu code we have
        // no up-to-date cache of them anywhere.
        fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
                                       location:[[_bridge window] convertScreenToBase:[NSEvent mouseLocation]]
                                  modifierFlags:[initiatingEvent modifierFlags]
                                      timestamp:[initiatingEvent timestamp]
                                   windowNumber:[initiatingEvent windowNumber]
                                        context:[initiatingEvent context]
                                    eventNumber:0
                                     clickCount:0
                                       pressure:0];
        mouseMoved(fakeEvent);
    }
    
    KWQ_UNBLOCK_EXCEPTIONS;
}

void MacFrame::mouseMoved(NSEvent *event)
{
    KHTMLView *v = d->m_view;
    // Reject a mouse moved if the button is down - screws up tracking during autoscroll
    // These happen because WebKit sometimes has to fake up moved events.
    if (!v || d->m_bMousePressed || _sendingEventToSubview) {
        return;
    }
    
    KWQ_BLOCK_EXCEPTIONS;

    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = KWQRetain(event);
    
    QMouseEvent kEvent(QEvent::MouseMove, event);
    v->viewportMouseMoveEvent(&kEvent);
    
    ASSERT(_currentEvent == event);
    KWQRelease(event);
    _currentEvent = oldCurrentEvent;

    KWQ_UNBLOCK_EXCEPTIONS;
}

// Called as we walk up the element chain for nodes with CSS property -khtml-user-drag == auto
bool MacFrame::shouldDragAutoNode(NodeImpl* node, int x, int y) const
{
    // We assume that WebKit only cares about dragging things that can be leaf nodes (text, images, urls).
    // This saves a bunch of expensive calls (creating WC and WK element dicts) as we walk farther up
    // the node hierarchy, and we also don't have to cook up a way to ask WK about non-leaf nodes
    // (since right now WK just hit-tests using a cached lastMouseDown).
    if (!node->hasChildNodes() && d->m_view) {
        int windowX, windowY;
        d->m_view->contentsToViewport(x, y, windowX, windowY);
        NSPoint eventLoc = {windowX, windowY};
        return [_bridge mayStartDragAtEventLocation:eventLoc];
    } else {
        return NO;
    }
}

bool MacFrame::sendContextMenuEvent(NSEvent *event)
{
    DocumentImpl *doc = d->m_doc;
    KHTMLView *v = d->m_view;
    if (!doc || !v) {
        return false;
    }

    bool swallowEvent;
    KWQ_BLOCK_EXCEPTIONS;

    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = KWQRetain(event);
    
    QMouseEvent qev(QEvent::MouseButtonPress, event);

    int xm, ym;
    v->viewportToContents(qev.x(), qev.y(), xm, ym);

    NodeImpl::MouseEvent mev(qev.stateAfter(), NodeImpl::MousePress);
    doc->prepareMouseEvent(false, xm, ym, &mev);

    swallowEvent = v->dispatchMouseEvent(contextmenuEvent,
        mev.innerNode.get(), true, 0, &qev, true, NodeImpl::MousePress);
    if (!swallowEvent && !isPointInsideSelection(xm, ym) &&
        ([_bridge selectWordBeforeMenuEvent] || [_bridge isEditable] || mev.innerNode->isContentEditable())) {
        selectClosestWordFromMouseEvent(&qev, mev.innerNode.get(), xm, ym);
    }

    ASSERT(_currentEvent == event);
    KWQRelease(event);
    _currentEvent = oldCurrentEvent;

    return swallowEvent;

    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

struct ListItemInfo {
    unsigned start;
    unsigned end;
};

NSFileWrapper *MacFrame::fileWrapperForElement(ElementImpl *e)
{
    NSFileWrapper *wrapper = nil;
    KWQ_BLOCK_EXCEPTIONS;
    
    const AtomicString& attr = e->getAttribute(srcAttr);
    if (!attr.isEmpty()) {
        NSURL *URL = completeURL(attr.qstring()).getNSURL();
        wrapper = [_bridge fileWrapperForURL:URL];
    }    
    if (!wrapper) {
        RenderImage *renderer = static_cast<RenderImage *>(e->renderer());
        if (renderer->isImage()) {
            wrapper = [[NSFileWrapper alloc] initRegularFileWithContents:[renderer->pixmap().imageRenderer() TIFFRepresentation]];
            [wrapper setPreferredFilename:@"image.tiff"];
            [wrapper autorelease];
        }
    }

    return wrapper;

    KWQ_UNBLOCK_EXCEPTIONS;

    return nil;
}

static ElementImpl *listParent(ElementImpl *item)
{
    while (!item->hasTagName(ulTag) && !item->hasTagName(olTag)) {
        item = static_cast<ElementImpl *>(item->parentNode());
        if (!item)
            break;
    }
    return item;
}

static NodeImpl* isTextFirstInListItem(NodeImpl *e)
{
    if (!e->isTextNode())
        return 0;
    NodeImpl* par = e->parentNode();
    while (par) {
        if (par->firstChild() != e)
            return 0;
        if (par->hasTagName(liTag))
            return par;
        e = par;
        par = par->parentNode();
    }
    return 0;
}

// FIXME: This collosal function needs to be refactored into maintainable smaller bits.

#define BULLET_CHAR 0x2022
#define SQUARE_CHAR 0x25AA
#define CIRCLE_CHAR 0x25E6

NSAttributedString *MacFrame::attributedString(NodeImpl *_start, int startOffset, NodeImpl *endNode, int endOffset)
{
    NSMutableAttributedString *result;
    KWQ_BLOCK_EXCEPTIONS;

    NodeImpl * _startNode = _start;

    if (_startNode == nil) {
        return nil;
    }

    result = [[[NSMutableAttributedString alloc] init] autorelease];

    bool hasNewLine = true;
    bool addedSpace = true;
    NSAttributedString *pendingStyledSpace = nil;
    bool hasParagraphBreak = true;
    const ElementImpl *linkStartNode = 0;
    unsigned linkStartLocation = 0;
    QPtrList<ElementImpl> listItems;
    QValueList<ListItemInfo> listItemLocations;
    float maxMarkerWidth = 0;
    
    NodeImpl *n = _startNode;
    
    // If the first item is the entire text of a list item, use the list item node as the start of the 
    // selection, not the text node.  The user's intent was probably to select the list.
    if (n->isTextNode() && startOffset == 0) {
        NodeImpl *startListNode = isTextFirstInListItem(_startNode);
        if (startListNode){
            _startNode = startListNode;
            n = _startNode;
        }
    }
    
    while (n) {
        RenderObject *renderer = n->renderer();
        if (renderer) {
            RenderStyle *style = renderer->style();
            NSFont *font = style->font().getNSFont();
            bool needSpace = pendingStyledSpace != nil;
            if (n->isTextNode()) {
                if (hasNewLine) {
                    addedSpace = true;
                    needSpace = false;
                    [pendingStyledSpace release];
                    pendingStyledSpace = nil;
                    hasNewLine = false;
                }
                QString text;
                QString str = n->nodeValue().qstring();
                int start = (n == _startNode) ? startOffset : -1;
                int end = (n == endNode) ? endOffset : -1;
                if (renderer->isText()) {
                    if (!style->collapseWhiteSpace()) {
                        if (needSpace && !addedSpace) {
                            if (text.isEmpty() && linkStartLocation == [result length]) {
                                ++linkStartLocation;
                            }
                            [result appendAttributedString:pendingStyledSpace];
                        }
                        int runStart = (start == -1) ? 0 : start;
                        int runEnd = (end == -1) ? str.length() : end;
                        text += str.mid(runStart, runEnd-runStart);
                        [pendingStyledSpace release];
                        pendingStyledSpace = nil;
                        addedSpace = str[runEnd-1].direction() == QChar::DirWS;
                    }
                    else {
                        RenderText* textObj = static_cast<RenderText*>(renderer);
                        if (!textObj->firstTextBox() && str.length() > 0 && !addedSpace) {
                            // We have no runs, but we do have a length.  This means we must be
                            // whitespace that collapsed away at the end of a line.
                            text += ' ';
                            addedSpace = true;
                        }
                        else {
                            addedSpace = false;
                            for (InlineTextBox* box = textObj->firstTextBox(); box; box = box->nextTextBox()) {
                                int runStart = (start == -1) ? box->m_start : start;
                                int runEnd = (end == -1) ? box->m_start + box->m_len : end;
                                runEnd = kMin(runEnd, box->m_start + box->m_len);
                                if (runStart >= box->m_start &&
                                    runStart < box->m_start + box->m_len) {
                                    if (box == textObj->firstTextBox() && box->m_start == runStart && runStart > 0) {
                                        needSpace = true; // collapsed space at the start
                                    }
                                    if (needSpace && !addedSpace) {
                                        if (pendingStyledSpace != nil) {
                                            if (text.isEmpty() && linkStartLocation == [result length]) {
                                                ++linkStartLocation;
                                            }
                                            [result appendAttributedString:pendingStyledSpace];
                                        } else {
                                            text += ' ';
                                        }
                                    }
                                    QString runText = str.mid(runStart, runEnd - runStart);
                                    runText.replace('\n', ' ');
                                    text += runText;
                                    int nextRunStart = box->nextTextBox() ? box->nextTextBox()->m_start : str.length(); // collapsed space between runs or at the end
                                    needSpace = nextRunStart > runEnd;
                                    [pendingStyledSpace release];
                                    pendingStyledSpace = nil;
                                    addedSpace = str[runEnd-1].direction() == QChar::DirWS;
                                    start = -1;
                                }
                                if (end != -1 && runEnd >= end)
                                    break;
                            }
                        }
                    }
                }
                
                text.replace(QChar('\\'), renderer->backslashAsCurrencySymbol());
    
                if (text.length() > 0 || needSpace) {
                    NSMutableDictionary *attrs = [[NSMutableDictionary alloc] init];
                    [attrs setObject:font forKey:NSFontAttributeName];
                    if (style && style->color().isValid() && qAlpha(style->color().rgb()) != 0)
                        [attrs setObject:nsColor(style->color()) forKey:NSForegroundColorAttributeName];
                    if (style && style->backgroundColor().isValid() && qAlpha(style->backgroundColor().rgb()) != 0)
                        [attrs setObject:nsColor(style->backgroundColor()) forKey:NSBackgroundColorAttributeName];

                    if (text.length() > 0) {
                        hasParagraphBreak = false;
                        NSAttributedString *partialString = [[NSAttributedString alloc] initWithString:text.getNSString() attributes:attrs];
                        [result appendAttributedString: partialString];                
                        [partialString release];
                    }

                    if (needSpace) {
                        [pendingStyledSpace release];
                        pendingStyledSpace = [[NSAttributedString alloc] initWithString:@" " attributes:attrs];
                    }

                    [attrs release];
                }
            } else {
                // This is our simple HTML -> ASCII transformation:
                QString text;
                if (n->hasTagName(aTag)) {
                    // Note the start of the <a> element.  We will add the NSLinkAttributeName
                    // attribute to the attributed string when navigating to the next sibling 
                    // of this node.
                    linkStartLocation = [result length];
                    linkStartNode = static_cast<ElementImpl*>(n);
                } else if (n->hasTagName(brTag)) {
                    text += "\n";
                    hasNewLine = true;
                } else if (n->hasTagName(liTag)) {
                    QString listText;
                    ElementImpl *itemParent = listParent(static_cast<ElementImpl *>(n));
                    
                    if (!hasNewLine)
                        listText += '\n';
                    hasNewLine = true;

                    listItems.append(static_cast<ElementImpl*>(n));
                    ListItemInfo info;
                    info.start = [result length];
                    info.end = 0;
                    listItemLocations.append (info);
                    
                    listText += '\t';
                    if (itemParent && renderer->isListItem()) {
                        RenderListItem *listRenderer = static_cast<RenderListItem*>(renderer);

                        maxMarkerWidth = MAX([font pointSize], maxMarkerWidth);
                        switch(style->listStyleType()) {
                            case khtml::DISC:
                                listText += ((QChar)BULLET_CHAR);
                                break;
                            case khtml::CIRCLE:
                                listText += ((QChar)CIRCLE_CHAR);
                                break;
                            case khtml::SQUARE:
                                listText += ((QChar)SQUARE_CHAR);
                                break;
                            case khtml::LNONE:
                                break;
                            default:
                                QString marker = listRenderer->markerStringValue();
                                listText += marker;
                                // Use AppKit metrics.  Will be rendered by AppKit.
                                float markerWidth = [font widthOfString: marker.getNSString()];
                                maxMarkerWidth = MAX(markerWidth, maxMarkerWidth);
                        }

                        listText += ' ';
                        listText += '\t';

                        NSMutableDictionary *attrs = [[NSMutableDictionary alloc] init];
                        [attrs setObject:font forKey:NSFontAttributeName];
                        if (style && style->color().isValid())
                            [attrs setObject:nsColor(style->color()) forKey:NSForegroundColorAttributeName];
                        if (style && style->backgroundColor().isValid())
                            [attrs setObject:nsColor(style->backgroundColor()) forKey:NSBackgroundColorAttributeName];

                        NSAttributedString *partialString = [[NSAttributedString alloc] initWithString:listText.getNSString() attributes:attrs];
                        [attrs release];
                        [result appendAttributedString: partialString];                
                        [partialString release];
                    }
                } else if (n->hasTagName(olTag) || n->hasTagName(ulTag)) {
                    if (!hasNewLine)
                        text += "\n";
                    hasNewLine = true;
                } else if (n->hasTagName(tdTag) ||
                           n->hasTagName(thTag) ||
                           n->hasTagName(hrTag) ||
                           n->hasTagName(ddTag) ||
                           n->hasTagName(dlTag) ||
                           n->hasTagName(dtTag) ||
                           n->hasTagName(preTag) ||
                           n->hasTagName(blockquoteTag) ||
                           n->hasTagName(divTag)) {
                    if (!hasNewLine)
                        text += '\n';
                    hasNewLine = true;
                } else if (n->hasTagName(pTag) ||
                           n->hasTagName(trTag) ||
                           n->hasTagName(h1Tag) ||
                           n->hasTagName(h2Tag) ||
                           n->hasTagName(h3Tag) ||
                           n->hasTagName(h4Tag) ||
                           n->hasTagName(h5Tag) ||
                           n->hasTagName(h6Tag)) {
                    if (!hasNewLine)
                        text += '\n';
                    
                    // In certain cases, emit a paragraph break.
                    int bottomMargin = renderer->collapsedMarginBottom();
                    int fontSize = style->htmlFont().getFontDef().computedPixelSize();
                    if (bottomMargin * 2 >= fontSize) {
                        if (!hasParagraphBreak) {
                            text += '\n';
                            hasParagraphBreak = true;
                        }
                    }
                    
                    hasNewLine = true;
                }
                else if (n->hasTagName(imgTag)) {
                    if (pendingStyledSpace != nil) {
                        if (linkStartLocation == [result length]) {
                            ++linkStartLocation;
                        }
                        [result appendAttributedString:pendingStyledSpace];
                        [pendingStyledSpace release];
                        pendingStyledSpace = nil;
                    }
                    NSFileWrapper *fileWrapper = fileWrapperForElement(static_cast<ElementImpl *>(n));
                    NSTextAttachment *attachment = [[NSTextAttachment alloc] initWithFileWrapper:fileWrapper];
                    NSAttributedString *iString = [NSAttributedString attributedStringWithAttachment:attachment];
                    [result appendAttributedString: iString];
                    [attachment release];
                }

                NSAttributedString *partialString = [[NSAttributedString alloc] initWithString:text.getNSString()];
                [result appendAttributedString: partialString];
                [partialString release];
            }
        }

        if (n == endNode)
            break;

        NodeImpl *next = n->firstChild();
        if (!next) {
            next = n->nextSibling();
        }

        while (!next && n->parentNode()) {
            QString text;
            n = n->parentNode();
            if (n == endNode)
                break;
            next = n->nextSibling();

            if (n->hasTagName(aTag)) {
                // End of a <a> element.  Create an attributed string NSLinkAttributeName attribute
                // for the range of the link.  Note that we create the attributed string from the DOM, which
                // will have corrected any illegally nested <a> elements.
                if (linkStartNode && n == linkStartNode) {
                    DOMString href = parseURL(linkStartNode->getAttribute(hrefAttr));
                    KURL kURL = Mac(linkStartNode->getDocument()->frame())->completeURL(href.qstring());
                    
                    NSURL *URL = kURL.getNSURL();
                    NSRange tempRange = { linkStartLocation, [result length]-linkStartLocation }; // workaround for 4213314
                    [result addAttribute:NSLinkAttributeName value:URL range:tempRange];
                    linkStartNode = 0;
                }
            }
            else if (n->hasTagName(olTag) || n->hasTagName(ulTag)) {
                if (!hasNewLine)
                    text += '\n';
                hasNewLine = true;
            } else if (n->hasTagName(liTag)) {
                
                int i, count = listItems.count();
                for (i = 0; i < count; i++){
                    if (listItems.at(i) == n){
                        listItemLocations[i].end = [result length];
                        break;
                    }
                }
                if (!hasNewLine)
                    text += '\n';
                hasNewLine = true;
            } else if (n->hasTagName(tdTag) ||
                       n->hasTagName(thTag) ||
                       n->hasTagName(hrTag) ||
                       n->hasTagName(ddTag) ||
                       n->hasTagName(dlTag) ||
                       n->hasTagName(dtTag) ||
                       n->hasTagName(preTag) ||
                       n->hasTagName(blockquoteTag) ||
                       n->hasTagName(divTag)) {
                if (!hasNewLine)
                    text += '\n';
                hasNewLine = true;
            } else if (n->hasTagName(pTag) ||
                       n->hasTagName(trTag) ||
                       n->hasTagName(h1Tag) ||
                       n->hasTagName(h2Tag) ||
                       n->hasTagName(h3Tag) ||
                       n->hasTagName(h4Tag) ||
                       n->hasTagName(h5Tag) ||
                       n->hasTagName(h6Tag)) {
                if (!hasNewLine)
                    text += '\n';
                // An extra newline is needed at the start, not the end, of these types of tags,
                // so don't add another here.
                hasNewLine = true;
            }
            
            NSAttributedString *partialString = [[NSAttributedString alloc] initWithString:text.getNSString()];
            [result appendAttributedString:partialString];
            [partialString release];
        }

        n = next;
    }
    
    [pendingStyledSpace release];
    
    // Apply paragraph styles from outside in.  This ensures that nested lists correctly
    // override their parent's paragraph style.
    {
        unsigned i, count = listItems.count();
        ElementImpl *e;
        ListItemInfo info;

#ifdef POSITION_LIST
        NodeImpl *containingBlock;
        int containingBlockX, containingBlockY;
        
        // Determine the position of the outermost containing block.  All paragraph
        // styles and tabs should be relative to this position.  So, the horizontal position of 
        // each item in the list (in the resulting attributed string) will be relative to position 
        // of the outermost containing block.
        if (count > 0){
            containingBlock = _startNode;
            while (containingBlock->renderer()->isInline()){
                containingBlock = containingBlock->parentNode();
            }
            containingBlock->renderer()->absolutePosition(containingBlockX, containingBlockY);
        }
#endif
        
        for (i = 0; i < count; i++){
            e = listItems.at(i);
            info = listItemLocations[i];
            
            if (info.end < info.start)
                info.end = [result length];
                
            RenderObject *r = e->renderer();
            RenderStyle *style = r->style();

            int rx;
            NSFont *font = style->font().getNSFont();
            float pointSize = [font pointSize];

#ifdef POSITION_LIST
            int ry;
            r->absolutePosition(rx, ry);
            rx -= containingBlockX;
            
            // Ensure that the text is indented at least enough to allow for the markers.
            rx = MAX(rx, (int)maxMarkerWidth);
#else
            rx = (int)MAX(maxMarkerWidth, pointSize);
#endif

            // The bullet text will be right aligned at the first tab marker, followed
            // by a space, followed by the list item text.  The space is arbitrarily
            // picked as pointSize*2/3.  The space on the first line of the text item
            // is established by a left aligned tab, on subsequent lines it's established
            // by the head indent.
            NSMutableParagraphStyle *mps = [[NSMutableParagraphStyle alloc] init];
            [mps setFirstLineHeadIndent: 0];
            [mps setHeadIndent: rx];
            [mps setTabStops:[NSArray arrayWithObjects:
                        [[[NSTextTab alloc] initWithType:NSRightTabStopType location:rx-(pointSize*2/3)] autorelease],
                        [[[NSTextTab alloc] initWithType:NSLeftTabStopType location:rx] autorelease],
                        nil]];
            NSRange tempRange = { info.start, info.end-info.start }; // workaround for 4213314
            [result addAttribute:NSParagraphStyleAttributeName value:mps range:tempRange];
            [mps release];
        }
    }

    return result;

    KWQ_UNBLOCK_EXCEPTIONS;

    return nil;
}

// returns NSRect because going through IntRect would truncate any floats
NSRect MacFrame::visibleSelectionRect() const
{
    if (!d->m_view) {
        return NSZeroRect;
    }
    NSView *documentView = d->m_view->getDocumentView();
    if (!documentView) {
        return NSZeroRect;
    }
    return NSIntersectionRect(selectionRect(), [documentView visibleRect]);     
}

NSImage *MacFrame::imageFromRect(NSRect rect) const
{
    NSView *view = d->m_view->getDocumentView();
    if (!view) {
        return nil;
    }
    
    NSImage *resultImage;
    KWQ_BLOCK_EXCEPTIONS;
    
    NSRect bounds = [view bounds];
    resultImage = [[[NSImage alloc] initWithSize:rect.size] autorelease];

    if (rect.size.width != 0 && rect.size.height != 0) {
        [resultImage setFlipped:YES];
        [resultImage lockFocus];

        CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

        CGContextSaveGState(context);
        CGContextTranslateCTM(context, bounds.origin.x - rect.origin.x, bounds.origin.y - rect.origin.y);
        [view drawRect:rect];
        CGContextRestoreGState(context);

        [resultImage unlockFocus];
        [resultImage setFlipped:NO];
    }

    return resultImage;

    KWQ_UNBLOCK_EXCEPTIONS;
    
    return nil;
}

NSImage *MacFrame::selectionImage() const
{
    _drawSelectionOnly = true;  // invoke special drawing mode
    NSImage *result = imageFromRect(visibleSelectionRect());
    _drawSelectionOnly = false;
    return result;
}

NSImage *MacFrame::snapshotDragImage(NodeImpl *node, NSRect *imageRect, NSRect *elementRect) const
{
    RenderObject *renderer = node->renderer();
    if (!renderer) {
        return nil;
    }
    
    renderer->updateDragState(true);    // mark dragged nodes (so they pick up the right CSS)
    d->m_doc->updateLayout();        // forces style recalc - needed since changing the drag state might
                                        // imply new styles, plus JS could have changed other things
    IntRect topLevelRect;
    NSRect paintingRect = renderer->paintingRootRect(topLevelRect);

    _elementToDraw = node;              // invoke special sub-tree drawing mode
    NSImage *result = imageFromRect(paintingRect);
    renderer->updateDragState(false);
    d->m_doc->updateLayout();
    _elementToDraw = 0;

    if (elementRect) {
        *elementRect = topLevelRect;
    }
    if (imageRect) {
        *imageRect = paintingRect;
    }
    return result;
}

NSFont *MacFrame::fontForSelection(bool *hasMultipleFonts) const
{
    if (hasMultipleFonts)
        *hasMultipleFonts = false;

    if (!d->m_selection.isRange()) {
        NodeImpl *nodeToRemove;
        RenderStyle *style = styleForSelectionStart(nodeToRemove); // sets nodeToRemove

        NSFont *result = 0;
        if (style)
            result = style->font().getNSFont();
        
        if (nodeToRemove) {
            int exceptionCode;
            nodeToRemove->remove(exceptionCode);
            ASSERT(exceptionCode == 0);
        }

        return result;
    }

    NSFont *font = nil;

    RefPtr<RangeImpl> range = d->m_selection.toRange();
    NodeImpl *startNode = range->editingStartPosition().node();
    if (startNode != nil) {
        NodeImpl *pastEnd = range->pastEndNode();
        // In the loop below, n should eventually match pastEnd and not become nil, but we've seen at least one
        // unreproducible case where this didn't happen, so check for nil also.
        for (NodeImpl *n = startNode; n && n != pastEnd; n = n->traverseNextNode()) {
            RenderObject *renderer = n->renderer();
            if (!renderer)
                continue;
            // FIXME: Are there any node types that have renderers, but that we should be skipping?
            NSFont *f = renderer->style()->font().getNSFont();
            if (font == nil) {
                font = f;
                if (!hasMultipleFonts)
                    break;
            } else if (font != f) {
                *hasMultipleFonts = true;
                break;
            }
        }
    }

    return font;
}

NSDictionary *MacFrame::fontAttributesForSelectionStart() const
{
    NodeImpl *nodeToRemove;
    RenderStyle *style = styleForSelectionStart(nodeToRemove);
    if (!style)
        return nil;

    NSMutableDictionary *result = [NSMutableDictionary dictionary];

    if (style->backgroundColor().isValid() && style->backgroundColor().alpha() != 0)
        [result setObject:nsColor(style->backgroundColor()) forKey:NSBackgroundColorAttributeName];

    if (style->font().getNSFont())
        [result setObject:style->font().getNSFont() forKey:NSFontAttributeName];

    if (style->color().isValid() && style->color() != black)
        [result setObject:nsColor(style->color()) forKey:NSForegroundColorAttributeName];

    ShadowData *shadow = style->textShadow();
    if (shadow) {
        NSShadow *s = [[NSShadow alloc] init];
        [s setShadowOffset:NSMakeSize(shadow->x, shadow->y)];
        [s setShadowBlurRadius:shadow->blur];
        [s setShadowColor:nsColor(shadow->color)];
        [result setObject:s forKey:NSShadowAttributeName];
    }

    int decoration = style->textDecorationsInEffect();
    if (decoration & khtml::LINE_THROUGH)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];

    int superscriptInt = 0;
    switch (style->verticalAlign()) {
        case khtml::BASELINE:
        case khtml::BOTTOM:
        case khtml::BASELINE_MIDDLE:
        case khtml::LENGTH:
        case khtml::MIDDLE:
        case khtml::TEXT_BOTTOM:
        case khtml::TEXT_TOP:
        case khtml::TOP:
            break;
        case khtml::SUB:
            superscriptInt = -1;
            break;
        case khtml::SUPER:
            superscriptInt = 1;
            break;
    }
    if (superscriptInt)
        [result setObject:[NSNumber numberWithInt:superscriptInt] forKey:NSSuperscriptAttributeName];

    if (decoration & khtml::UNDERLINE)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];

    if (nodeToRemove) {
        int exceptionCode = 0;
        nodeToRemove->remove(exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    return result;
}

NSWritingDirection MacFrame::baseWritingDirectionForSelectionStart() const
{
    NSWritingDirection result = NSWritingDirectionLeftToRight;

    Position pos = VisiblePosition(d->m_selection.start(), d->m_selection.affinity()).deepEquivalent();
    NodeImpl *node = pos.node();
    if (!node || !node->renderer() || !node->renderer()->containingBlock())
        return result;
    RenderStyle *style = node->renderer()->containingBlock()->style();
    if (!style)
        return result;
        
    switch (style->direction()) {
        case khtml::LTR:
            result = NSWritingDirectionLeftToRight;
            break;
        case khtml::RTL:
            result = NSWritingDirectionRightToLeft;
            break;
    }

    return result;
}

KWQWindowWidget *MacFrame::topLevelWidget()
{
    return _windowWidget;
}

void MacFrame::tokenizerProcessedData()
{
    if (d->m_doc) {
        checkCompleted();
    }
    [_bridge tokenizerProcessedData];
}

void MacFrame::setBridge(WebCoreFrameBridge *p)
{ 
    if (_bridge != p)
        delete _windowWidget;
    _bridge = p;
    _windowWidget = new KWQWindowWidget(_bridge);
}

QString MacFrame::overrideMediaType() const
{
    NSString *overrideType = [_bridge overrideMediaType];
    if (overrideType)
        return QString::fromNSString(overrideType);
    return QString();
}

void MacFrame::setDisplaysWithFocusAttributes(bool flag)
{
    if (d->m_isFocused == flag)
        return;
        
    d->m_isFocused = flag;

    // This method does the job of updating the view based on whether the view is "active".
    // This involves four kinds of drawing updates:

    // 1. The background color used to draw behind selected content (active | inactive color)
    if (d->m_view)
        d->m_view->updateContents(IntRect(visibleSelectionRect()));

    // 2. Caret blinking (blinks | does not blink)
    if (flag)
        setSelectionFromNone();
    setCaretVisible(flag);
    
    // 3. The drawing of a focus ring around links in web pages.
    DocumentImpl *doc = xmlDocImpl();
    if (doc) {
        NodeImpl *node = doc->focusNode();
        if (node) {
            node->setChanged();
            if (node->renderer() && node->renderer()->style()->hasAppearance())
                theme()->stateChanged(node->renderer(), FocusState);
        }
    }
    
    // 4. Changing the tint of controls from clear to aqua/graphite and vice versa.  We
    // do a "fake" paint.  When the theme gets a paint call, it can then do an invalidate.
    if (theme()->supportsControlTints()) {
        NSView *documentView = d->m_view ? d->m_view->getDocumentView() : 0;
        if (documentView && renderer()) {
            doc->updateLayout(); // Ensure layout is up to date.
            IntRect visibleRect([documentView visibleRect]);
            QPainter p;
            p.setUpdatingControlTints(true);
            paint(&p, visibleRect);
        }
    }
}

NSColor *MacFrame::bodyBackgroundColor() const
{
    if (xmlDocImpl() && xmlDocImpl()->body() && xmlDocImpl()->body()->renderer()) {
        QColor bgColor = xmlDocImpl()->body()->renderer()->style()->backgroundColor();
        if (bgColor.isValid()) {
            return nsColor(bgColor);
        }
    }
    return nil;
}

WebCoreKeyboardUIMode MacFrame::keyboardUIMode() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return [_bridge keyboardUIMode];
    KWQ_UNBLOCK_EXCEPTIONS;

    return WebCoreKeyboardAccessDefault;
}

void MacFrame::didTellBridgeAboutLoad(const DOM::DOMString& URL)
{
    urlsBridgeKnowsAbout.insert(URL.impl());
}

bool MacFrame::haveToldBridgeAboutLoad(const DOM::DOMString& URL)
{
    return urlsBridgeKnowsAbout.contains(URL.impl());
}

void MacFrame::clear()
{
    urlsBridgeKnowsAbout.clear();
    setMarkedTextRange(0, nil, nil);
    Frame::clear();
}

void Frame::print()
{
    [Mac(this)->_bridge print];
}

KJS::Bindings::Instance *MacFrame::getAppletInstanceForWidget(QWidget *widget)
{
    NSView *aView = widget->getView();
    if (!aView)
        return 0;
    jobject applet;
    
    // Get a pointer to the actual Java applet instance.
    if ([_bridge respondsToSelector:@selector(getAppletInView:)])
        applet = [_bridge getAppletInView:aView];
    else
        applet = [_bridge pollForAppletInView:aView];
    
    if (applet) {
        // Wrap the Java instance in a language neutral binding and hand
        // off ownership to the APPLET element.
        KJS::Bindings::RootObject *executionContext = KJS::Bindings::RootObject::findRootObjectForNativeHandleFunction ()(aView);
        KJS::Bindings::Instance *instance = KJS::Bindings::Instance::createBindingForLanguageInstance (KJS::Bindings::Instance::JavaLanguage, applet, executionContext);        
        return instance;
    }
    
    return 0;
}

@interface NSObject (WebPlugIn)
- (id)objectForWebScript;
- (void *)pluginScriptableObject;
@end

static KJS::Bindings::Instance *getInstanceForView(NSView *aView)
{
    if ([aView respondsToSelector:@selector(objectForWebScript)]){
        id object = [aView objectForWebScript];
        if (object) {
            KJS::Bindings::RootObject *executionContext = KJS::Bindings::RootObject::findRootObjectForNativeHandleFunction ()(aView);
            return KJS::Bindings::Instance::createBindingForLanguageInstance (KJS::Bindings::Instance::ObjectiveCLanguage, object, executionContext);
        }
    }
    else if ([aView respondsToSelector:@selector(pluginScriptableObject)]){
        void *object = [aView pluginScriptableObject];
        if (object) {
            KJS::Bindings::RootObject *executionContext = KJS::Bindings::RootObject::findRootObjectForNativeHandleFunction ()(aView);
            return KJS::Bindings::Instance::createBindingForLanguageInstance (KJS::Bindings::Instance::CLanguage, object, executionContext);
        }
    }
    return 0;
}

KJS::Bindings::Instance *MacFrame::getEmbedInstanceForWidget(QWidget *widget)
{
    return getInstanceForView(widget->getView());
}

KJS::Bindings::Instance *MacFrame::getObjectInstanceForWidget(QWidget *widget)
{
    return getInstanceForView(widget->getView());
}

void MacFrame::addPluginRootObject(const KJS::Bindings::RootObject *root)
{
    rootObjects.append (root);
}

void MacFrame::cleanupPluginRootObjects()
{
    JSLock lock;

    KJS::Bindings::RootObject *root;
    while ((root = rootObjects.getLast())) {
        root->removeAllNativeReferences();
        rootObjects.removeLast();
    }
}

void MacFrame::registerCommandForUndoOrRedo(const EditCommandPtr &cmd, bool isRedo)
{
    ASSERT(cmd.get());
    KWQEditCommand *kwq = [KWQEditCommand commandWithEditCommand:cmd.get()];
    NSUndoManager *undoManager = [_bridge undoManager];
    [undoManager registerUndoWithTarget:_bridge selector:(isRedo ? @selector(redoEditing:) : @selector(undoEditing:)) object:kwq];
    NSString *actionName = [_bridge nameForUndoAction:static_cast<WebUndoAction>(cmd.editingAction())];
    if (actionName != nil) {
        [undoManager setActionName:actionName];
    }
    _haveUndoRedoOperations = YES;
}

void MacFrame::registerCommandForUndo(const EditCommandPtr &cmd)
{
    registerCommandForUndoOrRedo(cmd, NO);
}

void MacFrame::registerCommandForRedo(const EditCommandPtr &cmd)
{
    registerCommandForUndoOrRedo(cmd, YES);
}

void MacFrame::clearUndoRedoOperations()
{
    if (_haveUndoRedoOperations) {
        [[_bridge undoManager] removeAllActionsWithTarget:_bridge];
        _haveUndoRedoOperations = NO;
    }
}

void MacFrame::issueUndoCommand()
{
    if (canUndo())
        [[_bridge undoManager] undo];
}

void MacFrame::issueRedoCommand()
{
    if (canRedo())
        [[_bridge undoManager] redo];
}

void MacFrame::issueCutCommand()
{
    [_bridge issueCutCommand];
}

void MacFrame::issueCopyCommand()
{
    [_bridge issueCopyCommand];
}

void MacFrame::issuePasteCommand()
{
    [_bridge issuePasteCommand];
}

void MacFrame::issuePasteAndMatchStyleCommand()
{
    [_bridge issuePasteAndMatchStyleCommand];
}

void MacFrame::issueTransposeCommand()
{
    [_bridge issueTransposeCommand];
}

bool Frame::canUndo() const
{
    return [[Mac(this)->_bridge undoManager] canUndo];
}

bool Frame::canRedo() const
{
    return [[Mac(this)->_bridge undoManager] canRedo];
}

bool Frame::canPaste() const
{
    return [Mac(this)->_bridge canPaste];
}

void MacFrame::markMisspellingsInAdjacentWords(const VisiblePosition &p)
{
    if (![_bridge isContinuousSpellCheckingEnabled])
        return;
    markMisspellings(SelectionController(startOfWord(p, LeftWordIfOnBoundary), endOfWord(p, RightWordIfOnBoundary)));
}

void MacFrame::markMisspellings(const SelectionController &selection)
{
    // This function is called with a selection already expanded to word boundaries.
    // Might be nice to assert that here.

    if (![_bridge isContinuousSpellCheckingEnabled])
        return;

    RefPtr<RangeImpl> searchRange(selection.toRange());
    if (!searchRange || searchRange->isDetached())
        return;
    
    // If we're not in an editable node, bail.
    int exception = 0;
    NodeImpl *editableNodeImpl = searchRange->startContainer(exception);
    if (!editableNodeImpl->isContentEditable())
        return;
    
    // Get the spell checker if it is available
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (checker == nil)
        return;
    
    WordAwareIterator it(searchRange.get());
    
    while (!it.atEnd()) {      // we may be starting at the end of the doc, and already by atEnd
        const QChar *chars = it.characters();
        int len = it.length();
        if (len > 1 || !chars[0].isSpace()) {
            NSString *chunk = [[NSString alloc] initWithCharactersNoCopy:(unichar *)chars length:len freeWhenDone:NO];
            int startIndex = 0;
            // Loop over the chunk to find each misspelling in it.
            while (startIndex < len) {
                NSRange misspelling = [checker checkSpellingOfString:chunk startingAt:startIndex language:nil wrap:NO inSpellDocumentWithTag:[_bridge spellCheckerDocumentTag] wordCount:NULL];
                if (misspelling.length == 0) {
                    break;
                }
                else {
                    // Build up result range and string.  Note the misspelling may span many text nodes,
                    // but the CharIterator insulates us from this complexity
                    RefPtr<RangeImpl> misspellingRange(rangeOfContents(xmlDocImpl()));
                    CharacterIterator chars(it.range().get());
                    chars.advance(misspelling.location);
                    misspellingRange->setStart(chars.range()->startContainer(exception), chars.range()->startOffset(exception), exception);
                    chars.advance(misspelling.length);
                    misspellingRange->setEnd(chars.range()->startContainer(exception), chars.range()->startOffset(exception), exception);
                    // Mark misspelling in document.
                    xmlDocImpl()->addMarker(misspellingRange.get(), DocumentMarker::Spelling);
                    startIndex = misspelling.location + misspelling.length;
                }
            }
            [chunk release];
        }
    
        it.advance();
    }
}

void MacFrame::respondToChangedSelection(const SelectionController &oldSelection, bool closeTyping)
{
    if (xmlDocImpl()) {
        if ([_bridge isContinuousSpellCheckingEnabled]) {
            SelectionController oldAdjacentWords = SelectionController();
            
            // If this is a change in selection resulting from a delete operation, oldSelection may no longer
            // be in the document.
            if (oldSelection.start().node() && oldSelection.start().node()->inDocument()) {
                VisiblePosition oldStart(oldSelection.start(), oldSelection.affinity());
                oldAdjacentWords = SelectionController(startOfWord(oldStart, LeftWordIfOnBoundary), endOfWord(oldStart, RightWordIfOnBoundary));   
            }

            VisiblePosition newStart(selection().start(), selection().affinity());
            SelectionController newAdjacentWords(startOfWord(newStart, LeftWordIfOnBoundary), endOfWord(newStart, RightWordIfOnBoundary));

            if (oldAdjacentWords != newAdjacentWords) {
                // Mark misspellings in the portion that was previously unmarked because of
                // the proximity of the start of the selection. We only spell check words in
                // the vicinity of the start of the old selection because the spelling checker
                // is not fast enough to do a lot of spelling checking implicitly. This matches
                // AppKit. This function is really the only code that knows that rule. The
                // markMisspellings function is prepared to handler larger ranges.

                // When typing we check spelling elsewhere, so don't redo it here.
                if (closeTyping) {
                    markMisspellings(oldAdjacentWords);
                }

                // This only erases a marker in the first word of the selection.
                // Perhaps peculiar, but it matches AppKit.
                xmlDocImpl()->removeMarkers(newAdjacentWords.toRange().get(), DocumentMarker::Spelling);
            }
        } else {
            // When continuous spell checking is off, no markers appear after the selection changes.
            xmlDocImpl()->removeMarkers(DocumentMarker::Spelling);
        }
    }

    [_bridge respondToChangedSelection];
}

bool MacFrame::shouldChangeSelection(const SelectionController &oldSelection, const SelectionController &newSelection, khtml::EAffinity affinity, bool stillSelecting) const
{
    return [_bridge shouldChangeSelectedDOMRange:[DOMRange _rangeWithImpl:oldSelection.toRange().get()]
                                      toDOMRange:[DOMRange _rangeWithImpl:newSelection.toRange().get()]
                                        affinity:static_cast<NSSelectionAffinity>(affinity)
                                  stillSelecting:stillSelecting];
}

void MacFrame::respondToChangedContents()
{
    if (KWQAccObjectCache::accessibilityEnabled())
        renderer()->document()->getAccObjectCache()->postNotificationToTopWebArea(renderer(), "AXValueChanged");
    [_bridge respondToChangedContents];
}

bool MacFrame::isContentEditable() const
{
    return Frame::isContentEditable() || [_bridge isEditable];
}

bool MacFrame::shouldBeginEditing(const RangeImpl *range) const
{
    ASSERT(range);
    return [_bridge shouldBeginEditing:[DOMRange _rangeWithImpl:const_cast<RangeImpl *>(range)]];
}

bool MacFrame::shouldEndEditing(const RangeImpl *range) const
{
    ASSERT(range);
    return [_bridge shouldEndEditing:[DOMRange _rangeWithImpl:const_cast<RangeImpl *>(range)]];
}

static QValueList<MarkedTextUnderline> convertAttributesToUnderlines(const RangeImpl *markedTextRange, NSArray *attributes, NSArray *ranges)
{
    QValueList<MarkedTextUnderline> result;

    int exception = 0;
    int baseOffset = markedTextRange->startOffset(exception);

    unsigned length = [attributes count];
    ASSERT([ranges count] == length);

    for (unsigned i = 0; i < length; i++) {
        NSNumber *style = [[attributes objectAtIndex:i] objectForKey:NSUnderlineStyleAttributeName];
        if (!style)
            continue;
        NSRange range = [[ranges objectAtIndex:i] rangeValue];
        NSColor *color = [[attributes objectAtIndex:i] objectForKey:NSUnderlineColorAttributeName];
        QColor qColor = Qt::black;
        if (color) {
            NSColor* deviceColor = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
            qColor = QColor(qRgba((int)(255 * [deviceColor redComponent]),
                                  (int)(255 * [deviceColor blueComponent]),
                                  (int)(255 * [deviceColor greenComponent]),
                                  (int)(255 * [deviceColor alphaComponent])));
        }

        result.append(MarkedTextUnderline(range.location + baseOffset, 
                                          range.location + baseOffset + range.length, 
                                          qColor,
                                          [style intValue] > 1));
    }

    return result;
}

void MacFrame::setMarkedTextRange(const RangeImpl *range, NSArray *attributes, NSArray *ranges)
{
    int exception = 0;

    ASSERT(!range || range->startContainer(exception) == range->endContainer(exception));
    ASSERT(!range || range->collapsed(exception) || range->startContainer(exception)->nodeType() == DOM::Node::TEXT_NODE);

    if (attributes == nil) {
        m_markedTextUsesUnderlines = false;
        m_markedTextUnderlines.clear();
    } else {
        m_markedTextUsesUnderlines = true;
        m_markedTextUnderlines = convertAttributesToUnderlines(range, attributes, ranges);
    }

    if (m_markedTextRange.get() && xmlDocImpl() && m_markedTextRange->startContainer(exception)->renderer())
        m_markedTextRange->startContainer(exception)->renderer()->repaint();

    if ( range && range->collapsed(exception) ) {
        m_markedTextRange = 0;
    } else {
        m_markedTextRange = const_cast<RangeImpl *>(range);
    }

    if (m_markedTextRange.get() && xmlDocImpl() && m_markedTextRange->startContainer(exception)->renderer()) {
        m_markedTextRange->startContainer(exception)->renderer()->repaint();
    }
}

bool MacFrame::canGoBackOrForward(int distance) const
{
    return [_bridge canGoBackOrForward:distance];
}

void MacFrame::didFirstLayout()
{
    [_bridge didFirstLayout];
}

NSMutableDictionary *MacFrame::dashboardRegionsDictionary()
{
    DocumentImpl *doc = xmlDocImpl();
    if (!doc) {
        return nil;
    }

    const QValueList<DashboardRegionValue> regions = doc->dashboardRegions();
    uint i, count = regions.count();

    // Convert the QValueList<DashboardRegionValue> into a NSDictionary of WebDashboardRegions
    NSMutableDictionary *webRegions = [[[NSMutableDictionary alloc] initWithCapacity:count] autorelease];
    for (i = 0; i < count; i++) {
        DashboardRegionValue region = regions[i];

        if (region.type == StyleDashboardRegion::None)
            continue;
            
        NSRect clip;
        clip.origin.x = region.clip.x();
        clip.origin.y = region.clip.y();
        clip.size.width = region.clip.width();
        clip.size.height = region.clip.height();
        NSRect rect;
        rect.origin.x = region.bounds.x();
        rect.origin.y = region.bounds.y();
        rect.size.width = region.bounds.width();
        rect.size.height = region.bounds.height();
        NSString *label = region.label.getNSString();
        WebDashboardRegionType type = WebDashboardRegionTypeNone;
        if (region.type == StyleDashboardRegion::Circle)
            type = WebDashboardRegionTypeCircle;
        else if (region.type == StyleDashboardRegion::Rectangle)
            type = WebDashboardRegionTypeRectangle;
        NSMutableArray *regionValues = [webRegions objectForKey:label];
        if (!regionValues) {
            regionValues = [NSMutableArray array];
            [webRegions setObject:regionValues forKey:label];
        }
        
        WebDashboardRegion *webRegion = [[[WebDashboardRegion alloc] initWithRect:rect clip:clip type:type] autorelease];
        [regionValues addObject:webRegion];
    }
    
    return webRegions;
}

void MacFrame::dashboardRegionsChanged()
{
    NSMutableDictionary *webRegions = dashboardRegionsDictionary();
    [_bridge dashboardRegionsChanged:webRegions];
}

bool MacFrame::isCharacterSmartReplaceExempt(const QChar &c, bool isPreviousChar)
{
    return [_bridge isCharacterSmartReplaceExempt:c.unicode() isPreviousCharacter:isPreviousChar];
}

void MacFrame::handledOnloadEvents()
{
    [_bridge handledOnloadEvents];
}

bool MacFrame::shouldClose()
{
    KWQ_BLOCK_EXCEPTIONS;

    if (![_bridge canRunBeforeUnloadConfirmPanel])
        return true;

    RefPtr<DocumentImpl> document = xmlDocImpl();
    if (!document)
        return true;
    HTMLElementImpl* body = document->body();
    if (!body)
        return true;

    RefPtr<BeforeUnloadEventImpl> event = new BeforeUnloadEventImpl;
    event->setTarget(document.get());
    int exception = 0;
    body->dispatchGenericEvent(event.get(), exception);
    if (!event->defaultPrevented() && document)
        document->defaultEventHandler(event.get());
    if (event->result().isNull())
        return true;

    QString text = event->result().qstring();
    text.replace(QChar('\\'), backslashAsCurrencySymbol());

    return [_bridge runBeforeUnloadConfirmPanelWithMessage:text.getNSString()];

    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
}

void MacFrame::dragSourceMovedTo(const IntPoint &loc)
{
    if (_dragSrc && _dragSrcMayBeDHTML) {
        // for now we don't care if event handler cancels default behavior, since there is none
        dispatchDragSrcEvent(dragEvent, loc);
    }
}

void MacFrame::dragSourceEndedAt(const IntPoint &loc, NSDragOperation operation)
{
    if (_dragSrc && _dragSrcMayBeDHTML) {
        _dragClipboard->setDestinationOperation(operation);
        // for now we don't care if event handler cancels default behavior, since there is none
        dispatchDragSrcEvent(dragendEvent, loc);
    }
    freeClipboard();
    _dragSrc = 0;
}

// returns if we should continue "default processing", i.e., whether eventhandler canceled
bool MacFrame::dispatchDragSrcEvent(const AtomicString &eventType, const IntPoint &loc) const
{
    bool noDefaultProc = d->m_view->dispatchDragEvent(eventType, _dragSrc.get(), loc, _dragClipboard);
    return !noDefaultProc;
}
