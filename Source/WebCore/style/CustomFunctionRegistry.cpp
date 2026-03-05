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

#include "config.h"
#include "CustomFunctionRegistry.h"

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CustomFunction);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CustomFunctionRegistry);

CustomFunction::CustomFunction(const AtomString& name, const Vector<StyleRuleFunction::Parameter>& parameters, const StyleProperties& properties)
    : name(name)
    , parameters(parameters)
    , properties(properties)
{
}

void CustomFunctionRegistry::registerFunction(const StyleRuleFunction& function, const Vector<Ref<const StyleRuleFunctionDeclarations>>& declarationsList)
{
    if (declarationsList.isEmpty())
        return;

    auto mergedProperties = [&]() -> Ref<const StyleProperties> {
        if (declarationsList.size() == 1)
            return declarationsList.first()->properties();

        auto mutableProperties = MutableStyleProperties::create();
        for (auto& declarations : declarationsList) {
            Ref properties = declarations->properties();
            mutableProperties->mergeAndOverrideOnConflict(properties.get());
        }
        return mutableProperties->immutableCopy();
    };

    auto customFunction = makeUniqueRef<CustomFunction>(function.name(), function.parameters(), mergedProperties());

    // Last function with the same name wins.
    m_functions.set(function.name(), WTF::move(customFunction));
}

const CustomFunction* CustomFunctionRegistry::functionForName(const AtomString& name) const
{
    return m_functions.get(name);
}

}
}

