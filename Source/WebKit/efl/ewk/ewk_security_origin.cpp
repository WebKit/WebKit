/*
    Copyright (C) 2012 Intel Corporation

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
#include "ewk_security_origin.h"

#include "DatabaseTracker.h"
#include "SecurityOrigin.h"
#include "ewk_private.h"
#include <Eina.h>
#include <wtf/RefPtr.h>
#include <wtf/text/CString.h>

struct _Ewk_Security_Origin {
    RefPtr<WebCore::SecurityOrigin> securityOrigin;
    const char* protocol;
    const char* host;
};

const char* ewk_security_origin_protocol_get(Ewk_Security_Origin* origin)
{
    if (!origin->protocol)
        origin->protocol = eina_stringshare_add(origin->securityOrigin->protocol().utf8().data());

    return origin->protocol;
}

const char* ewk_security_origin_host_get(Ewk_Security_Origin* origin)
{
    if (!origin->host)
        origin->host = eina_stringshare_add(origin->securityOrigin->host().utf8().data());

    return origin->host;
}

uint32_t ewk_security_origin_port_get(const Ewk_Security_Origin* origin)
{
    return origin->securityOrigin->port();
}

uint64_t ewk_security_origin_web_database_usage_get(const Ewk_Security_Origin* origin)
{
#if ENABLE(SQL_DATABASE)
    return WebCore::DatabaseTracker::tracker().usageForOrigin(origin->securityOrigin.get());
#else
    return 0;
#endif
}

uint64_t ewk_security_origin_web_database_quota_get(const Ewk_Security_Origin* origin)
{
#if ENABLE(SQL_DATABASE)
    return WebCore::DatabaseTracker::tracker().quotaForOrigin(origin->securityOrigin.get());
#else
    return 0;
#endif
}

void ewk_security_origin_web_database_quota_set(const Ewk_Security_Origin* origin, uint64_t quota)
{
#if ENABLE(SQL_DATABASE)
    WebCore::DatabaseTracker::tracker().setQuota(origin->securityOrigin.get(), quota);
#endif
}

void ewk_security_origin_free(Ewk_Security_Origin* origin)
{
    origin->securityOrigin = 0;
    eina_stringshare_del(origin->host);
    eina_stringshare_del(origin->protocol);

    delete origin;
}

/**
 * @internal
 * Creates a EWK wrapper for WebCore Security Origin object.
 *
 * @param coreOrigin WebCore Security Origin object
 *
 * @return a EWK wrapper of WebCore Security Origin object which should be
 * freed by ewk_security_origin_free()
 */
Ewk_Security_Origin* ewk_security_origin_new(WebCore::SecurityOrigin* coreOrigin)
{
    Ewk_Security_Origin* origin = new Ewk_Security_Origin;

    origin->securityOrigin = coreOrigin;
    origin->host = 0;
    origin->protocol = 0;

    return origin;
}
