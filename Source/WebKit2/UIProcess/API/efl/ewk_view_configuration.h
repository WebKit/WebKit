/*
 * Copyright (C) 2015 Naver Corp. All rights reserved.
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
 * @file    ewk_view_configuration.h
 * @brief   Describes the Ewk_View_Configuration API.
 */

#ifndef ewk_view_configuration_h
#define ewk_view_configuration_h

#include "ewk_settings.h"
#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Declare Ewk_View_Configuration as Ewk_Object.
 *
 * Ewk_View_Configuration is a collection of properties used to initialize a ewk_view.
 * 
 * @see Ewk_Object
 * @see ewk_view_add_with_configuration
 */
#ifdef __cplusplus
typedef class EwkObject Ewk_View_Configuration;
#else
typedef struct EwkObject Ewk_View_Configuration;
#endif

/**
 * Creates a new Ewk_View_Configuration.
 *
 * The returned Ewk_View_Configuration object @b should be unref'ed after use.
 *
 * @return Ewk_View_Configuration object on success or @c NULL on failure
 *
 * @see ewk_object_unref
 */
EAPI Ewk_View_Configuration *ewk_view_configuration_new(void);

/**
 * Gets the Ewk_Settings of this @a configuration.
 *
 * @param configuration Ewk_View_Configuration object to get Ewk_Settings
 *
 * @return the Ewk_Settings instance or @c NULL on failure
 */
EAPI Ewk_Settings *ewk_view_configuration_settings_get(const Ewk_View_Configuration *configuration);

#ifdef __cplusplus
}
#endif

#endif
