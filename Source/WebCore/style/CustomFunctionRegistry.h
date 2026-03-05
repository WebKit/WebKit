/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include "StyleRuleFunction.h"

namespace WebCore {
namespace Style {

// CustomFunction registration represents @function after things like conditional group rules (@media) and cascade layers have been resolved.
// https://drafts.csswg.org/css-mixins/#evaluating-custom-functions

struct CustomFunction : CanMakeCheckedPtr<CustomFunction> {
    AtomString name;
    const Vector<StyleRuleFunction::Parameter> parameters;
    Ref<const StyleProperties> properties;

    CustomFunction(const AtomString&, const Vector<StyleRuleFunction::Parameter>&, const StyleProperties&);

    WTF_MAKE_STRUCT_TZONE_ALLOCATED(CustomFunction);
    WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(CustomFunction);
};

class CustomFunctionRegistry : public CanMakeCheckedPtr<CustomFunctionRegistry> {
    WTF_MAKE_TZONE_ALLOCATED(CustomFunctionRegistry);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CustomFunctionRegistry);
public:
    CustomFunctionRegistry() = default;

    const CustomFunction* functionForName(const AtomString&) const;
    void registerFunction(const StyleRuleFunction&, const Vector<Ref<const StyleRuleFunctionDeclarations>>&);

private:
    HashMap<AtomString, UniqueRef<const CustomFunction>> m_functions;
};

}

}
