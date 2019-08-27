/*
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Identifier.h"

#include "CallFrame.h"
#include "JSObject.h"
#include "JSScope.h"
#include "NumericStrings.h"
#include "JSCInlines.h"
#include <new>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/StringHash.h>

namespace JSC {

Ref<StringImpl> Identifier::add(VM& vm, const char* c)
{
    ASSERT(c);
    ASSERT(c[0]);
    if (!c[1])
        return vm.smallStrings.singleCharacterStringRep(c[0]);

    return *AtomStringImpl::add(c);
}

Ref<StringImpl> Identifier::add8(VM& vm, const UChar* s, int length)
{
    if (length == 1) {
        UChar c = s[0];
        ASSERT(isLatin1(c));
        if (canUseSingleCharacterString(c))
            return vm.smallStrings.singleCharacterStringRep(c);
    }
    if (!length)
        return *StringImpl::empty();

    return *AtomStringImpl::add(s, length);
}

Identifier Identifier::from(VM& vm, unsigned value)
{
    return Identifier(vm, vm.numericStrings.add(value));
}

Identifier Identifier::from(VM& vm, int value)
{
    return Identifier(vm, vm.numericStrings.add(value));
}

Identifier Identifier::from(VM& vm, double value)
{
    return Identifier(vm, vm.numericStrings.add(value));
}

void Identifier::dump(PrintStream& out) const
{
    if (impl()) {
        if (impl()->isSymbol()) {
            auto* symbol = static_cast<SymbolImpl*>(impl());
            if (symbol->isPrivate())
                out.print("PrivateSymbol.");
        }
        out.print(impl());
    } else
        out.print("<null identifier>");
}

#ifndef NDEBUG

void Identifier::checkCurrentAtomStringTable(VM& vm)
{
    // Check the identifier table accessible through the threadspecific matches the
    // vm's identifier table.
    ASSERT_UNUSED(vm, vm.atomStringTable() == Thread::current().atomStringTable());
}

#else

// These only exists so that our exports are the same for debug and release builds.
// This would be an RELEASE_ASSERT_NOT_REACHED(), but we're in NDEBUG only code here!
NO_RETURN_DUE_TO_CRASH void Identifier::checkCurrentAtomStringTable(VM&) { CRASH(); }

#endif

} // namespace JSC
