/*
 * Copyright (C) 2015 Andy VanWagoner <thetalecrafter@gmail.com>.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
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

function match(regexp)
{
    "use strict";

    if (this == null) {
        if (this === null)
            throw new @TypeError("String.prototype.match requires that |this| not be null");
        throw new @TypeError("String.prototype.match requires that |this| not be undefined");
    }

    if (regexp != null) {
        var matcher = regexp[@symbolMatch];
        if (matcher != @undefined)
            return matcher.@call(regexp, this);
    }

    let thisString = @toString(this);
    let createdRegExp = @regExpCreate(regexp, @undefined);
    return createdRegExp[@symbolMatch](thisString);
}

function search(regexp)
{
    "use strict";

    if (this == null) {
        if (this === null)
            throw new @TypeError("String.prototype.search requires that |this| not be null");
        throw new @TypeError("String.prototype.search requires that |this| not be undefined");
    }

    if (regexp != null) {
         var searcher = regexp[@symbolSearch];
         if (searcher != @undefined)
            return searcher.@call(regexp, this);
    }

    var thisString = @toString(this);
    var createdRegExp = @regExpCreate(regexp, @undefined);
    return createdRegExp[@symbolSearch](thisString);
}

function repeatSlowPath(string, count)
{
    "use strict";

    var repeatCount = @toInteger(count);
    if (repeatCount < 0 || repeatCount === @Infinity)
        throw new @RangeError("String.prototype.repeat argument must be greater than or equal to 0 and not be infinity");

    // Return an empty string.
    if (repeatCount === 0 || string.length === 0)
        return "";

    // Return the original string.
    if (repeatCount === 1)
        return string;

    if (string.length * repeatCount > @MAX_STRING_LENGTH)
        throw new @Error("Out of memory");

    if (string.length === 1) {
        // Here, |repeatCount| is always Int32.
        return @repeatCharacter(string, repeatCount);
    }

    // Bit operation onto |repeatCount| is safe because |repeatCount| should be within Int32 range,
    // Repeat log N times to generate the repeated string rope.
    var result = "";
    var operand = string;
    while (true) {
        if (repeatCount & 1)
            result += operand;
        repeatCount >>= 1;
        if (!repeatCount)
            return result;
        operand += operand;
    }
}

function repeat(count)
{
    "use strict";

    if (this == null) {
        var message = "String.prototype.repeat requires that |this| not be undefined";
        if (this === null)
            message = "String.prototype.repeat requires that |this| not be null";
        throw new @TypeError(message);
    }

    var string = @toString(this);
    if (string.length === 1) {
        var result = @repeatCharacter(string, count);
        if (result !== null)
            return result;
    }

    return @repeatSlowPath(string, count);
}

function split(separator, limit)
{
    "use strict";
    
    if (this == null) {
        if (this === null)
            throw new @TypeError("String.prototype.split requires that |this| not be null");
        throw new @TypeError("String.prototype.split requires that |this| not be undefined");
    }
    
    if (separator != null) {
        var splitter = separator[@symbolSplit];
        if (splitter != @undefined)
            return splitter.@call(separator, this, limit);
    }
    
    return @stringSplitFast.@call(this, separator, limit);
}
