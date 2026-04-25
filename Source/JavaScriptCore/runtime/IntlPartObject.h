/*
 * Copyright (C) 2026 Anthropic PBC.
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

#include "JSGlobalObject.h"
#include "JSString.h"
#include "ObjectConstructor.h"

namespace JSC {

// PropertyOffset definitions for pre-built Intl part object Structures.
// {type, value}
static constexpr PropertyOffset intlPartObjectTypePropertyOffset = 0;
static constexpr PropertyOffset intlPartObjectValuePropertyOffset = 1;

// {type, value, source}
static constexpr PropertyOffset intlPartObjectWithSourceSourcePropertyOffset = 2;

// {type, value, unit}
static constexpr PropertyOffset intlPartObjectWithUnitUnitPropertyOffset = 2;

// {type, value, unit, source}
static constexpr PropertyOffset intlPartObjectWithUnitAndSourceUnitPropertyOffset = 2;
static constexpr PropertyOffset intlPartObjectWithUnitAndSourceSourcePropertyOffset = 3;

// Structure creation functions
Structure* createIntlPartObjectStructure(VM&, JSGlobalObject&);
Structure* createIntlPartObjectWithSourceStructure(VM&, JSGlobalObject&);
Structure* createIntlPartObjectWithUnitStructure(VM&, JSGlobalObject&);
Structure* createIntlPartObjectWithUnitAndSourceStructure(VM&, JSGlobalObject&);

// Inline helper functions for creating Intl part objects
ALWAYS_INLINE JSObject* createIntlPartObject(JSGlobalObject* globalObject, JSString* type, JSString* value)
{
    VM& vm = globalObject->vm();
    Structure* structure = globalObject->intlPartObjectStructure();
    JSObject* result = constructEmptyObject(vm, structure);
    result->putDirectOffset(vm, intlPartObjectTypePropertyOffset, type);
    result->putDirectOffset(vm, intlPartObjectValuePropertyOffset, value);
    return result;
}

ALWAYS_INLINE JSObject* createIntlPartObjectWithSource(JSGlobalObject* globalObject, JSString* type, JSString* value, JSString* source)
{
    VM& vm = globalObject->vm();
    Structure* structure = globalObject->intlPartObjectWithSourceStructure();
    JSObject* result = constructEmptyObject(vm, structure);
    result->putDirectOffset(vm, intlPartObjectTypePropertyOffset, type);
    result->putDirectOffset(vm, intlPartObjectValuePropertyOffset, value);
    result->putDirectOffset(vm, intlPartObjectWithSourceSourcePropertyOffset, source);
    return result;
}

ALWAYS_INLINE JSObject* createIntlPartObjectWithUnit(JSGlobalObject* globalObject, JSString* type, JSString* value, JSString* unit)
{
    VM& vm = globalObject->vm();
    Structure* structure = globalObject->intlPartObjectWithUnitStructure();
    JSObject* result = constructEmptyObject(vm, structure);
    result->putDirectOffset(vm, intlPartObjectTypePropertyOffset, type);
    result->putDirectOffset(vm, intlPartObjectValuePropertyOffset, value);
    result->putDirectOffset(vm, intlPartObjectWithUnitUnitPropertyOffset, unit);
    return result;
}

ALWAYS_INLINE JSObject* createIntlPartObjectWithUnitAndSource(JSGlobalObject* globalObject, JSString* type, JSString* value, JSString* unit, JSString* source)
{
    VM& vm = globalObject->vm();
    Structure* structure = globalObject->intlPartObjectWithUnitAndSourceStructure();
    JSObject* result = constructEmptyObject(vm, structure);
    result->putDirectOffset(vm, intlPartObjectTypePropertyOffset, type);
    result->putDirectOffset(vm, intlPartObjectValuePropertyOffset, value);
    result->putDirectOffset(vm, intlPartObjectWithUnitAndSourceUnitPropertyOffset, unit);
    result->putDirectOffset(vm, intlPartObjectWithUnitAndSourceSourcePropertyOffset, source);
    return result;
}

} // namespace JSC
