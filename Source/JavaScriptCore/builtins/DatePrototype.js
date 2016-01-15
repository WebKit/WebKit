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

function toLocaleString(/* locales, options */)
{
    "use strict";

    function toDateTimeOptionsAnyAll(opts)
    {
        // ToDateTimeOptions abstract operation (ECMA-402 2.0)
        // http://ecma-international.org/publications/standards/Ecma-402.htm

        // 1. If options is undefined, then let options be null, else let options be ToObject(options).
        // 2. ReturnIfAbrupt(options).
        var optObj;
        if (opts === undefined)
            optObj = null;
        else if (opts === null)
            throw new @TypeError("null is not an object");
        else
            optObj = @Object(opts);

        // 3. Let options be ObjectCreate(options).
        var options = @Object.create(optObj);

        // 4. Let needDefaults be true.
        // 5. If required is "date" or "any",
        // a. For each of the property names "weekday", "year", "month", "day":
        // i. Let prop be the property name.
        // ii. Let value be Get(options, prop).
        // iii. ReturnIfAbrupt(value).
        // iv. If value is not undefined, then let needDefaults be false.
        // 6. If required is "time" or "any",
        // a. For each of the property names "hour", "minute", "second":
        // i. Let prop be the property name.
        // ii. Let value be Get(options, prop).
        // iii. ReturnIfAbrupt(value).
        // iv. If value is not undefined, then let needDefaults be false.
        // Check optObj instead of options to reduce lookups up the prototype chain.
        var needsDefaults = !optObj || (
            optObj.weekday === undefined &&
            optObj.year === undefined &&
            optObj.month === undefined &&
            optObj.day === undefined &&
            optObj.hour === undefined &&
            optObj.minute === undefined &&
            optObj.second === undefined
        );

        // 7. If needDefaults is true and defaults is either "date" or "all", then a. For each of the property names "year", "month", "day":
        // i. Let status be CreateDatePropertyOrThrow(options, prop, "numeric").
        // ii. ReturnIfAbrupt(status).
        // 8. If needDefaults is true and defaults is either "time" or "all", then
        // a. For each of the property names "hour", "minute", "second":
        // i. Let status be CreateDatePropertyOrThrow(options, prop, "numeric").
        // ii. ReturnIfAbrupt(status).
        if (needsDefaults) {
            options.year = "numeric";
            options.month = "numeric";
            options.day = "numeric";
            options.hour = "numeric";
            options.minute = "numeric";
            options.second = "numeric";
        }

        // 9. Return options.
        return options;
    }

    // 13.3.1 Date.prototype.toLocaleString ([locales [, options ]]) (ECMA-402 2.0)
    // http://ecma-international.org/publications/standards/Ecma-402.htm

    // 1. Let x be thisTimeValue(this value).
    // 2. ReturnIfAbrupt(x).
    var value = @thisTimeValue.@call(this);

    // 3. If x is NaN, return "Invalid Date".
    if (@isNaN(value))
        return "Invalid Date";

    // 4. Let options be ToDateTimeOptions(options, "any", "all").
    // 5. ReturnIfAbrupt(options).
    var options = toDateTimeOptionsAnyAll(arguments[1]);

    // 6. Let dateFormat be Construct(%DateTimeFormat%, «locales, options»).
    // 7. ReturnIfAbrupt(dateFormat).
    var locales = arguments[0];
    var dateFormat = new @DateTimeFormat(locales, options);

    // 8. Return FormatDateTime(dateFormat, x).
    return dateFormat.format(value);
}
