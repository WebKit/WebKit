/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

this.useUnicode = true;

if (!String.prototype.toTitleCase) {
    String.prototype.toTitleCase = function()
    {
        return this.charAt(0).toUpperCase() + this.substr(1).toLowerCase();
    }
}

class UnicodeStrings
{
    static get(str)
    {
        if (!this.instance) {
            this.instance = this;

            for (let keyStr in UnicodeStrings.table) {
                let keyStrLower = keyStr.toLowerCase();
                let valueStrLower = UnicodeStrings.table[keyStr].toLowerCase();
                UnicodeStrings.table[keyStrLower] = valueStrLower;
                
                let keyStrTitle = keyStr.toTitleCase();
                let valueStrTitle = UnicodeStrings.table[keyStr].toTitleCase();
                UnicodeStrings.table[keyStrTitle] = valueStrTitle;
            }
        }

        return this.table[str];
    }

}

UnicodeStrings.instance = null;
UnicodeStrings.table = {
    "START": "\u{041d}\u{0410}\u{0427}\u{0410}\u{0422}\u{042c}", // НАЧАТЬ
    "TIMING": "\u{0425}\u{0420}\u{041e}\u{041d}\u{041e}\u{041c}\u{0415}\u{0422}\u{0420}\u{0410}\u{0416}", // ХРОНОМЕТРАЖ
    "TAXI": "\u{0420}\u{0423}\u{041b}\u{0415}\u{041d}\u{0418}\u{0415}", // РУЛЕНИЕ
    "RUNUP": "\u{0414}\u{0412}\u{0418}\u{0413}\u{0410}\u{0422}\u{0415}\u{041b}\u{042c}-\u{041d}\u{0410}\u{041a}\u{0410}\u{0422}\u{0410}", // ДВИГАТЕЛЬ НАКАТА
    "TAKEOFF": "\u{0412}\u{0417}\u{041b}\u{0415}\u{0422}", // ВЗЛЕТ
    "CLIMB": "\u{041f}\u{041e}\u{0414}\u{041d}\u{042f}\u{0422}\u{042c}\u{0421}\u{042f}", // ПОДНЯТЬСЯ
    "PATTERN": "\u{041a}\u{0420}\u{0423}\u{0413}", // КРУГ
    "LEFT": "\u{041b}\u{0415}\u{0412}\u{042b}\u{0419}", // ЛЕВЫЙ
    "RIGHT": "\u{041f}\u{0420}\u{0410}\u{0412}\u{042b}\u{0419}" // ПРАВЫЙ
};
