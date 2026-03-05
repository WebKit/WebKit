/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "APIContentWorldConfiguration.h"

#include "ContentWorldShared.h"

namespace API {

Ref<ContentWorldConfiguration> ContentWorldConfiguration::create()
{
    return adoptRef(*new ContentWorldConfiguration());
}

ContentWorldConfiguration::Data::Data() = default;

ContentWorldConfiguration::ContentWorldConfiguration() = default;

ContentWorldConfiguration::~ContentWorldConfiguration() = default;

OptionSet<WebKit::ContentWorldOption> ContentWorldConfiguration::optionSet() const
{
    OptionSet<WebKit::ContentWorldOption> result;

    if (allowAccessToClosedShadowRoots())
        result.add(WebKit::ContentWorldOption::AllowAccessToClosedShadowRoots);
    if (allowAutofill())
        result.add(WebKit::ContentWorldOption::AllowAutofill);
    if (allowElementUserInfo())
        result.add(WebKit::ContentWorldOption::AllowElementUserInfo);
    if (disableLegacyBuiltinOverrides())
        result.add(WebKit::ContentWorldOption::DisableLegacyBuiltinOverrides);
    if (allowJSHandleCreation())
        result.add(WebKit::ContentWorldOption::AllowJSHandleCreation);
    if (allowNodeSerialization())
        result.add(WebKit::ContentWorldOption::AllowNodeSerialization);
    if (isInspectable())
        result.add(WebKit::ContentWorldOption::Inspectable);

    return result;
}

Ref<ContentWorldConfiguration> ContentWorldConfiguration::copy() const
{
    Ref other = create();
    other->m_data = m_data;
    return other;
}

const WTF::String& ContentWorldConfiguration::name() const
{
    return m_data.name;
}

void ContentWorldConfiguration::setName(WTF::String&& name)
{
    m_data.name = WTF::move(name);
}

bool ContentWorldConfiguration::allowAccessToClosedShadowRoots() const
{
    return m_data.allowAccessToClosedShadowRoots;
}

void ContentWorldConfiguration::setAllowAccessToClosedShadowRoots(bool allow)
{
    m_data.allowAccessToClosedShadowRoots = allow;
}

bool ContentWorldConfiguration::allowAutofill() const
{
    return m_data.allowAutofill;
}

void ContentWorldConfiguration::setAllowAutofill(bool allow)
{
    m_data.allowAutofill = allow;
}

bool ContentWorldConfiguration::allowElementUserInfo() const
{
    return m_data.allowElementUserInfo;
}

void ContentWorldConfiguration::setAllowElementUserInfo(bool allow)
{
    m_data.allowElementUserInfo = allow;
}

bool ContentWorldConfiguration::disableLegacyBuiltinOverrides() const
{
    return m_data.disableLegacyBuiltinOverrides;
}

void ContentWorldConfiguration::setDisableLegacyBuiltinOverrides(bool disable)
{
    m_data.disableLegacyBuiltinOverrides = disable;
}

bool ContentWorldConfiguration::allowJSHandleCreation() const
{
    return m_data.allowJSHandleCreation;
}

void ContentWorldConfiguration::setAllowJSHandleCreation(bool allow)
{
    m_data.allowJSHandleCreation = allow;
}

bool ContentWorldConfiguration::allowNodeSerialization() const
{
    return m_data.allowNodeSerialization;
}

void ContentWorldConfiguration::setAllowNodeSerialization(bool allow)
{
    m_data.allowNodeSerialization = allow;
}

bool ContentWorldConfiguration::isInspectable() const
{
    return m_data.inspectable;
}

void ContentWorldConfiguration::setInspectable(bool inspectable)
{
    m_data.inspectable = inspectable;
}

} // namespace API
