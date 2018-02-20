/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

namespace JSC {

enum class SecurityKind : uint8_t {
    // The JSValueOOB security kind is for cells that contain JValues and can be accessed out-of-bounds
    // up to minimumDistanceBetweenCellsFromDifferentOrigins.
    //
    // JSValues can reference cells in JSValueOOB. Therefore, JSValues can only reference cells in
    // JSValueOOB - otherwise a Spectre OOB attack would be able to violate the rules of JSValueStrict
    // and DangerousBits.
    //
    // The OOB space is the space that depends on the heap's distancing to do OOB protection.
    JSValueOOB,
    
    // The JSValueStrict security kind is for cells that contain JSValues but cannot be accessed
    // out-of-bounds. Currently, it's not essential to keep this separate from DangerousBits. We're
    // using this to get some wiggle room for how we handle array elements. For example, we might want
    // to allow OOB reads but not OOB writes.
    //
    // It's illegal to use this for any subclass of JSObject, JSString, or Symbol, or any other cell
    // that could be referenced from a JSValue. You must use poisoned pointers to point at these cells.
    JSValueStrict,
    
    // The DangerousBits security kind is for cells that contain values that could be usefully type-
    // confused with JSValue.
    //
    // It's illegal to use this for any subclass of JSObject, JSString, or Symbol, or any other cell
    // that could be referenced from a JSValue. You must use poisoned pointers to point at these cells.
    DangerousBits
};

} // namespace JSC

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::SecurityKind);

} // namespace WTF

