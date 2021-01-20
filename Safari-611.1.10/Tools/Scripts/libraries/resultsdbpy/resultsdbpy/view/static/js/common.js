// Copyright (C) 2019, 2020 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

function ErrorDisplay(json) {
    return `<div class="content">
        <h2>${json['error']}<\h2>
        <div class="text">${json['description']}</div>
    </div>`;
}

function queryToParams(query) {
    if (!query)
        return {};

    var result = {};
    query.split('&').forEach((element) => {
        var splitQuery = element.split('=');
        if (splitQuery.length != 2) {
            if (!result[splitQuery[0]])
                result[splitQuery[0]] = null;
            return;
        }
        if (!result[splitQuery[0]])
            result[splitQuery[0]] = [];
        result[decodeURIComponent(splitQuery[0])].push(decodeURIComponent(splitQuery[1]));
    });
    return result;
}

function paramsToQuery(json) {
    var result = '';
    for (var parameter in json) {
        if (!json[parameter]) {
            result += (result ? '&' : '') + encodeURIComponent(parameter);
            continue;
        }
        json[parameter].forEach((value) => {
            if (value === null || value === undefined)
                return;
            result += (result ? '&' : '') + encodeURIComponent(parameter) + '=' + encodeURIComponent(value);
        });
    }
    return result;
}

class QueryModifier {
    constructor(argument) {
        this.argument = argument;
    }

    current() {
        var params = queryToParams(document.URL.split('?')[1]);
        if (!params[this.argument])
            return [];
        return params[this.argument];
    }

    append(value) {
        const splitURL = document.URL.split('?');
        var params = queryToParams(splitURL[1]);
        if (!params[this.argument])
            params[this.argument] = [];
        if (params[this.argument].indexOf(value) >= 0)
            return;
        params[this.argument].push(value);

        const queryString = paramsToQuery(params);
        window.history.pushState(queryString, '', splitURL[0] + '?' + queryString);
    }

    remove(value = null) {
        const splitURL = document.URL.split('?');
        var params = queryToParams(splitURL[1]);
        if (!params[this.argument])
            return;
        if (value === null)
            delete params[this.argument];
        else {
            const index = params[this.argument].indexOf(value);
            if (index < 0)
                return;
            params[this.argument].splice(index, 1);
        }

        const queryString = paramsToQuery(params);
        const url = splitURL[0] + (queryString ? '?' + queryString : '');
        window.history.pushState(queryString, '', url);
    }

    replace(value) {
        const splitURL = document.URL.split('?');
        var params = queryToParams(splitURL[1]);
        params[this.argument] = [value];

        const queryString = paramsToQuery(params);
        window.history.pushState(queryString, '', splitURL[0] + '?' + queryString);
     }
}

// Based on <https://stackoverflow.com/questions/6234773/can-i-escape-html-special-chars-in-javascript>
function escapeHTML(text) {
    return text.replace(/[&<"'\n]/g, function(character) {
        switch (character) {
            case '&':
                return '&amp;';
            case '<':
                return '&lt;';
            case '"':
                return '&quot;';
            case '\n':
                return '<br>';
            default:
                return '&#039;';
        }
  });
}

function deepCompare(a, b) {
    if (a === b)
        return true;

    if (Array.isArray(a) && Array.isArray(b)) {
        if (a.length != b.length)
            return false;
        for (let i = 0; i < a.length; ++i) {
            if (!deepCompare(a[i], b[i]))
                return false;
        }
        return true;
    }

    if (!(a instanceof Object) || !(b instanceof Object))
        return false;

    for (let key in a) {
        if (!(key in b))
            return false;
        if (!deepCompare(a[key], b[key]))
            return false;
    }
    for (let key in b ) {
        if (!(key in a))
            return false;
    }
    return true;
}

function percentage(value, max)
{
    if (value === max)
        return '100 %';
    if (!value)
        return '0 %';
    const result = Math.round(value / max * 100);
    if (!result)
        return '<1 %';
    if (result === 100)
        return '99 %';
    return `${result} %`
}

function elapsedTime(startTimestamp, endTimestamp)
{
    const time = new Date((endTimestamp - startTimestamp) * 1000);
    let result = '';
    if (time.getMinutes())
        result += `${time.getMinutes()} minute${time.getMinutes() == 1 ? '' : 's'} and `;
    result += `${time.getSeconds()} second${time.getSeconds() == 1 ? '' : 's'} to run`;
    return result;
}

export {deepCompare, ErrorDisplay, queryToParams, paramsToQuery, QueryModifier, escapeHTML, percentage, elapsedTime};
