/*
 *  Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *  Copyright (C) 2010 Joone Hur <joone@kldp.org>
 *  Copyright (C) 2009 Google Inc. All rights reserved.
 *  Copyright (C) 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DumpRenderTreeSupportGtk.h"

#include "APICast.h"
#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "AnimationController.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "EditorClientGtk.h"
#include "Element.h"
#include "FocusController.h"
#include "FrameLoaderClientGtk.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GCController.h"
#include "GeolocationClientMock.h"
#include "GeolocationController.h"
#include "GeolocationError.h"
#include "GeolocationPosition.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "JSDOMWindow.h"
#include "JSDocument.h"
#include "JSElement.h"
#include "JSLock.h"
#include "JSNodeList.h"
#include "JSRange.h"
#include "JSValue.h"
#include "NodeList.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include "PrintContext.h"
#include "RenderListItem.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "TextIterator.h"
#include "WebKitAccessibleWrapperAtk.h"
#include "WebKitDOMRangePrivate.h"
#include "WorkerThread.h"
#include "webkitglobalsprivate.h"
#include "webkitwebframe.h"
#include "webkitwebframeprivate.h"
#include "webkitwebview.h"
#include "webkitwebviewprivate.h"
#include <JavaScriptCore/APICast.h>

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"
#include "SVGSMILElement.h"
#endif

using namespace JSC;
using namespace WebCore;
using namespace WebKit;

bool DumpRenderTreeSupportGtk::s_drtRun = false;
bool DumpRenderTreeSupportGtk::s_linksIncludedInTabChain = true;
bool DumpRenderTreeSupportGtk::s_selectTrailingWhitespaceEnabled = false;

DumpRenderTreeSupportGtk::DumpRenderTreeSupportGtk()
{
}

DumpRenderTreeSupportGtk::~DumpRenderTreeSupportGtk()
{
}

void DumpRenderTreeSupportGtk::setDumpRenderTreeModeEnabled(bool enabled)
{
    s_drtRun = enabled;
}

bool DumpRenderTreeSupportGtk::dumpRenderTreeModeEnabled()
{
    return s_drtRun;
}
void DumpRenderTreeSupportGtk::setLinksIncludedInFocusChain(bool enabled)
{
    s_linksIncludedInTabChain = enabled;
}

bool DumpRenderTreeSupportGtk::linksIncludedInFocusChain()
{
    return s_linksIncludedInTabChain;
}

void DumpRenderTreeSupportGtk::setSelectTrailingWhitespaceEnabled(bool enabled)
{
    s_selectTrailingWhitespaceEnabled = enabled;
}

bool DumpRenderTreeSupportGtk::selectTrailingWhitespaceEnabled()
{
    return s_selectTrailingWhitespaceEnabled;
}

JSValueRef DumpRenderTreeSupportGtk::nodesFromRect(JSContextRef context, JSValueRef value, int x, int y, unsigned top, unsigned right, unsigned bottom, unsigned left, bool ignoreClipping)
{
    JSLock lock(SilenceAssertionsOnly);
    ExecState* exec = toJS(context);
    if (!value)
        return JSValueMakeUndefined(context);
    JSValue jsValue = toJS(exec, value);
    if (!jsValue.inherits(&JSDocument::s_info))
       return JSValueMakeUndefined(context);

    JSDocument* jsDocument = static_cast<JSDocument*>(asObject(jsValue));
    Document* document = jsDocument->impl();
    RefPtr<NodeList> nodes = document->nodesFromRect(x, y, top, right, bottom, left, ignoreClipping);
    return toRef(exec, toJS(exec, jsDocument->globalObject(), nodes.get()));
}

WebKitDOMRange* DumpRenderTreeSupportGtk::jsValueToDOMRange(JSContextRef context, JSValueRef value)
{
    if (!value)
        return 0;

    JSLock lock(SilenceAssertionsOnly);
    ExecState* exec = toJS(context);

    Range* range = toRange(toJS(exec, value));
    if (!range)
        return 0;
    return kit(range);
}

/**
 * getFrameChildren:
 * @frame: a #WebKitWebFrame
 *
 * Return value: child frames of @frame
 */
GSList* DumpRenderTreeSupportGtk::getFrameChildren(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), 0);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return 0;

    GSList* children = 0;
    for (Frame* child = coreFrame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        FrameLoader* loader = child->loader();
        WebKit::FrameLoaderClient* client = static_cast<WebKit::FrameLoaderClient*>(loader->client());
        if (client)
          children = g_slist_append(children, client->webFrame());
    }

    return children;
}

/**
 * getInnerText:
 * @frame: a #WebKitWebFrame
 *
 * Return value: inner text of @frame
 */
CString DumpRenderTreeSupportGtk::getInnerText(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), CString(""));

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return CString("");

    FrameView* view = coreFrame->view();
    if (view && view->layoutPending())
        view->layout();

    Element* documentElement = coreFrame->document()->documentElement();
    if (!documentElement)
        return CString("");
    return documentElement->innerText().utf8();
}

/**
 * dumpRenderTree:
 * @frame: a #WebKitWebFrame
 *
 * Return value: Non-recursive render tree dump of @frame
 */
CString DumpRenderTreeSupportGtk::dumpRenderTree(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), CString(""));

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return CString("");

    FrameView* view = coreFrame->view();

    if (view && view->layoutPending())
        view->layout();

    return externalRepresentation(coreFrame).utf8();
}

/**
 * counterValueForElementById:
 * @frame: a #WebKitWebFrame
 * @id: an element ID string
 *
 * Return value: The counter value of element @id in @frame
 */
CString DumpRenderTreeSupportGtk::counterValueForElementById(WebKitWebFrame* frame, const char* id)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), CString());

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return CString();

    Element* coreElement = coreFrame->document()->getElementById(AtomicString(id));
    if (!coreElement)
        return CString();

    return counterValueForElement(coreElement).utf8();
}

/**
 * numberForElementById
 * @frame: a #WebKitWebFrame
 * @id: an element ID string
 * @pageWidth: width of a page
 * @pageHeight: height of a page
 *
 * Return value: The number of page where the specified element will be put
 */
int DumpRenderTreeSupportGtk::pageNumberForElementById(WebKitWebFrame* frame, const char* id, float pageWidth, float pageHeight)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), 0);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return -1;

    Element* coreElement = coreFrame->document()->getElementById(AtomicString(id));
    if (!coreElement)
        return -1;
    return PrintContext::pageNumberForElement(coreElement, FloatSize(pageWidth, pageHeight));
}

/**
 * numberOfPagesForFrame
 * @frame: a #WebKitWebFrame
 * @pageWidth: width of a page
 * @pageHeight: height of a page
 *
 * Return value: The number of pages to be printed.
 */
int DumpRenderTreeSupportGtk::numberOfPagesForFrame(WebKitWebFrame* frame, float pageWidth, float pageHeight)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), 0);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return -1;

    return PrintContext::numberOfPages(coreFrame, FloatSize(pageWidth, pageHeight));
}

/**
 * pageProperty
 * @frame: a #WebKitWebFrame
 * @propertyName: name of a property
 * @pageNumber: number of a page 
 *
 * Return value: The value of the given property name.
 */
CString DumpRenderTreeSupportGtk::pageProperty(WebKitWebFrame* frame, const char* propertyName, int pageNumber)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), CString());

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return CString();

    return PrintContext::pageProperty(coreFrame, propertyName, pageNumber).utf8();
}

/**
 * isPageBoxVisible
 * @frame: a #WebKitWebFrame
 * @pageNumber: number of a page 
 *
 * Return value: TRUE if a page box is visible. 
 */
bool DumpRenderTreeSupportGtk::isPageBoxVisible(WebKitWebFrame* frame, int pageNumber)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), false);

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return false;

    return coreFrame->document()->isPageBoxVisible(pageNumber); 
}

/**
 * pageSizeAndMarginsInPixels
 * @frame: a #WebKitWebFrame
 * @pageNumber: number of a page 
 * @width: width of a page
 * @height: height of a page
 * @marginTop: top margin of a page
 * @marginRight: right margin of a page
 * @marginBottom: bottom margin of a page
 * @marginLeft: left margin of a page
 *
 * Return value: The value of page size and margin.
 */
CString DumpRenderTreeSupportGtk::pageSizeAndMarginsInPixels(WebKitWebFrame* frame, int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), CString());

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return CString();

    return PrintContext::pageSizeAndMarginsInPixels(coreFrame, pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft).utf8();
}

/**
 * addUserStyleSheet
 * @frame: a #WebKitWebFrame
 * @sourceCode: code of a user stylesheet
 *
 */
void DumpRenderTreeSupportGtk::addUserStyleSheet(WebKitWebFrame* frame, const char* sourceCode, bool allFrames)
{
    g_return_if_fail(WEBKIT_IS_WEB_FRAME(frame));

    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    WebKitWebView* webView = getViewFromFrame(frame);
    Page* page = core(webView);
    page->group().addUserStyleSheetToWorld(mainThreadNormalWorld(), sourceCode, KURL(), nullptr, nullptr, allFrames ? InjectInAllFrames : InjectInTopFrameOnly); 
}

/**
 * getPendingUnloadEventCount:
 * @frame: a #WebKitWebFrame
 *
 * Return value: number of pending unload events
 */
guint DumpRenderTreeSupportGtk::getPendingUnloadEventCount(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), 0);

    return core(frame)->domWindow()->pendingUnloadEventListeners();
}

bool DumpRenderTreeSupportGtk::pauseAnimation(WebKitWebFrame* frame, const char* name, double time, const char* element)
{
    ASSERT(core(frame));
    Element* coreElement = core(frame)->document()->getElementById(AtomicString(element));
    if (!coreElement || !coreElement->renderer())
        return false;
    return core(frame)->animation()->pauseAnimationAtTime(coreElement->renderer(), AtomicString(name), time);
}

bool DumpRenderTreeSupportGtk::pauseTransition(WebKitWebFrame* frame, const char* name, double time, const char* element)
{
    ASSERT(core(frame));
    Element* coreElement = core(frame)->document()->getElementById(AtomicString(element));
    if (!coreElement || !coreElement->renderer())
        return false;
    return core(frame)->animation()->pauseTransitionAtTime(coreElement->renderer(), AtomicString(name), time);
}

bool DumpRenderTreeSupportGtk::pauseSVGAnimation(WebKitWebFrame* frame, const char* animationId, double time, const char* elementId)
{
    ASSERT(core(frame));
#if ENABLE(SVG)
    Document* document = core(frame)->document();
    if (!document || !document->svgExtensions())
        return false;
    Element* coreElement = document->getElementById(AtomicString(animationId));
    if (!coreElement || !SVGSMILElement::isSMILElement(coreElement))
        return false;
    return document->accessSVGExtensions()->sampleAnimationAtTime(elementId, static_cast<SVGSMILElement*>(coreElement), time);
#else
    return false;
#endif
}

CString DumpRenderTreeSupportGtk::markerTextForListItem(WebKitWebFrame* frame, JSContextRef context, JSValueRef nodeObject)
{
    JSC::ExecState* exec = toJS(context);
    Element* element = toElement(toJS(exec, nodeObject));
    if (!element)
        return CString();

    return WebCore::markerTextForListItem(element).utf8();
}

unsigned int DumpRenderTreeSupportGtk::numberOfActiveAnimations(WebKitWebFrame* frame)
{
    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return 0;

    return coreFrame->animation()->numberOfActiveAnimations(coreFrame->document());
}

void DumpRenderTreeSupportGtk::suspendAnimations(WebKitWebFrame* frame)
{
    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    return coreFrame->animation()->suspendAnimations();
}

void DumpRenderTreeSupportGtk::resumeAnimations(WebKitWebFrame* frame)
{
    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    return coreFrame->animation()->resumeAnimations();
}

void DumpRenderTreeSupportGtk::clearMainFrameName(WebKitWebFrame* frame)
{
    g_return_if_fail(WEBKIT_IS_WEB_FRAME(frame));

    core(frame)->tree()->clearName();
}

AtkObject* DumpRenderTreeSupportGtk::getRootAccessibleElement(WebKitWebFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), 0);

#if HAVE(ACCESSIBILITY)
    if (!AXObjectCache::accessibilityEnabled())
        AXObjectCache::enableAccessibility();

    WebKitWebFramePrivate* priv = frame->priv;
    if (!priv->coreFrame || !priv->coreFrame->document())
        return 0;

    AtkObject* wrapper =  priv->coreFrame->document()->axObjectCache()->rootObject()->wrapper();
    if (!wrapper)
        return 0;

    return wrapper;
#else
    return 0;
#endif
}

AtkObject* DumpRenderTreeSupportGtk::getFocusedAccessibleElement(WebKitWebFrame* frame)
{
#if HAVE(ACCESSIBILITY)
    AtkObject* wrapper = getRootAccessibleElement(frame);
    if (!wrapper)
        return 0;

    return webkit_accessible_get_focused_element(WEBKIT_ACCESSIBLE(wrapper));
#else
    return 0;
#endif
}

void DumpRenderTreeSupportGtk::executeCoreCommandByName(WebKitWebView* webView, const gchar* name, const gchar* value)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(name);
    g_return_if_fail(value);

    core(webView)->focusController()->focusedOrMainFrame()->editor()->command(name).execute(value);
}

bool DumpRenderTreeSupportGtk::isCommandEnabled(WebKitWebView* webView, const gchar* name)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(name, FALSE);

    return core(webView)->focusController()->focusedOrMainFrame()->editor()->command(name).isEnabled();
}

void DumpRenderTreeSupportGtk::setComposition(WebKitWebView* webView, const char* text, int start, int length)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(text);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    Editor* editor = frame->editor();
    if (!editor || (!editor->canEdit() && !editor->hasComposition()))
        return;

    String compositionString = String::fromUTF8(text);
    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, compositionString.length(), Color(0, 0, 0), false));
    editor->setComposition(compositionString, underlines, start, start + length);
}

bool DumpRenderTreeSupportGtk::hasComposition(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), false);
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame)
        return false;
    Editor* editor = frame->editor();
    if (!editor)
        return false;

    return editor->hasComposition();
}

bool DumpRenderTreeSupportGtk::compositionRange(WebKitWebView* webView, int* start, int* length)
{
    g_return_val_if_fail(start && length, false);
    *start = *length = 0;

    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), false);
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame)
        return false;

    Editor* editor = frame->editor();
    if (!editor || !editor->hasComposition())
        return false;

    *start = editor->compositionStart();
    *length = editor->compositionEnd() - *start;
    return true;
}

void DumpRenderTreeSupportGtk::confirmComposition(WebKitWebView* webView, const char* text)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    Editor* editor = frame->editor();
    if (!editor)
        return;

    if (!editor->hasComposition()) {
        editor->insertText(String::fromUTF8(text), 0);
        return;
    }
    if (text) {
        editor->confirmComposition(String::fromUTF8(text));
        return;
    }
    editor->confirmComposition();
}

void DumpRenderTreeSupportGtk::doCommand(WebKitWebView* webView, const char* command)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    Editor* editor = frame->editor();
    if (!editor)
        return;

    String commandString(command);
    // Remove ending : here.
    if (commandString.endsWith(":", true))
        commandString = commandString.left(commandString.length() - 1);

    // Make the first char in upper case.
    String firstChar = commandString.left(1);
    commandString = commandString.right(commandString.length() - 1);
    firstChar.makeUpper();
    commandString.insert(firstChar, 0);

    editor->command(commandString).execute();
}

bool DumpRenderTreeSupportGtk::firstRectForCharacterRange(WebKitWebView* webView, int location, int length, cairo_rectangle_int_t* rect)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), false);
    g_return_val_if_fail(rect, false);

    if ((location + length < location) && (location + length))
        length = 0;

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame)
        return false;

    Editor* editor = frame->editor();
    if (!editor)
        return false;

    RefPtr<Range> range = TextIterator::rangeFromLocationAndLength(frame->selection()->rootEditableElementOrDocumentElement(), location, length);
    if (!range)
        return false;

    *rect = editor->firstRectForRange(range.get());
    return true;
}

bool DumpRenderTreeSupportGtk::selectedRange(WebKitWebView* webView, int* start, int* length)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), false);
    g_return_val_if_fail(start && length, false);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame)
        return false;

    RefPtr<Range> range = frame->selection()->toNormalizedRange().get();
    if (!range)
        return false;

    Element* selectionRoot = frame->selection()->rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : frame->document()->documentElement();

    RefPtr<Range> testRange = Range::create(scope->document(), scope, 0, range->startContainer(), range->startOffset());
    ASSERT(testRange->startContainer() == scope);
    *start = TextIterator::rangeLength(testRange.get());

    ExceptionCode ec;
    testRange->setEnd(range->endContainer(), range->endOffset(), ec);
    ASSERT(testRange->startContainer() == scope);
    *length = TextIterator::rangeLength(testRange.get());

    return true;
}

void DumpRenderTreeSupportGtk::setSmartInsertDeleteEnabled(WebKitWebView* webView, bool enabled)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(webView);

    WebKit::EditorClient* client = static_cast<WebKit::EditorClient*>(core(webView)->editorClient());
    client->setSmartInsertDeleteEnabled(enabled);
}

void DumpRenderTreeSupportGtk::whiteListAccessFromOrigin(const gchar* sourceOrigin, const gchar* destinationProtocol, const gchar* destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessWhitelistEntry(*SecurityOrigin::createFromString(sourceOrigin), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void DumpRenderTreeSupportGtk::resetOriginAccessWhiteLists()
{
    SecurityPolicy::resetOriginAccessWhitelists();
}

void DumpRenderTreeSupportGtk::gcCollectJavascriptObjects()
{
    gcController().garbageCollectNow();
}

void DumpRenderTreeSupportGtk::gcCollectJavascriptObjectsOnAlternateThread(bool waitUntilDone)
{
    gcController().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
}

unsigned long DumpRenderTreeSupportGtk::gcCountJavascriptObjects()
{
    JSC::JSLock lock(JSC::SilenceAssertionsOnly);
    return JSDOMWindow::commonJSGlobalData()->heap.objectCount();
}

void DumpRenderTreeSupportGtk::layoutFrame(WebKitWebFrame* frame)
{
    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    FrameView* view = coreFrame->view();
    if (!view)
        return;

    view->layout();
}

// For testing fast/viewport.
void DumpRenderTreeSupportGtk::dumpConfigurationForViewport(WebKitWebView* webView, gint deviceDPI, gint deviceWidth, gint deviceHeight, gint availableWidth, gint availableHeight)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    ViewportArguments arguments = webView->priv->corePage->mainFrame()->document()->viewportArguments();
    ViewportAttributes attrs = computeViewportAttributes(arguments, /* default layout width for non-mobile pages */ 980, deviceWidth, deviceHeight, deviceDPI, IntSize(availableWidth, availableHeight));
    restrictMinimumScaleFactorToViewportSize(attrs, IntSize(availableWidth, availableHeight));
    restrictScaleFactorToInitialScaleIfNotUserScalable(attrs);
    fprintf(stdout, "viewport size %dx%d scale %f with limits [%f, %f] and userScalable %f\n", attrs.layoutSize.width(), attrs.layoutSize.height(), attrs.initialScale, attrs.minimumScale, attrs.maximumScale, attrs.userScalable);
}

void DumpRenderTreeSupportGtk::clearOpener(WebKitWebFrame* frame)
{
    Frame* coreFrame = core(frame);
    if (coreFrame)
        coreFrame->loader()->setOpener(0);
}

unsigned int DumpRenderTreeSupportGtk::workerThreadCount()
{
#if ENABLE(WORKERS)
    return WebCore::WorkerThread::workerThreadCount();
#else
    return 0;
#endif
}

bool DumpRenderTreeSupportGtk::webkitWebFrameSelectionHasSpellingMarker(WebKitWebFrame *frame, gint from, gint length)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_FRAME(frame), FALSE);

    return core(frame)->editor()->selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}

bool DumpRenderTreeSupportGtk::findString(WebKitWebView* webView, const gchar* targetString, WebKitFindOptions findOptions)
{
    return core(webView)->findString(String::fromUTF8(targetString), findOptions);
}

double DumpRenderTreeSupportGtk::defaultMinimumTimerInterval()
{
    return Settings::defaultMinDOMTimerInterval();
}

void DumpRenderTreeSupportGtk::setMinimumTimerInterval(WebKitWebView* webView, double interval)
{
    core(webView)->settings()->setMinDOMTimerInterval(interval);
}

static void modifyAccessibilityValue(AtkObject* axObject, bool increment)
{
    if (!axObject || !WEBKIT_IS_ACCESSIBLE(axObject))
        return;

    AccessibilityObject* coreObject = webkit_accessible_get_accessibility_object(WEBKIT_ACCESSIBLE(axObject));
    if (!coreObject)
        return;

    if (increment)
        coreObject->increment();
    else
        coreObject->decrement();
}

void DumpRenderTreeSupportGtk::incrementAccessibilityValue(AtkObject* axObject)
{
    modifyAccessibilityValue(axObject, true);
}

void DumpRenderTreeSupportGtk::decrementAccessibilityValue(AtkObject* axObject)
{
    modifyAccessibilityValue(axObject, false);
}

CString DumpRenderTreeSupportGtk::accessibilityHelpText(AtkObject* axObject)
{
    if (!axObject || !WEBKIT_IS_ACCESSIBLE(axObject))
        return CString();

    AccessibilityObject* coreObject = webkit_accessible_get_accessibility_object(WEBKIT_ACCESSIBLE(axObject));
    if (!coreObject)
        return CString();

    return coreObject->helpText().utf8();
}

void DumpRenderTreeSupportGtk::setAutofilled(JSContextRef context, JSValueRef nodeObject, bool autofilled)
{
    JSC::ExecState* exec = toJS(context);
    Element* element = toElement(toJS(exec, nodeObject));
    if (!element)
        return;
    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement)
        return;

    inputElement->setAutofilled(autofilled);
}

void DumpRenderTreeSupportGtk::setValueForUser(JSContextRef context, JSValueRef nodeObject, JSStringRef value)
{
    JSC::ExecState* exec = toJS(context);
    Element* element = toElement(toJS(exec, nodeObject));
    if (!element)
        return;
    HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement)
        return;

    size_t bufferSize = JSStringGetMaximumUTF8CStringSize(value);
    GOwnPtr<gchar> valueBuffer(static_cast<gchar*>(g_malloc(bufferSize)));
    JSStringGetUTF8CString(value, valueBuffer.get(), bufferSize);
    inputElement->setValueForUser(String::fromUTF8(valueBuffer.get()));
}

void DumpRenderTreeSupportGtk::rectangleForSelection(WebKitWebFrame* frame, cairo_rectangle_int_t* rectangle)
{
    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return;

    IntRect bounds = enclosingIntRect(coreFrame->selection()->bounds());
    rectangle->x = bounds.x();
    rectangle->y = bounds.y();
    rectangle->width = bounds.width();
    rectangle->height = bounds.height();
}

bool DumpRenderTreeSupportGtk::shouldClose(WebKitWebFrame* frame)
{
    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return true;
    return coreFrame->loader()->shouldClose();
}

void DumpRenderTreeSupportGtk::scalePageBy(WebKitWebView* webView, float scaleFactor, float x, float y)
{
    core(webView)->setPageScaleFactor(scaleFactor, IntPoint(x, y));
}

void DumpRenderTreeSupportGtk::resetGeolocationClientMock(WebKitWebView* webView)
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    GeolocationClientMock* mock = static_cast<GeolocationClientMock*>(core(webView)->geolocationController()->client());
    mock->reset();
#endif
}

void DumpRenderTreeSupportGtk::setMockGeolocationPermission(WebKitWebView* webView, bool allowed)
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    GeolocationClientMock* mock = static_cast<GeolocationClientMock*>(core(webView)->geolocationController()->client());
    mock->setPermission(allowed);
#endif
}

void DumpRenderTreeSupportGtk::setMockGeolocationPosition(WebKitWebView* webView, double latitude, double longitude, double accuracy)
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    GeolocationClientMock* mock = static_cast<GeolocationClientMock*>(core(webView)->geolocationController()->client());

    double timestamp = g_get_real_time() / 1000000.0;
    mock->setPosition(GeolocationPosition::create(timestamp, latitude, longitude, accuracy));
#endif
}

void DumpRenderTreeSupportGtk::setMockGeolocationError(WebKitWebView* webView, int errorCode, const gchar* errorMessage)
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    GeolocationClientMock* mock = static_cast<GeolocationClientMock*>(core(webView)->geolocationController()->client());

    GeolocationError::ErrorCode code;
    switch (errorCode) {
    case PositionError::PERMISSION_DENIED:
        code = GeolocationError::PermissionDenied;
        break;
    case PositionError::POSITION_UNAVAILABLE:
    default:
        code = GeolocationError::PositionUnavailable;
        break;
    }

    mock->setError(GeolocationError::create(code, errorMessage));
#endif
}

int DumpRenderTreeSupportGtk::numberOfPendingGeolocationPermissionRequests(WebKitWebView* webView)
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    GeolocationClientMock* mock = static_cast<GeolocationClientMock*>(core(webView)->geolocationController()->client());
    return mock->numberOfPendingPermissionRequests();
#else
    return 0;
#endif
}

void DumpRenderTreeSupportGtk::setHixie76WebSocketProtocolEnabled(WebKitWebView* webView, bool enabled)
{
#if ENABLE(WEB_SOCKETS)
    core(webView)->settings()->setUseHixie76WebSocketProtocol(enabled);
#else
    UNUSED_PARAM(webView);
    UNUSED_PARAM(enabled);
#endif
}
