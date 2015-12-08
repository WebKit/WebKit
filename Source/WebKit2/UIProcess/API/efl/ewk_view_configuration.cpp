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

#include "config.h"
#include "ewk_view_configuration.h"

#include "WKAPICast.h"
#include "WKPageGroup.h"
#include "ewk_page_group_private.h"
#include "ewk_settings_private.h"
#include "ewk_view_configuration_private.h"

Ref<EwkViewConfiguration> EwkViewConfiguration::create(WKPageConfigurationRef configuration)
{
    return adoptRef(*new EwkViewConfiguration(configuration));
}

EwkViewConfiguration::EwkViewConfiguration(WKPageConfigurationRef pageConfiguration)
    : m_pageConfiguration(pageConfiguration)
{
    WKPageGroupRef pageGroup = WKPageConfigurationGetPageGroup(pageConfiguration);
    if (!pageGroup) {
        pageGroup = WKPageGroupCreateWithIdentifier(nullptr);
        WKPageConfigurationSetPageGroup(pageConfiguration, pageGroup);
    }

    m_pageGroup = EwkPageGroup::findOrCreateWrapper(pageGroup);
}

Ewk_View_Configuration* ewk_view_configuration_new()
{
    return &EwkViewConfiguration::create(WKPageConfigurationCreate()).leakRef();
}

Ewk_Settings* ewk_view_configuration_settings_get(const Ewk_View_Configuration* ewkConfiguration)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkViewConfiguration, ewkConfiguration, impl, nullptr);

    return impl->pageGroup()->settings();
}
