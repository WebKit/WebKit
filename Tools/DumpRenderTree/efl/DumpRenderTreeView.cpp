/*
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2011 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Red istributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DumpRenderTreeView.h"

#include "DumpRenderTree.h"
#include "DumpRenderTreeChrome.h"
#include "LayoutTestController.h"
#include <EWebKit.h>
#include <Ecore.h>
#include <Eina.h>
#include <Evas.h>
#include <cstdio>
#include <cstdlib>

using namespace std;

static Ewk_View_Smart_Class gParentSmartClass = EWK_VIEW_SMART_CLASS_INIT_NULL;

static void onConsoleMessage(Ewk_View_Smart_Data*, const char* message, unsigned int lineNumber, const char*)
{
    printf("CONSOLE MESSAGE: ");
    if (lineNumber)
        printf("line %u: ", lineNumber);
    printf("%s\n", message);
}

static void onJavaScriptAlert(Ewk_View_Smart_Data*, Evas_Object*, const char* message)
{
    printf("ALERT: %s\n", message);
}

static Eina_Bool onJavaScriptConfirm(Ewk_View_Smart_Data*, Evas_Object*, const char* message)
{
    printf("CONFIRM: %s\n", message);
    return EINA_TRUE;
}

static Eina_Bool onJavaScriptPrompt(Ewk_View_Smart_Data*, Evas_Object*, const char* message, const char* defaultValue, char** value)
{
    printf("PROMPT: %s, default text: %s\n", message, defaultValue);
    *value = strdup(defaultValue);
    return EINA_TRUE;
}

static Evas_Object* onWindowCreate(Ewk_View_Smart_Data*, Eina_Bool, const Ewk_Window_Features*)
{
    return gLayoutTestController->canOpenWindows() ? browser->createNewWindow() : 0;
}

static Eina_Bool onWindowCloseDelayed(void* data)
{
    Evas_Object* view = static_cast<Evas_Object*>(data);
    browser->removeWindow(view);
    return EINA_FALSE;
}

static void onWindowClose(Ewk_View_Smart_Data* smartData)
{
    Evas_Object* view = smartData->self;
    ecore_idler_add(onWindowCloseDelayed, view);
}

static bool shouldUseSingleBackingStore()
{
    const char* useSingleBackingStore = getenv("DRT_USE_SINGLE_BACKING_STORE");
    return useSingleBackingStore && *useSingleBackingStore == '1';
}

static bool chooseAndInitializeAppropriateSmartClass(Ewk_View_Smart_Class* api)
{
    return shouldUseSingleBackingStore() ? ewk_view_single_smart_set(api) : ewk_view_tiled_smart_set(api);
}

Evas_Object* drtViewAdd(Evas* evas)
{
    static Ewk_View_Smart_Class api = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("DRT_View");

    if (!chooseAndInitializeAppropriateSmartClass(&api))
        return 0;

    if (EINA_UNLIKELY(!gParentSmartClass.sc.add))
        ewk_view_base_smart_set(&gParentSmartClass);

    api.add_console_message = onConsoleMessage;
    api.run_javascript_alert = onJavaScriptAlert;
    api.run_javascript_confirm = onJavaScriptConfirm;
    api.run_javascript_prompt = onJavaScriptPrompt;
    api.window_create = onWindowCreate;
    api.window_close = onWindowClose;

    return evas_object_smart_add(evas, evas_smart_class_new(&api.sc));
}
