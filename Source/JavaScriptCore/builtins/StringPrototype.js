/*
 * Copyright (C) 2015 Andy VanWagoner <andy@vanwagoner.family>.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.match requires that |this| not be null or undefined");

    if (!@isUndefinedOrNull(regexp)) {
        var matcher = regexp.@@match;
        if (!@isUndefinedOrNull(matcher))
            return matcher.@call(regexp, this);
    }

    var thisString = @toString(this);
    var createdRegExp = @regExpCreate(regexp, @undefined);
    return createdRegExp.@@match(thisString);
}

function matchAll(arg)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.matchAll requires |this| not to be null nor undefined");

    if (!@isUndefinedOrNull(arg)) {
        if (@isRegExp(arg) && !@stringIncludesInternal.@call(@toString(arg.flags), "g"))
            @throwTypeError("String.prototype.matchAll argument must not be a non-global regular expression");

        var matcher = arg.@@matchAll;
        if (!@isUndefinedOrNull(matcher))
            return matcher.@call(arg, this);
    }

    var string = @toString(this);
    var regExp = @regExpCreate(arg, "g");
    return regExp.@@matchAll(string);
}

@linkTimeConstant
function repeatSlowPath(string, count)
{
    "use strict";

    // Return an empty string.
    if (count === 0 || string.length === 0)
        return "";

    // Return the original string.
    if (count === 1)
        return string;

    if (string.length * count > @MAX_STRING_LENGTH)
        @throwOutOfMemoryError();

    // Bit operation onto |count| is safe because |count| should be within Int32 range,
    // Repeat log N times to generate the repeated string rope.
    var result = "";
    var operand = string;
    while (true) {
        if (count & 1)
            result += operand;
        count >>= 1;
        if (!count)
            return result;
        operand += operand;
    }
}

@linkTimeConstant
function repeatCharactersSlowPath(string, count)
{
    "use strict";
    var repeatCount = (count / string.length) | 0;
    var remainingCharacters = count - repeatCount * string.length;
    var result = "";
    var operand = string;
    // Bit operation onto |repeatCount| is safe because |repeatCount| should be within Int32 range,
    // Repeat log N times to generate the repeated string rope.
    while (true) {
        if (repeatCount & 1)
            result += operand;
        repeatCount >>= 1;
        if (!repeatCount)
            break;
        operand += operand;
    }
    if (remainingCharacters)
        result += @stringSubstring.@call(string, 0, remainingCharacters);
    return result;
}


function repeat(count)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.repeat requires that |this| not be null or undefined");

    var string = @toString(this);
    count = @toIntegerOrInfinity(count);

    if (count < 0 || count === @Infinity)
        @throwRangeError("String.prototype.repeat argument must be greater than or equal to 0 and not be Infinity");

    if (string.length === 1)
        return @repeatCharacter(string, count);

    return @repeatSlowPath(string, count);
}

function padStart(maxLength/*, fillString*/)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.padStart requires that |this| not be null or undefined");

    var string = @toString(this);
    maxLength = @toLength(maxLength);

    var stringLength = string.length;
    if (maxLength <= stringLength)
        return string;

    var filler;
    var fillString = @argument(1);
    if (fillString === @undefined)
        filler = " ";
    else {
        filler = @toString(fillString);
        if (filler === "")
            return string;
    }

    if (maxLength > @MAX_STRING_LENGTH)
        @throwOutOfMemoryError();

    var fillLength = maxLength - stringLength;
    var truncatedStringFiller;

    if (filler.length === 1)
        truncatedStringFiller = @repeatCharacter(filler, fillLength);
    else
        truncatedStringFiller = @repeatCharactersSlowPath(filler, fillLength);
    return truncatedStringFiller + string;
}

function padEnd(maxLength/*, fillString*/)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.padEnd requires that |this| not be null or undefined");

    var string = @toString(this);
    maxLength = @toLength(maxLength);

    var stringLength = string.length;
    if (maxLength <= stringLength)
        return string;

    var filler;
    var fillString = @argument(1);
    if (fillString === @undefined)
        filler = " ";
    else {
        filler = @toString(fillString);
        if (filler === "")
            return string;
    }

    if (maxLength > @MAX_STRING_LENGTH)
        @throwOutOfMemoryError();

    var fillLength = maxLength - stringLength;
    var truncatedStringFiller;

    if (filler.length === 1)
        truncatedStringFiller = @repeatCharacter(filler, fillLength);
    else
        truncatedStringFiller = @repeatCharactersSlowPath(filler, fillLength);
    return string + truncatedStringFiller;
}

@linkTimeConstant
function hasObservableSideEffectsForStringReplace(regexp, replacer)
{
    "use strict";

    if (!@isRegExpObject(regexp))
        return true;

    if (replacer !== @regExpPrototypeSymbolReplace)
        return true;
    
    var regexpExec = @tryGetById(regexp, "exec");
    if (regexpExec !== @regExpBuiltinExec)
        return true;

    var regexpGlobal = @tryGetById(regexp, "global");
    if (regexpGlobal !== @regExpProtoGlobalGetter)
        return true;

    var regexpUnicode = @tryGetById(regexp, "unicode");
    if (regexpUnicode !== @regExpProtoUnicodeGetter)
        return true;

    return typeof regexp.lastIndex !== "number";
}

@intrinsic=StringPrototypeReplaceIntrinsic
function replace(search, replace)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.replace requires that |this| not be null or undefined");

    if (!@isUndefinedOrNull(search)) {
        var replacer = search.@@replace;
        if (!@isUndefinedOrNull(replacer)) {
            if (!@hasObservableSideEffectsForStringReplace(search, replacer))
                return @toString(this).@replaceUsingRegExp(search, replace);
            return replacer.@call(search, this, replace);
        }
    }

    var thisString = @toString(this);
    var searchString = @toString(search);
    return thisString.@replaceUsingStringSearch(searchString, replace);
}

function replaceAll(search, replace)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.replaceAll requires |this| not to be null nor undefined");

    if (!@isUndefinedOrNull(search)) {
        if (@isRegExp(search) && !@stringIncludesInternal.@call(@toString(search.flags), "g"))
            @throwTypeError("String.prototype.replaceAll argument must not be a non-global regular expression");

        var replacer = search.@@replace;
        if (!@isUndefinedOrNull(replacer)) {
            if (!@hasObservableSideEffectsForStringReplace(search, replacer))
                return @toString(this).@replaceUsingRegExp(search, replace);
            return replacer.@call(search, this, replace);
        }
    }

    var thisString = @toString(this);
    var searchString = @toString(search);
    return thisString.@replaceAllUsingStringSearch(searchString, replace);
}

function search(regexp)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.search requires that |this| not be null or undefined");

    if (!@isUndefinedOrNull(regexp)) {
        var searcher = regexp.@@search;
        if (!@isUndefinedOrNull(searcher))
            return searcher.@call(regexp, this);
    }

    var thisString = @toString(this);
    var createdRegExp = @regExpCreate(regexp, @undefined);
    return createdRegExp.@@search(thisString);
}

function split(separator, limit)
{
    "use strict";
    
    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.split requires that |this| not be null or undefined");
    
    if (!@isUndefinedOrNull(separator)) {
        var splitter = separator.@@split;
        if (!@isUndefinedOrNull(splitter))
            return splitter.@call(separator, this, limit);
    }
    
    return @stringSplitFast.@call(this, separator, limit);
}

@linkTimeConstant
function stringConcatSlowPath()
{
    "use strict";

    var result = @toString(this);
    for (var i = 0, length = arguments.length; i < length; ++i)
        result += @toString(arguments[i]);
    return result;
}

function concat(arg /* ... */)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.concat requires that |this| not be null or undefined");

    if (@argumentCount() === 1)
        return @toString(this) + @toString(arg);
    return @tailCallForwardArguments(@stringConcatSlowPath, this);
}

// FIXME: This is extremely similar to charAt, so we should optimize it accordingly.    
//        https://bugs.webkit.org/show_bug.cgi?id=217139    
function at(index)    
{   
    "use strict";   

    if (@isUndefinedOrNull(this))   
        @throwTypeError("String.prototype.at requires that |this| not be null or undefined"); 

    var string = @toString(this);   
    var length = string.length; 

    var k = @toIntegerOrInfinity(index);  
    if (k < 0)  
        k += length;    

    return (k >= 0 && k < length) ? string[k] : @undefined; 
}

@linkTimeConstant
function createHTML(func, string, tag, attribute, value)
{
    "use strict";
    if (@isUndefinedOrNull(string))
        @throwTypeError(`${func} requires that |this| not be null or undefined`);
    var S = @toString(string);
    var p1 = "<" + tag;
    if (attribute) {
        var V = @toString(value);
        var escapedV = V.@replaceUsingRegExp(/"/g, '&quot;');
        p1 = p1 + " " + @toString(attribute) + '="' + escapedV + '"'
    }
    var p2 = p1 + ">"
    var p3 = p2 + S;
    var p4 = p3 + "</" + tag + ">";
    return p4;
}

function anchor(url)
{
    "use strict";
    return @createHTML("String.prototype.link", this, "a", "name", url)
}

function big()
{
    "use strict";
    return @createHTML("String.prototype.big", this, "big", "", "");
}

function blink()
{
    "use strict";
    return @createHTML("String.prototype.blink", this, "blink", "", "");
}

function bold()
{
    "use strict";
    return @createHTML("String.prototype.bold", this, "b", "", "");
}

function fixed()
{
    "use strict";
    return @createHTML("String.prototype.fixed", this, "tt", "", "");
}

function fontcolor(color)
{
    "use strict";
    return @createHTML("String.prototype.fontcolor", this, "font", "color", color);
}

function fontsize(size)
{
    "use strict";
    return @createHTML("String.prototype.fontsize", this, "font", "size", size);
}

function italics()
{
    "use strict";
    return @createHTML("String.prototype.italics", this, "i", "", "");
}

function link(url)
{
    "use strict";
    return @createHTML("String.prototype.link", this, "a", "href", url)
}

function small()
{
    "use strict";
    return @createHTML("String.prototype.small", this, "small", "", "");
}

function strike()
{
    "use strict";
    return @createHTML("String.prototype.strike", this, "strike", "", "");
}

function sub()
{
    "use strict";
    return @createHTML("String.prototype.sub", this, "sub", "", "");
}

function sup()
{
    "use strict";
    return @createHTML("String.prototype.sup", this, "sup", "", "");
}
