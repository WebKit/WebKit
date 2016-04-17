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

function advanceStringIndexUnicode(string, stringLength, index)
{
    // This function implements AdvanceStringIndex described in ES6 21.2.5.2.3, steps 6-11.
    // It assumes that "unicode" is true for its callers.
    "use strict";

    if (index + 1 >= stringLength)
        return index + 1;

    let first = string.@charCodeAt(index);
    if (first < 0xD800 || first > 0xDBFF)
        return index + 1;

    let second = string.@charCodeAt(index + 1);
    if (second < 0xDC00 || second > 0xDFFF)
        return index + 1;

    return index + 2;
}

function match(str)
{
    "use strict";

    if (!@isObject(this))
        throw new @TypeError("RegExp.prototype.@@match requires that |this| be an Object");

    let regexp = @Object(this);
    let stringArg = @toString(str);

    if (!regexp.global)
        return regexp.exec(stringArg);

    let unicode = regexp.unicode;
    regexp.lastIndex = 0;
    let resultList = [];
    let execFunc = regexp.exec;

    if (execFunc !== @RegExp.prototype.@exec && typeof execFunc === "function") {
        // Match using the overridden exec.
        let stringLength = stringArg.length;

        while (true) {
            let result = execFunc(stringArg);
            
            if (result === null) {
                if (resultList.length === 0)
                    return null;
                return resultList;
            }

            if (!@isObject(result))
                throw new @TypeError("RegExp.prototype.@@match call to RegExp.exec didn't return null or an object");

            let resultString = @toString(result[0]);

            if (!resultString.length) {
                if (unicode)
                    regexp.lastIndex = @advanceStringIndexUnicode(stringArg, stringLength, regexp.lastIndex);
                else
                    regexp.lastIndex++;
            }

            resultList.@push(resultString);

            execFunc = regexp.exec;
        }
    }

    return regexp.@match(stringArg);
}

