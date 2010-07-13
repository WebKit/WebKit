/*
    Copyright (C) 2010 ProFUSION embedded systems
    Copyright (C) 2010 Samsung Electronics

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

#ifndef ewk_cookies_h
#define ewk_cookies_h

#include "ewk_eapi.h"
#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _Ewk_Cookie {
    char *name;
    char *value;
    char *domain;
    char *path;
    time_t expires;
    Eina_Bool secure;
    Eina_Bool http_only;
};

typedef struct _Ewk_Cookie Ewk_Cookie;

enum _Ewk_Cookie_Policy {
    EWK_COOKIE_JAR_ACCEPT_NEVER,
    EWK_COOKIE_JAR_ACCEPT_ALWAYS,
    EWK_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY
};

typedef enum _Ewk_Cookie_Policy Ewk_Cookie_Policy;

/************************** Exported functions ***********************/

EAPI Eina_Bool          ewk_cookies_file_set(const char *filename);
EAPI void               ewk_cookies_clear();
EAPI Eina_List*         ewk_cookies_get_all();
EAPI void               ewk_cookies_cookie_del(Ewk_Cookie *cookie);
EAPI void               ewk_cookies_cookie_free(Ewk_Cookie *cookie);
EAPI void               ewk_cookies_policy_set(Ewk_Cookie_Policy p);
EAPI Ewk_Cookie_Policy  ewk_cookies_policy_get();

#ifdef __cplusplus
}
#endif
#endif // ewk_cookies_h
