// This test checks the behavior of custom iterable objects.

var returnCalled = false;
var iter = {
    __key: 0,
    next: function () {
        return {
            done: this.__key === 42,
            value: this.__key++
        };
    },
    [Symbol.iterator]: function () {
        return this;
    },
    return: function () {
        returnCalled = true;
    }
};
var expected = 0;
for (var value of iter) {
    if (value !== expected++)
        throw "Error: bad value: " + value;
}
if (returnCalled)
    throw new Error("return was called");



var returnCalled = false;
var iter = {
    __key: 0,
    next: function () {
        return {
            done: this.__key === 42,
            value: this.__key++
        };
    },
    [Symbol.iterator]: function () {
        return this;
    },
    return: function () {
        returnCalled = true;
        return {
            done: true,
            value: undefined
        };
    }
};

try {
    for (var value of iter) {
        throw "Error: Terminate iteration.";
    }
} catch (e) {
    if (String(e) !== "Error: Terminate iteration.")
        throw e;
}
if (!returnCalled)
    throw "Error: return is not called.";



var returnCalled = false;
var iter = {
    __key: 0,
    next: function () {
        return {
            done: this.__key === 42,
            value: this.__key++
        };
    },
    [Symbol.iterator]: function () {
        return this;
    },
    return: function () {
        returnCalled = true;
        return {
            done: true,
            value: undefined
        };
    }
};

for (var value of iter) {
    break;
}
if (!returnCalled)
    throw "Error: return is not called.";



var returnCalled = false;
var iter = {
    __key: 0,
    get next() {
        throw "Error: looking up next.";
    },
    [Symbol.iterator]: function () {
        return this;
    },
    return: function () {
        returnCalled = true;
    }
};
try {
    for (var value of iter) {
        throw "Error: Iteration should not occur.";
    }
} catch (e) {
    if (String(e) !== "Error: looking up next.")
        throw e;
}
if (returnCalled)
    throw new Error("return was called");



var iter = {
    __key: 0,
    next: function () {
        return {
            done: this.__key === 42,
            value: this.__key++
        };
    },
    [Symbol.iterator]: function () {
        return this;
    },
    get return() {
        throw "Error: looking up return."
    }
};
try {
    for (var value of iter) {
        throw "Error: Terminate iteration.";
    }
} catch (e) {
    if (String(e) !== "Error: Terminate iteration.")
        throw e;
}



var returnCalled = false;
var iter = {
    __key: 0,
    next: function () {
        throw "Error: next is called."
    },
    [Symbol.iterator]: function () {
        return this;
    },
    return: function () {
        returnCalled = true;
        return {
            done: true,
            value: undefined
        };
    }
};

try {
    for (var value of iter) {
        throw "Error: Terminate iteration.";
    }
} catch (e) {
    if (String(e) !== "Error: next is called.")
        throw e;
}
if (returnCalled)
    throw new Error("return was called");



var returnCalled = false;
var iter = {
    __key: 0,
    next: function () {
        return { done: false, value: 42 };
    },
    [Symbol.iterator]: function () {
        return this;
    },
    return: function () {
        returnCalled = true;
        throw new Error("return was called");
    }
};

try {
    for (var value of iter) {
        throw "Error: Terminate iteration.";
    }
} catch (e) {
    if (String(e) !== "Error: Terminate iteration.")
        throw e;
}
if (!returnCalled)
    throw "Error: return is not called.";


var returnCalled = false;
var iter = {
    __key: 0,
    next: function () {
        return { done: false, value: 42 };
    },
    [Symbol.iterator]: function () {
        return this;
    },
    return: function () {
        returnCalled = true;
        throw new Error("return was called");
    }
};
try {
    for (var value of iter) {
        break;
    }
} catch (e) {
    if (String(e) !== "Error: return was called")
        throw e;
}
if (!returnCalled)
    throw "Error: return is not called.";


var primitives = [
    undefined,
    null,
    42,
    "string",
    true,
    Symbol("Cocoa")
];

function iteratorInterfaceErrorTest(notIteratorResult) {
    var returnCalled = false;
    var iter = {
        __key: 0,
        next: function () {
            return notIteratorResult;
        },
        [Symbol.iterator]: function () {
            return this;
        },
        return: function () {
            returnCalled = true;
            return undefined;
        }
    };
    try {
        for (var value of iter) {
            throw "Error: Iteration should not occur.";
        }
    } catch (e) {
        if (String(e) !== "TypeError: Iterator result interface is not an object.")
            throw e;
    }
    if (returnCalled)
        throw new Error("return was called");
}

function iteratorInterfaceErrorTestReturn(notIteratorResult) {
    var returnCalled = false;
    var iter = {
        __key: 0,
        next: function () {
            return { done: false, value: 42 };
        },
        [Symbol.iterator]: function () {
            return this;
        },
        return: function () {
            returnCalled = true;
            return notIteratorResult;
        }
    };
    try {
        for (var value of iter) {
            throw "Error: Terminate iteration.";
        }
    } catch (e) {
        if (String(e) !== "Error: Terminate iteration.")
            throw e;
    }
    if (!returnCalled)
        throw "Error: return is not called.";
}

primitives.forEach(iteratorInterfaceErrorTest);
primitives.forEach(iteratorInterfaceErrorTestReturn);


function iteratorInterfaceBreakTestReturn(notIteratorResult) {
    var returnCalled = false;
    var iter = {
        __key: 0,
        next: function () {
            return { done: false, value: 42 };
        },
        [Symbol.iterator]: function () {
            return this;
        },
        return: function () {
            returnCalled = true;
            return notIteratorResult;
        }
    };
    try {
        for (var value of iter) {
            break;
        }
    } catch (e) {
        if (String(e) !== "TypeError: Iterator result interface is not an object.")
            throw e;
    }
    if (!returnCalled)
        throw "Error: return is not called.";
}

primitives.forEach(iteratorInterfaceBreakTestReturn);
