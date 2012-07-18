/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ewk_context.h"

#include "BatteryProvider.h"
#include "WKAPICast.h"
#include "WKRetainPtr.h"
#include "ewk_context_private.h"
#include "ewk_cookie_manager_private.h"

using namespace WebKit;

struct _Ewk_Context {
    WKRetainPtr<WKContextRef> context;

    Ewk_Cookie_Manager* cookieManager;
#if ENABLE(BATTERY_STATUS)
    RefPtr<BatteryProvider> batteryProvider;
#endif

    _Ewk_Context(WKContextRef contextRef)
        : context(contextRef)
        , cookieManager(0)
    { }

    ~_Ewk_Context()
    {
        if (cookieManager)
            ewk_cookie_manager_free(cookieManager);
    }
};

Ewk_Cookie_Manager* ewk_context_cookie_manager_get(const Ewk_Context* ewkContext)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkContext, 0);

    if (!ewkContext->cookieManager)
        const_cast<Ewk_Context*>(ewkContext)->cookieManager = ewk_cookie_manager_new(WKContextGetCookieManager(ewkContext->context.get()));

    return ewkContext->cookieManager;
}

WKContextRef ewk_context_WKContext_get(const Ewk_Context* ewkContext)
{
    return ewkContext->context.get();
}

static inline Ewk_Context* createDefaultEwkContext()
{
    WKContextRef wkContext = WKContextGetSharedProcessContext();
    Ewk_Context* ewkContext = new Ewk_Context(wkContext);

#if ENABLE(BATTERY_STATUS)
    WKBatteryManagerRef wkBatteryManager = WKContextGetBatteryManager(wkContext);
    ewkContext->batteryProvider = BatteryProvider::create(wkBatteryManager);
#endif

    return ewkContext;
}

Ewk_Context* ewk_context_default_get()
{
    static Ewk_Context* defaultContext = createDefaultEwkContext();

    return defaultContext;
}
