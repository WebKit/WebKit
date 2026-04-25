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

    if (@isObject(regexp)) {
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

    if (@isObject(arg)) {
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

function search(regexp)
{
    "use strict";

    if (@isUndefinedOrNull(this))
        @throwTypeError("String.prototype.search requires that |this| not be null or undefined");

    if (@isObject(regexp)) {
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
    
    if (@isObject(separator)) {
        var splitter = separator.@@split;
        if (!@isUndefinedOrNull(splitter))
            return splitter.@call(separator, this, limit);
    }
    
    return @stringSplitFast.@call(this, separator, limit);
}
