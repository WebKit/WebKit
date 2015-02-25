/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// FIXME: Provide Parameter lists for the constructors themselves? (new RegExp(...)).
// FIXME: Provide Parameter lists for global functions (eval, decodeURI, ...).

WebInspector.NativeConstructorFunctionParameters = {
    Object: {
        create: "prototype, [propertiesObject]",
        defineProperty: "object, propertyName, descriptor",
        defineProperties: "object, properties",
        freeze: "object",
        getOwnPropertyDescriptor: "object, propertyName",
        getOwnPropertyNames: "object",
        getPrototypeOf: "object",
        isExtensible: "object",
        isFrozen: "object",
        isSealed: "object",
        keys: "object",
        preventExtensions: "object",
        seal: "object",
        __proto__: null,
    },

    Array: {
        from: "arrayLike, [mapFunction], [thisArg]",
        isArray: "object",
        of: "[...values]",
        __proto__: null,
    },

    ArrayBuffer: {
        isView: "object",
        transfer: "oldBuffer, [newByteLength=length]",
        __proto__: null,
    },

    Number: {
        isFinite: "value",
        isInteger: "value",
        isNaN: "value",
        isSafeInteger: "value",
        parseFloat: "string",
        parseInt: "string, [radix]",
        __proto__: null,
    },

    Math: {
        abs: "x",
        acos: "x",
        acosh: "x",
        asin: "x",
        asinh: "x",
        atan: "x",
        atan2: "y, x",
        atanh: "x",
        cbrt: "x",
        ceil: "x",
        cos: "x",
        cosh: "x",
        exp: "x",
        expm1: "x",
        floor: "x",
        fround: "x",
        hypot: "[...x]",
        imul: "x",
        log: "x",
        log1p: "x",
        log2: "x",
        log10: "x",
        max: "[...x]",
        min: "[...x]",
        pow: "x, y",
        round: "x",
        sin: "x",
        sinh: "x",
        sqrt: "x",
        tan: "x",
        tanh: "x",
        trunc: "x",
        __proto__: null,
    },

    JSON: {
        parse: "text, [reviver]",
        stringify: "value, [replacer], [space]",
        __proto__: null,
    },

    Date: {
        parse: "dateString",
        UTC: "year, [month], [day], [hour], [minute], [second], [ms]",
        __proto__: null,
    },

    Promise: {
        all: "iterable",
        race: "iterable",
        reject: "reason",
        resolve: "value",
        __proto__: null,
    },

    String: {
        fromCharCode: "code, [...codes]",
        __proto__: null,
    },

    Symbol: {
        for: "key",
        keyFor: "symbol",
        __proto__: null,
    },
};

WebInspector.NativePrototypeFunctionParameters = {

    // Built-in JavaScript objects.
    // FIXME: TypedArrays (Int8Array, etc),

    Object: {
        __defineGetter__: "propertyName, getterFunction",
        __defineSetter__: "propertyName, setterFunction",
        __lookupGetter__: "propertyName",
        __lookupSetter__: "propertyName",
        hasOwnProperty: "propertyName",
        isPrototypeOf: "property",
        propertyIsEnumerable: "propertyName",
        __proto__: null,
    },

    Array: {
        concat: "value, ...",
        every: "callback, [thisArg]",
        fill: "value, [startIndex=0], [endIndex=length]",
        filter: "callback, [thisArg]",
        find: "callback, [thisArg]",
        findIndex: "callback, [thisArg]",
        forEach: "callback, [thisArg]",
        indexOf: "searchValue, [startIndex=0]",
        join: "[separator=\",\"]",
        lastIndexOf: "searchValue, [startIndex=length]",
        map: "callback, [thisArg]",
        push: "value, ...",
        reduce: "callback, [initialValue]",
        reduceRight: "callback, [initialValue]",
        slice: "[startIndex=0], [endIndex=length]",
        some: "callback, [thisArg]",
        sort: "[compareFunction]",
        splice: "startIndex, [deleteCount=0], ...itemsToAdd",
        __proto__: null,
    },

    ArrayBuffer: {
        slice: "startIndex, [endIndex=byteLength]",
        __proto__: null,
    },

    DataView: {
        setInt8: "byteOffset, value",
        setInt16: "byteOffset, value, [littleEndian=false]",
        setInt23: "byteOffset, value, [littleEndian=false]",
        setUint8: "byteOffset, value",
        setUint16: "byteOffset, value, [littleEndian=false]",
        setUint32: "byteOffset, value, [littleEndian=false]",
        setFloat32: "byteOffset, value, [littleEndian=false]",
        setFloat64: "byteOffset, value, [littleEndian=false]",
        getInt8: "byteOffset",
        getInt16: "byteOffset, [littleEndian=false]",
        getInt23: "byteOffset, [littleEndian=false]",
        getUint8: "byteOffset",
        getUint16: "byteOffset, [littleEndian=false]",
        getUint32: "byteOffset, [littleEndian=false]",
        getFloat32: "byteOffset, [littleEndian=false]",
        getFloat64: "byteOffset, [littleEndian=false]",
        __proto__: null,
    },

    Date: {
        setDate: "day",
        setFullYear: "year, [month=getMonth()], [day=getDate()]",
        setHours: "hours, [minutes=getMinutes()], [seconds=getSeconds()], [ms=getMilliseconds()]",
        setMilliseconds: "ms",
        setMinutes: "minutes, [seconds=getSeconds()], [ms=getMilliseconds()]",
        setMonth: "month, [day=getDate()]",
        setSeconds: "seconds, [ms=getMilliseconds()]",
        setTime: "time",
        setUTCDate: "day",
        setUTCFullYear: "year, [month=getUTCMonth()], [day=getUTCDate()]",
        setUTCHours: "hours, [minutes=getUTCMinutes()], [seconds=getUTCSeconds()], [ms=getUTCMilliseconds()]",
        setUTCMilliseconds: "ms",
        setUTCMinutes: "minutes, [seconds=getUTCSeconds()], [ms=getUTCMilliseconds()]",
        setUTCMonth: "month, [day=getUTCDate()]",
        setUTCSeconds: "seconds, [ms=getUTCMilliseconds()]",
        setUTCTime: "time",
        setYear: "year",
        __proto__: null,
    },

    Function: {
        apply: "thisObject, [argumentsArray]",
        bind: "thisObject, ...arguments",
        call: "thisObject, ...arguments",
        __proto__: null,
    },

    Map: {
        delete: "key",
        forEach: "callback, [thisArg]",
        get: "key",
        has: "key",
        set: "key, value",
        __proto__: null,
    },

    Number: {
        toExponential: "[digits]",
        toFixed: "[digits]",
        toPrecision: "[significantDigits]",
        toString: "[radix=10]",
        __proto__: null,
    },

    RegExp: {
        compile: "pattern, flags",
        exec: "string",
        test: "string",
        __proto__: null,
    },

    Set: {
        delete: "value",
        forEach: "callback, [thisArg]",
        has: "value",
        add: "value",
        __proto__: null,
    },

    String: {
        charAt: "index",
        charCodeAt: "index",
        concat: "string, ...",
        indexOf: "searchValue, [startIndex=0]",
        lastIndexOf: "searchValue, [startIndex=length]",
        localeCompare: "string",
        match: "regex",
        replace: "regex|string, replaceString|replaceHandler, [flags]",
        search: "regex",
        slice: "startIndex, [endIndex=length]",
        split: "[separator], [limit]",
        substr: "startIndex, [numberOfCharacters]",
        substring: "startIndex, [endIndex=length]",
        __proto__: null,
    },

    Promise: {
        catch: "rejectionHandler",
        then: "resolvedHandler, rejectionHandler",
        __proto__: null,
    },

    // DOM objects.
    // FIXME: Many idls. Generate?

    HTMLElement: {
        insertAdjacentElement: "position, [element]",
        insertAdjacentHTML: "position, [html]",
        insertAdjacentText: "position, [text]",
        __proto__: null,
    },
    
    Element: {
        closest: "selectors",
        getAttribute: "attributeName",
        getAttributeNS: "namespace, attributeName",
        getAttributeNode: "attributeName",
        getAttributeNodeNS: "namespace, attributeName",
        getElementsByClassName: "names",
        getElementsByTagName: "tagName",
        getElementsByTagNameNS: "namespace, localName",
        hasAttribute: "attributeName",
        hasAttributeNS: "namespace, attributeName",
        matches: "selector",
        removeAttribute: "attributeName",
        removeAttributeNS: "namespace, attributeName",
        removeAttributeNode: "attributeName",
        scrollIntoView: "[alignWithTop]",
        scrollIntoViewIfNeeded: "[centerIfNeeded]",
        scrollByLines: "[lines]",
        scrollByPages: "[pages]",
        setAttribute: "name, value",
        setAttributeNS: "namespace, name, value",
        setAttributeNode: "attributeNode",
        setAttributeNodeNS: "namespace, attributeNode",
        webkitMatchesSelector: "selectors",
        __proto__: null,
    },

    Node: {
        appendChild: "child",
        cloneNode: "deep",
        compareDocumentPosition: "node",
        contains: "node",
        insertBefore: "insertElement, referenceElement",
        isDefaultNamespace: "namespace",
        isEqualNode: "node",
        lookupNamespaceURI: "prefix",
        removeChild: "node",
        replaceChild: "newChild, oldChild",
        __proto__: null,
    },

    Window: {
        alert: "message",
        atob: "encodedData",
        btoa: "stringToEncode",
        clearTimeout: "timeoutId",
        confirm: "message",
        find: "string, [caseSensitive], [backwards], [wrapAround]",
        getComputedStyle: "element, [pseudoElement]",
        matchMedia: "mediaQueryString",
        moveBy: "deltaX, deltaY",
        moveTo: "screenX, screenY",
        open: "url, windowName, [featuresString]",
        postMessage: "message, targetOrigin, [...transferables]",
        prompt: "text, [value]",
        resizeBy: "deltaX, deltaY",
        resizeTo: "width, height",
        scrollBy: "deltaX, deltaY",
        scrollTo: "x, y",
        setInterval: "func, [delay], [...params]",
        setTimeout: "func, [delay], [...params]",
        showModalDialog: "url, [arguments], [options]",
        __proto__: null,
    },

    Event: {
        initEvent: "type, bubbles, cancelable",
        __proto__: null,
    },

    HTMLDocument: {
        write: "html",
        writeln: "html",
        __proto__: null,
    },

    Document: {
        adoptNode: "node",
        caretPositionFromPoint: "x, y",
        createAttribute: "attributeName",
        createCDATASection: "data",
        createComment: "data",
        createElement: "tagName",
        createElementNS: "namespace, qualifiedName",
        createEvent: "type",
        createExpression: "xpath, namespaceURLMapper",
        createNSResolver: "node",
        createNodeIterator: "root, whatToShow, filter",
        createProcessingInstruction: "target, data",
        createTextNode: "data",
        createTreeWalker: "root, whatToShow, filter, entityReferenceExpansion",
        elementFromPoint: "x, y",
        evaluate: "xpath, contextNode, namespaceResolver, resultType, result",
        execCommand: "command, userInterface, value",
        getElementById: "id",
        getElementsByName: "name",
        importNode: "node, deep",
        __proto__: null,
    },

    Geolocation: {
        clearWatch: "watchId",
        getCurrentPosition: "successHandler, [errorHandler], [options]",
        watchPosition: "successHandler, [errorHandler], [options]",
        __proto__: null,
    },

    Storage: {
        getItem: "key",
        key: "index",
        removeItem: "key",
        __proto__: null,
    },

    Location: {
        assign: "url",
        reload: "[force=false]",
        replace: "url",
        __proto__: null,
    },

    History: {
        go: "distance",
        pushState: "data, title, url",
        replaceState: "data, title, url",
        __proto__: null,
    },
};

WebInspector.NativePrototypeFunctionParameters.WeakMap = WebInspector.NativePrototypeFunctionParameters.Map;

(function() {
    function mixin(o, mixin) {
        for (var p in mixin)
            o[p] = mixin[p];
    }

    var EventTarget = {
        addEventListener: "type, listener, [useCapture=false]",
        removeEventListener: "type, listener, [useCapture=false]",
        dispatchEvent: "event",
    };

    mixin(WebInspector.NativePrototypeFunctionParameters.Node, EventTarget);
    mixin(WebInspector.NativePrototypeFunctionParameters.Window, EventTarget);

    var ElementQueries = {
        getElementsByClassName: "names",
        getElementsByTagName: "tagName",
        getElementsByTagNameNS: "namespace, localName",
        querySelector: "selectors",
        querySelectorAll: "selectors",
    };

    mixin(WebInspector.NativePrototypeFunctionParameters.Element, ElementQueries);
    mixin(WebInspector.NativePrototypeFunctionParameters.Document, ElementQueries);
})();
