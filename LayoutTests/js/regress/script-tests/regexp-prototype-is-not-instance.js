//@ runDefault

function stringify(x) {
    if (typeof x == "string")
        return '"' + x + '"';
    return x;
}

function assert(actual, expected) {
    if (actual !== expected)
        throw Error("FAIL: expected " + stringify(expected) + ", actual " + stringify(actual));
}

function assertThrows(func, expectedErrMsg) {
    let actualErrMsg;
    try {
        func();
        throw("FAIL: did not throw");
    } catch(e) {
        actualErrMsg = e.toString();
    }
    assert(actualErrMsg, expectedErrMsg);
}

assert(RegExp.prototype instanceof RegExp, false);

assert(RegExp.prototype.flags, "");
assert(RegExp.prototype.global, void 0);
assert(RegExp.prototype.ignoreCase, void 0);
assert(RegExp.prototype.multiline, void 0);
assert(RegExp.prototype.unicode, void 0);
assert(RegExp.prototype.sticky, void 0);
assert(RegExp.prototype.source, "(?:)");
assert(RegExp.prototype.toString(), "/(?:)/");

assert(Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags').get.call({}), "");

assertThrows(() => {
        Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags').get.call(1);
    }, 
    "TypeError: The RegExp.prototype.flags getter can only be called on an object");

assertThrows(() => {
        Object.getOwnPropertyDescriptor(RegExp.prototype, 'ignoreCase').get.call({});
    },
    "TypeError: The RegExp.prototype.ignoreCase getter can only be called on a RegExp object");

assertThrows(() => {
        Object.getOwnPropertyDescriptor(RegExp.prototype, 'multiline').get.call({});
    },
    "TypeError: The RegExp.prototype.multiline getter can only be called on a RegExp object");

assertThrows(() => {
        Object.getOwnPropertyDescriptor(RegExp.prototype, 'unicode').get.call({});
    },
    "TypeError: The RegExp.prototype.unicode getter can only be called on a RegExp object");

assertThrows(() => {
        Object.getOwnPropertyDescriptor(RegExp.prototype, 'sticky').get.call({});
    },
    "TypeError: The RegExp.prototype.sticky getter can only be called on a RegExp object");

assertThrows(() => {
        Object.getOwnPropertyDescriptor(RegExp.prototype, 'source').get.call({});
    },
    "TypeError: The RegExp.prototype.source getter can only be called on a RegExp object");

assertThrows(() => {
        Object.getOwnPropertyDescriptor(RegExp.prototype, 'toString').get.call({});
    },
    "TypeError: undefined is not an object (evaluating 'Object.getOwnPropertyDescriptor(RegExp.prototype, 'toString').get.call')");
