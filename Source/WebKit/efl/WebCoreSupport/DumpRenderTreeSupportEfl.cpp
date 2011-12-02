/*
    Copyright (C) 2011 ProFUSION embedded systems
    Copyright (C) 2011 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "DumpRenderTreeSupportEfl.h"

#include "FrameLoaderClientEfl.h"
#include "ewk_private.h"

#include <AnimationController.h>
#include <DocumentLoader.h>
#include <Eina.h>
#include <Evas.h>
#include <FindOptions.h>
#include <FloatSize.h>
#include <FrameView.h>
#include <IntRect.h>
#include <PrintContext.h>
#include <RenderTreeAsText.h>
#include <Settings.h>
#include <bindings/js/GCController.h>
#include <history/HistoryItem.h>
#include <workers/WorkerThread.h>
#include <wtf/text/AtomicString.h>

#if ENABLE(SVG)
#include <SVGDocumentExtensions.h>
#include <SVGSMILElement.h>
#endif

unsigned DumpRenderTreeSupportEfl::activeAnimationsCount(const Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return 0;

    WebCore::AnimationController* animationController = frame->animation();

    if (!animationController)
        return 0;

    return animationController->numberOfActiveAnimations(frame->document());
}

void DumpRenderTreeSupportEfl::clearFrameName(Evas_Object* ewkFrame)
{
    if (WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame))
        frame->tree()->clearName();
}

void DumpRenderTreeSupportEfl::clearOpener(Evas_Object* ewkFrame)
{
    if (WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame))
        frame->loader()->setOpener(0);
}

String DumpRenderTreeSupportEfl::counterValueByElementId(const Evas_Object* ewkFrame, const char* elementId)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return String();

    WebCore::Element* element = frame->document()->getElementById(elementId);

    if (!element)
        return String();

    return WebCore::counterValueForElement(element);
}

Eina_List* DumpRenderTreeSupportEfl::frameChildren(const Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return 0;

    Eina_List* childFrames = 0;

    for (unsigned index = 0; index < frame->tree()->childCount(); index++) {
        WebCore::Frame *childFrame = frame->tree()->child(index);
        WebCore::FrameLoaderClientEfl *client = static_cast<WebCore::FrameLoaderClientEfl*>(childFrame->loader()->client());

        if (!client)
            continue;

        childFrames = eina_list_append(childFrames, client->webFrame());
    }

    return childFrames;
}

WebCore::Frame* DumpRenderTreeSupportEfl::frameParent(const Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return 0;

    return frame->tree()->parent();
}

void DumpRenderTreeSupportEfl::layoutFrame(Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return;

    WebCore::FrameView* frameView = frame->view();

    if (!frameView)
        return;

    frameView->layout();
}

int DumpRenderTreeSupportEfl::numberOfPages(const Evas_Object* ewkFrame, float pageWidth, float pageHeight)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return 0;

    return WebCore::PrintContext::numberOfPages(frame, WebCore::FloatSize(pageWidth, pageHeight));
}

int DumpRenderTreeSupportEfl::numberOfPagesForElementId(const Evas_Object* ewkFrame, const char* elementId, float pageWidth, float pageHeight)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return 0;

    WebCore::Element *element = frame->document()->getElementById(elementId);

    if (!element)
        return 0;

    return WebCore::PrintContext::pageNumberForElement(element, WebCore::FloatSize(pageWidth, pageHeight));
}

bool DumpRenderTreeSupportEfl::pauseAnimation(Evas_Object* ewkFrame, const char* name, const char* elementId, double time)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return false;

    WebCore::Element* element = frame->document()->getElementById(elementId);

    if (!element || !element->renderer())
        return false;

    return frame->animation()->pauseAnimationAtTime(element->renderer(), name, time);
}

bool DumpRenderTreeSupportEfl::pauseSVGAnimation(Evas_Object* ewkFrame, const char* animationId, const char* elementId, double time)
{
#if ENABLE(SVG)
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return false;

    WebCore::Document* document = frame->document();

    if (!document || !document->svgExtensions())
        return false;

    WebCore::Element* element = document->getElementById(animationId);

    if (!element || !WebCore::SVGSMILElement::isSMILElement(element))
        return false;

    return document->accessSVGExtensions()->sampleAnimationAtTime(elementId, static_cast<WebCore::SVGSMILElement*>(element), time);
#else
    return false;
#endif
}

bool DumpRenderTreeSupportEfl::pauseTransition(Evas_Object* ewkFrame, const char* name, const char* elementId, double time)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return false;

    WebCore::Element* element = frame->document()->getElementById(elementId);

    if (!element || !element->renderer())
        return false;

    return frame->animation()->pauseTransitionAtTime(element->renderer(), name, time);
}

unsigned DumpRenderTreeSupportEfl::pendingUnloadEventCount(const Evas_Object* ewkFrame)
{
    if (WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame))
        return frame->domWindow()->pendingUnloadEventListeners();

    return 0;
}

String DumpRenderTreeSupportEfl::renderTreeDump(Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return String();

    WebCore::FrameView *frameView = frame->view();

    if (frameView && frameView->layoutPending())
        frameView->layout();

    return WebCore::externalRepresentation(frame);
}

String DumpRenderTreeSupportEfl::responseMimeType(const Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return String();

    WebCore::DocumentLoader *documentLoader = frame->loader()->documentLoader();

    if (!documentLoader)
        return String();

    return documentLoader->responseMIMEType();
}

void DumpRenderTreeSupportEfl::resumeAnimations(Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return;

    WebCore::AnimationController *animationController = frame->animation();
    if (animationController)
        animationController->resumeAnimations();
}

WebCore::IntRect DumpRenderTreeSupportEfl::selectionRectangle(const Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return WebCore::IntRect();

    return enclosingIntRect(frame->selection()->bounds());
}

// Compare with "WebKit/Tools/DumpRenderTree/mac/FrameLoadDelegate.mm
String DumpRenderTreeSupportEfl::suitableDRTFrameName(const Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return String();

    const String frameName(ewk_frame_name_get(ewkFrame));

    if (ewkFrame == ewk_view_frame_main_get(ewk_frame_view_get(ewkFrame))) {
        if (!frameName.isEmpty())
            return String("main frame \"") + frameName + String("\"");

        return String("main frame");
    }

    if (!frameName.isEmpty())
        return String("frame \"") + frameName + String("\"");

    return String("frame (anonymous)");
}

void DumpRenderTreeSupportEfl::suspendAnimations(Evas_Object* ewkFrame)
{
    WebCore::Frame* frame = EWKPrivate::coreFrame(ewkFrame);

    if (!frame)
        return;

    WebCore::AnimationController *animationController = frame->animation();
    if (animationController)
        animationController->suspendAnimations();
}

bool DumpRenderTreeSupportEfl::findString(const Evas_Object* ewkView, const char* text, WebCore::FindOptions options)
{
    WebCore::Page* page = EWKPrivate::corePage(ewkView);

    if (!page)
        return false;

    return page->findString(String::fromUTF8(text), options);
}

void DumpRenderTreeSupportEfl::garbageCollectorCollect()
{
    WebCore::gcController().garbageCollectNow();
}

void DumpRenderTreeSupportEfl::garbageCollectorCollectOnAlternateThread(bool waitUntilDone)
{
    WebCore::gcController().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
}

size_t DumpRenderTreeSupportEfl::javaScriptObjectsCount()
{
    return WebCore::JSDOMWindow::commonJSGlobalData()->heap.objectCount();
}

unsigned DumpRenderTreeSupportEfl::workerThreadCount()
{
#if ENABLE(WORKERS)
    return WebCore::WorkerThread::workerThreadCount();
#else
    return 0;
#endif
}

HistoryItemChildrenVector DumpRenderTreeSupportEfl::childHistoryItems(const Ewk_History_Item* ewkHistoryItem)
{
    WebCore::HistoryItem* historyItem = EWKPrivate::coreHistoryItem(ewkHistoryItem);
    HistoryItemChildrenVector kids;

    if (!historyItem)
        return kids;

    const WebCore::HistoryItemVector& children = historyItem->children();
    const unsigned size = children.size();

    for (unsigned i = 0; i < size; ++i) {
        Ewk_History_Item* kid = ewk_history_item_new_from_core(children[i].get());
        kids.append(kid);
    }

    return kids;
}

String DumpRenderTreeSupportEfl::historyItemTarget(const Ewk_History_Item* ewkHistoryItem)
{
    WebCore::HistoryItem* historyItem = EWKPrivate::coreHistoryItem(ewkHistoryItem);

    if (!historyItem)
        return String();

    return historyItem->target();
}

bool DumpRenderTreeSupportEfl::isTargetItem(const Ewk_History_Item* ewkHistoryItem)
{
    WebCore::HistoryItem* historyItem = EWKPrivate::coreHistoryItem(ewkHistoryItem);

    if (!historyItem)
        return false;

    return historyItem->isTargetItem();
}

void DumpRenderTreeSupportEfl::setMockScrollbarsEnabled(bool enable)
{
    WebCore::Settings::setMockScrollbarsEnabled(enable);
}
