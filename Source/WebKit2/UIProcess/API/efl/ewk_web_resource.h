/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    ewk_web_resource.h
 * @brief   Describes the Web Resource API.
 */

#ifndef ewk_web_resource_h
#define ewk_web_resource_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for @a Ewk_Web_Resource. */
typedef struct _Ewk_Web_Resource Ewk_Web_Resource;

/**
 * Increases the reference count of the given object.
 *
 * @param resource the resource object to increase the reference count
 */
EAPI void ewk_web_resource_ref(Ewk_Web_Resource *resource);

/**
 * Decreases the reference count of the given object, possibly freeing it.
 *
 * When the reference count it's reached 0, the resource is freed.
 *
 * @param resource the resource object to decrease the reference count
 */
EAPI void ewk_web_resource_unref(Ewk_Web_Resource *resource);

/**
 * Query URL for this resource.
 *
 * @param resource resource object to query.
 *
 * @return the URL pointer, that may be @c NULL.
 */
EAPI const char *ewk_web_resource_url_get(const Ewk_Web_Resource *resource);

/**
 * Query if this is the main resource.
 *
 * @param resource resource object to query.
 *
 * @return @c EINA_TRUE if this is the main resource, @c EINA_FALSE otherwise
 */
EAPI Eina_Bool ewk_web_resource_main_resource_get(const Ewk_Web_Resource *resource);

#ifdef __cplusplus
}
#endif

#endif // ewk_web_resource_h
