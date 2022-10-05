/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
"use strict";

// This file is for misc symbols.

// B3 types
const Void = Symbol("Void");
const Int32 = Symbol("Int32");
const Int64 = Symbol("Int64");
const Float = Symbol("Float");
const Double = Symbol("Double");

// Arg type
const GP = Symbol("GP");
const FP = Symbol("FP");

// Stack slot kind
const Locked = Symbol("Locked");
const Spill = Symbol("Spill");

// Frequency class
const Normal = Symbol("Normal");
const Rare = Symbol("Rare");

// Relational conditions
const Equal = Symbol("Equal");
const NotEqual = Symbol("NotEqual");
const Above = Symbol("Above");
const AboveOrEqual = Symbol("AboveOrEqual");
const Below = Symbol("Below");
const BelowOrEqual = Symbol("BelowOrEqual");
const GreaterThan = Symbol("GreaterThan");
const GreaterThanOrEqual = Symbol("GreaterThanOrEqual");
const LessThan = Symbol("LessThan");
const LessThanOrEqual = Symbol("LessThanOrEqual");

function relCondCode(cond)
{
    switch (cond) {
    case Equal:
        return 4;
    case NotEqual:
        return 5;
    case Above:
        return 7;
    case AboveOrEqual:
        return 3;
    case Below:
        return 2;
    case BelowOrEqual:
        return 6;
    case GreaterThan:
        return 15;
    case GreaterThanOrEqual:
        return 13;
    case LessThan:
        return 12;
    case LessThanOrEqual:
        return 14;
    default:
        throw new Error("Bad rel cond");
    }
}

// Result conditions
const Overflow = Symbol("Overflow");
const Signed = Symbol("Signed");
const PositiveOrZero = Symbol("PositiveOrZero");
const Zero = Symbol("Zero");
const NonZero = Symbol("NonZero");

function resCondCode(cond)
{
    switch (cond) {
    case Overflow:
        return 0;
    case Signed:
        return 8;
    case PositiveOrZero:
        return 9;
    case Zero:
        return 4;
    case NonZero:
        return 5;
    default:
        throw new Error("Bad res cond: " + cond.toString());
    }
}

// Double conditions
const DoubleEqual = Symbol("DoubleEqual");
const DoubleNotEqual = Symbol("DoubleNotEqual");
const DoubleGreaterThan = Symbol("DoubleGreaterThan");
const DoubleGreaterThanOrEqual = Symbol("DoubleGreaterThanOrEqual");
const DoubleLessThan = Symbol("DoubleLessThan");
const DoubleLessThanOrEqual = Symbol("DoubleLessThanOrEqual");
const DoubleEqualOrUnordered = Symbol("DoubleEqualOrUnordered");
const DoubleNotEqualOrUnordered = Symbol("DoubleNotEqualOrUnordered");
const DoubleGreaterThanOrUnordered = Symbol("DoubleGreaterThanOrUnordered");
const DoubleGreaterThanOrEqualOrUnordered = Symbol("DoubleGreaterThanOrEqualOrUnordered");
const DoubleLessThanOrUnordered = Symbol("DoubleLessThanOrUnordered");
const DoubleLessThanOrEqualOrUnordered = Symbol("DoubleLessThanOrEqualOrUnordered");

function doubleCondCode(cond)
{
    const bitInvert = 0x10;
    const bitSpecial = 0x20;
    switch (cond) {
    case DoubleEqual:
        return 4 | bitSpecial;
    case DoubleNotEqual:
        return 5;
    case DoubleGreaterThan:
        return 7;
    case DoubleGreaterThanOrEqual:
        return 3;
    case DoubleLessThan:
        return 7 | bitInvert;
    case DoubleLessThanOrEqual:
        return 3 | bitInvert;
    case DoubleEqualOrUnordered:
        return 4;
    case DoubleNotEqualOrUnordered:
        return 5 | bitSpecial;
    case DoubleGreaterThanOrUnordered:
        return 2 | bitInvert;
    case DoubleGreaterThanOrEqualOrUnordered:
        return 6 | bitInvert;
    case DoubleLessThanOrUnordered:
        return 2;
    case DoubleLessThanOrEqualOrUnordered:
        return 6;
    default:
        throw new Error("Bad cond");
    }
}

// Define pointerType()
const Ptr = 64;
