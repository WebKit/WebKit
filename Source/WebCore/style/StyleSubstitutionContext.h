/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/GenericHashKey.h>
#include <wtf/HashSet.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace Style {

// https://drafts.csswg.org/css-values-5/#substitution-context
// A substitution context is «dependency type, name».
struct SubstitutionContext {
    enum class Type : uint8_t { Property, Attribute, Function };
    Type type;
    AtomString name;

    bool operator==(const SubstitutionContext&) const = default;
};

inline void add(Hasher& hasher, const SubstitutionContext& context)
{
    add(hasher, context.type, context.name);
}

// https://drafts.csswg.org/css-values-5/#guarded
class GuardedSubstitutionContexts {
public:
    // RAII guard for a substitution context. If the context is already guarded,
    // all contexts involved in the cycle are marked as cyclic per spec.
    // Unguards on destruction (unless the context was already guarded).
    class Guard {
        WTF_MAKE_NONCOPYABLE(Guard);
    public:
        Guard(GuardedSubstitutionContexts& contexts, SubstitutionContext&& context)
            : m_contexts(contexts)
            , m_context(WTF::move(context))
            , m_previous(contexts.m_top)
        {
            if (contexts.m_guarded.add(m_context).isNewEntry) {
                contexts.m_top = this;
                return;
            }
            m_isCyclicContext = true;
            // Mark all contexts involved in the cycle as cyclic.
            for (auto* guard = m_previous; guard; guard = guard->m_previous) {
                guard->m_isCyclicContext = true;
                if (guard->m_context == m_context)
                    break;
            }
        }
        ~Guard()
        {
            if (m_contexts.m_top != this)
                return;
            m_contexts.m_guarded.remove(m_context);
            m_contexts.m_top = m_previous;
        }

        // Whether the guarded context is a cyclic substitution context.
        bool isCyclicContext() const { return m_isCyclicContext; }

    private:
        GuardedSubstitutionContexts& m_contexts;
        SubstitutionContext m_context;
        Guard* m_previous;
        bool m_isCyclicContext { false };
    };

    Guard guard(SubstitutionContext&& context) { return { *this, WTF::move(context) }; }

    void addFunctionContextsFrom(const GuardedSubstitutionContexts& other)
    {
        for (auto& context : other.m_guarded) {
            if (context.key().type == SubstitutionContext::Type::Function)
                m_guarded.add(context);
        }
        m_top = other.m_top;
    }

private:
    HashSet<GenericHashKey<SubstitutionContext>> m_guarded;
    Guard* m_top { nullptr };
};

} // namespace Style
} // namespace WebCore
