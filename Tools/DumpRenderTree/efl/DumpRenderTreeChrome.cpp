/*
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2011 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND ITS CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DumpRenderTreeChrome.h"

#include "DumpRenderTree.h"
#include "DumpRenderTreeView.h"
#include "EventSender.h"
#include "GCController.h"
#include "LayoutTestController.h"
#include "NotImplemented.h"
#include "WebCoreSupport/DumpRenderTreeSupportEfl.h"
#include "WorkQueue.h"
#include "ewk_private.h" // FIXME: create some WebCoreSupport/DumpRenderTree.cpp instead

#include <EWebKit.h>
#include <Ecore.h>
#include <Eina.h>
#include <Evas.h>
#include <cstdio>
#include <wtf/NotFound.h>

PassOwnPtr<DumpRenderTreeChrome> DumpRenderTreeChrome::create(Evas* evas)
{
    OwnPtr<DumpRenderTreeChrome> chrome = adoptPtr(new DumpRenderTreeChrome(evas));

    if (!chrome->initialize())
        return nullptr;

    return chrome.release();
}

DumpRenderTreeChrome::DumpRenderTreeChrome(Evas* evas)
    : m_mainView(0)
    , m_mainFrame(0)
    , m_evas(evas)
    , m_gcController(adoptPtr(new GCController))
{
}

DumpRenderTreeChrome::~DumpRenderTreeChrome()
{
}

Evas_Object* DumpRenderTreeChrome::createNewWindow()
{
    Evas_Object* newView = createView();

    ewk_view_setting_scripts_can_open_windows_set(newView, EINA_TRUE);
    ewk_view_setting_scripts_can_close_windows_set(newView, EINA_TRUE);

    m_extraViews.append(newView);

    return newView;
}

Evas_Object* DumpRenderTreeChrome::createView() const
{
    Evas_Object* view = drtViewAdd(m_evas);
    if (!view)
        return 0;

    ewk_view_theme_set(view, DATA_DIR"/default.edj");

    evas_object_smart_callback_add(view, "load,started", onLoadStarted, 0);
    evas_object_smart_callback_add(view, "load,finished", onLoadFinished, 0);
    evas_object_smart_callback_add(view, "title,changed", onTitleChanged, 0);
    evas_object_smart_callback_add(view, "window,object,cleared", onWindowObjectCleared, m_gcController.get());
    evas_object_smart_callback_add(view, "statusbar,text,set", onStatusbarTextSet, 0);
    evas_object_smart_callback_add(view, "load,document,finished", onDocumentLoadFinished, 0);

    return view;
}

void DumpRenderTreeChrome::removeWindow(Evas_Object* view)
{
    const size_t pos = m_extraViews.find(view);

    if (pos == notFound)
        return;

    m_extraViews.remove(pos);
    evas_object_del(view);
}

bool DumpRenderTreeChrome::initialize()
{
    DumpRenderTreeSupportEfl::setMockScrollbarsEnabled(true);

    m_mainView = createView();
    if (!m_mainView)
        return false;

    ewk_view_theme_set(m_mainView, DATA_DIR"/default.edj");

    evas_object_name_set(m_mainView, "m_mainView");
    evas_object_move(m_mainView, 0, 0);
    evas_object_resize(m_mainView, 800, 600);
    evas_object_layer_set(m_mainView, EVAS_LAYER_MAX);
    evas_object_show(m_mainView);
    evas_object_focus_set(m_mainView, EINA_TRUE);

    m_mainFrame = ewk_view_frame_main_get(m_mainView);

    return true;
}

Vector<Evas_Object*> DumpRenderTreeChrome::extraViews() const
{
    return m_extraViews;
}

Evas_Object* DumpRenderTreeChrome::mainFrame() const
{
    return m_mainFrame;
}

Evas_Object* DumpRenderTreeChrome::mainView() const
{
    return m_mainView;
}

void DumpRenderTreeChrome::resetDefaultsToConsistentValues()
{
    Vector<Evas_Object*>::iterator it = m_extraViews.begin();
    for (; it != m_extraViews.end(); ++it)
        evas_object_del(*it);
    m_extraViews.clear();

    ewk_settings_icon_database_clear();
    ewk_settings_icon_database_path_set(0);

    ewk_view_setting_private_browsing_set(mainView(), EINA_FALSE);
    ewk_view_setting_spatial_navigation_set(mainView(), EINA_FALSE);
    ewk_view_setting_enable_frame_flattening_set(mainView(), EINA_FALSE);
    ewk_view_setting_application_cache_set(mainView(), EINA_TRUE);
    ewk_view_setting_enable_scripts_set(mainView(), EINA_TRUE);
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_STANDARD, "Times");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_MONOSPACE, "Courier");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_SERIF, "Times");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_SANS_SERIF, "Helvetica");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_CURSIVE, "cursive");
    ewk_view_font_family_name_set(mainView(), EWK_FONT_FAMILY_FANTASY, "fantasy");
    ewk_view_setting_font_default_size_set(mainView(), 16);
    ewk_view_setting_font_monospace_size_set(mainView(), 13);
    ewk_view_setting_font_minimum_size_set(mainView(), 0);
    ewk_view_setting_caret_browsing_set(mainView(), EINA_FALSE);
    ewk_view_setting_page_cache_set(mainView(), EINA_FALSE);
    ewk_view_setting_enable_auto_resize_window_set(mainView(), EINA_TRUE);
    ewk_view_setting_enable_plugins_set(mainView(), EINA_TRUE);
    ewk_view_setting_scripts_can_open_windows_set(mainView(), EINA_TRUE);
    ewk_view_setting_scripts_can_close_windows_set(mainView(), EINA_TRUE);

    ewk_view_zoom_set(mainView(), 1.0, 0, 0);
    ewk_view_scale_set(mainView(), 1.0, 0, 0);

    ewk_history_clear(ewk_view_history_get(mainView()));

    ewk_cookies_clear();
    ewk_cookies_policy_set(EWK_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);

    DumpRenderTreeSupportEfl::clearFrameName(mainFrame());
    DumpRenderTreeSupportEfl::clearOpener(mainFrame());
}

// Smart Callbacks
// ---------------

void DumpRenderTreeChrome::onWindowObjectCleared(void* userData, Evas_Object*, void* eventInfo)
{
    Ewk_Window_Object_Cleared_Event* objectClearedInfo = static_cast<Ewk_Window_Object_Cleared_Event*>(eventInfo);
    JSValueRef exception = 0;
    ASSERT(gLayoutTestController);

    GCController* gcController = static_cast<GCController*>(userData);
    ASSERT(gcController);

    gLayoutTestController->makeWindowObject(objectClearedInfo->context, objectClearedInfo->windowObject, &exception);
    ASSERT(!exception);

    gcController->makeWindowObject(objectClearedInfo->context, objectClearedInfo->windowObject, &exception);
    ASSERT(!exception);

    JSRetainPtr<JSStringRef> controllerName(JSStringCreateWithUTF8CString("eventSender"));
    JSObjectSetProperty(objectClearedInfo->context, objectClearedInfo->windowObject,
                        controllerName.get(),
                        makeEventSender(objectClearedInfo->context, !DumpRenderTreeSupportEfl::frameParent(objectClearedInfo->frame)),
                        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, 0);
}

void DumpRenderTreeChrome::onLoadStarted(void*, Evas_Object* view, void*)
{
    // FIXME: we actually need the frame related to this event
    Evas_Object* frame = ewk_view_frame_main_get(view);

    // Make sure we only set this once per test. If it gets cleared, and then set again, we might
    // end up doing two dumps for one test.
    if (!topLoadingFrame && !done)
        topLoadingFrame = frame;
}

Eina_Bool DumpRenderTreeChrome::processWork(void* data)
{
    Evas_Object* frame = static_cast<Evas_Object*>(data);

    if (WorkQueue::shared()->processWork() && !gLayoutTestController->waitToDump())
        dump();

    return ECORE_CALLBACK_CANCEL;
}

void DumpRenderTreeChrome::onLoadFinished(void*, Evas_Object* view, void*)
{
    // FIXME: we actually need the frame related to this event
    Evas_Object* frame = ewk_view_frame_main_get(view);

    if (topLoadingFrame != frame)
        return;

    topLoadingFrame = 0;

    WorkQueue::shared()->setFrozen(true);
    if (gLayoutTestController->waitToDump())
        return;

    if (WorkQueue::shared()->count())
        ecore_idler_add(processWork, frame);
    else
        dump();
}

void DumpRenderTreeChrome::onStatusbarTextSet(void*, Evas_Object*, void* eventInfo)
{
    if (!gLayoutTestController->dumpStatusCallbacks())
        return;

    const char* statusbarText = static_cast<const char*>(eventInfo);
    printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n", statusbarText);
}

void DumpRenderTreeChrome::onTitleChanged(void*, Evas_Object*, void*)
{
    notImplemented();
}

void DumpRenderTreeChrome::onDocumentLoadFinished(void*, Evas_Object*, void* eventInfo)
{
    const Evas_Object* frame = static_cast<Evas_Object*>(eventInfo);
    const String frameName(DumpRenderTreeSupportEfl::suitableDRTFrameName(frame));

    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didFinishDocumentLoadForFrame\n", frameName.utf8().data());
    else if (!done) {
        const unsigned pendingFrameUnloadEvents = DumpRenderTreeSupportEfl::pendingUnloadEventCount(frame);
        if (pendingFrameUnloadEvents)
            printf("%s - has %u onunload handler(s)\n", frameName.utf8().data(), pendingFrameUnloadEvents);
    }
}
