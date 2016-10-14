
var propertyKey = {
    toString() {
        throw new Error("propertyKey.toString is called.");
    }
};

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

// GetByVal.
(function () {
    var called = null;
    var toStringCalled = false;
    var property = {
        toString() {
            toStringCalled = true;
            return "value";
        }
    };
    var object = {
        get value() {
        },
        set value(val) {
        }
    };
    Object.defineProperty(object, '', {
        get: function () {
            called = "getter for '' is called.";
        }
    });

    function test(object, property) {
        object[property];
    }
    noInline(test);

    shouldThrow(function () { test(object, propertyKey); }, "Error: propertyKey.toString is called.");
    if (called)
        throw new Error(called);
    toStringCalled = false;
    shouldThrow(function () { test(null, propertyKey); }, "TypeError: null is not an object (evaluating 'object[property]')");
    if (toStringCalled)
        throw new Error("toString is called.");
}());

// GetByVal DFG.
(function () {
    var called = null;
    var toStringCalled = false;
    var property = {
        toString() {
            toStringCalled = true;
            return "value";
        }
    };
    var object = {
        get ""() {
            called = "getter for '' is called.";
        },
        set ""(val) {
            called = "setter for '' is called.";
        },

        get value() {
        },
        set value(val) {
        }
    };

    function test(object, property) {
        object[property];
    }
    noInline(test);

    for (var i = 0; i < 10000; ++i)
        test(object, property);

    shouldThrow(function () { test(object, propertyKey); }, "Error: propertyKey.toString is called.");
    if (called)
        throw new Error(called);
    toStringCalled = false;
    shouldThrow(function () { test(null, propertyKey); }, "TypeError: null is not an object (evaluating 'object[property]')");
    if (toStringCalled)
        throw new Error("toString is called.");
}());


// GetByValString.
(function () {
    var called;
    var toStringCalled = false;
    var property = {
        toString() {
            toStringCalled = true;
            return "value";
        }
    };
    function test(array, length, property) {
        var result = 0;
        for (var i = 0; i < length; ++i)
            result += array[property];
        return result;
    }
    noInline(test);

    Object.defineProperty(String.prototype, "", {
        get: function () {
            called = "getter for '' is called.";
        }
    });

    var array = [1, 2, 3];
    for (var i = 0; i < 100000; ++i)
        test(array, array.length, 0);

    var array = [1, false, 3];
    for (var i = 0; i < 100000; ++i)
        test(array, array.length, 1);

    test("hello", "hello".length, 2);
    shouldThrow(function () { test("hello", "hello".length, propertyKey); }, "Error: propertyKey.toString is called.");
    if (called)
        throw new Error(called);
    toStringCalled = false;
    shouldThrow(function () { test(null, 20, propertyKey); }, "TypeError: null is not an object (evaluating 'array[property]')");
    if (toStringCalled)
        throw new Error("toString is called.");
}());
