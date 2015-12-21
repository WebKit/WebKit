/*
 * Copyright (C) 2015 Andy VanWagoner <thetalecrafter@gmail.com>.
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

// @conditional=ENABLE(INTL)

function localeCompare(that/*, locales, options */)
{
    "use strict";

    // 13.1.1 String.prototype.localeCompare (that [, locales [, options ]]) (ECMA-402 2.0)
    // http://ecma-international.org/publications/standards/Ecma-402.htm

    // 1. Let O be RequireObjectCoercible(this value).
    if (this === null)
        throw new @TypeError("String.prototype.localeCompare requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("String.prototype.localeCompare requires that |this| not be undefined");

    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    var thisString = @toString(this);

    // 4. Let That be ToString(that).
    // 5. ReturnIfAbrupt(That).
    var thatString = @toString(that);

    // 6. Let collator be Construct(%Collator%, «locales, options»).
    // 7. ReturnIfAbrupt(collator).
    var collator = new @Collator(arguments[1], arguments[2]);

    // 8. Return CompareStrings(collator, S, That).
    return collator.compare(thisString, thatString);
}
