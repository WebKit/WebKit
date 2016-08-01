function assert(b) {
    if (!b)
        throw new Error("bad");
}

function test(f) {
    for (let i = 0; i < 1000; i++)
        f();
}

test(function() {
    function foo(a, b) {
        assert(arguments[0] === a);
        assert(arguments[1] === b);
        assert(arguments.length === 2);
        arguments[0] = "hello";
        arguments[1] = "world";
        assert(a === "hello");
        assert(b === "world");
    }
    foo(null, null);
});

test(function() {
    function foo(a, b) {
        assert(arguments[0] === a);
        assert(arguments[1] === b);
        assert(arguments.length === 2);
        a = "hello";
        b = "world";
        assert(arguments[0] === "hello");
        assert(arguments[1] === "world");
    }
    foo(null, null);
});

test(function() {
    function foo(a, b, ...rest) {
        assert(arguments[0] === a);
        assert(arguments[1] === b);
        assert(arguments.length === 2);
        arguments[0] = "hello";
        arguments[1] = "world";
        assert(a === null);
        assert(b === null);
    }
    foo(null, null);
});

test(function() {
    function foo(a, b, ...rest) {
        assert(arguments[0] === a);
        assert(arguments[1] === b);
        assert(arguments.length === 2);
        a = "hello";
        b = "world";
        assert(arguments[0] === null);
        assert(arguments[1] === null);
    }
    foo(null, null);
});

test(function() {
    function foo(a, b, {destructure}) {
        assert(arguments[0] === a);
        assert(arguments[1] === b);
        assert(arguments.length === 3);
        arguments[0] = "hello";
        arguments[1] = "world";
        assert(a === null);
        assert(b === null);
    }
    foo(null, null, {});
});

test(function() {
    function foo(a, b, {destructure}) {
        assert(arguments[0] === a);
        assert(arguments[1] === b);
        assert(arguments.length === 3);
        a = "hello";
        b = "world";
        assert(arguments[0] === null);
        assert(arguments[1] === null);
    }
    foo(null, null, {});
});

test(function() {
    function foo(a, b, defaultParam = 20) {
        assert(arguments[0] === a);
        assert(arguments[1] === b);
        assert(arguments.length === 3);
        arguments[0] = "hello";
        arguments[1] = "world";
        assert(a === null);
        assert(b === null);
    }
    foo(null, null, {});
});

test(function() {
    function foo(a, b, defaultParam = 20) {
        assert(arguments[0] === a);
        assert(arguments[1] === b);
        assert(arguments.length === 3);
        a = "hello";
        b = "world";
        assert(arguments[0] === null);
        assert(arguments[1] === null);
    }
    foo(null, null, {});
});

test(function() {
    let obj = {}
    function foo(a, b, {foo = b}) {
        assert(foo === b);
        assert(foo === obj);
    }
    foo(null, obj, {});
});

test(function() {
    let obj = {}
    function foo(a, b, {foo = b}) {
        assert(foo === b);
        assert(foo === obj);
        function capB() { return b; }
    }
    foo(null, obj, {});
});

test(function() {
    let obj = {}
    function foo(a, b, {foo = b}) {
        assert(foo === 25);
    }
    foo(null, obj, {foo: 25});
});

test(function() {
    let obj = {}
    function foo(a, b, {foo = function() { return b; }}) {
        assert(foo() === b);
        assert(foo() === obj);
        return foo;
    }
    let result = foo(null, obj, {});
    assert(result() === obj);
});

test(function() {
    let obj = {}
    function foo(a, b, [foo = function() { return b; }]) {
        assert(foo() === b);
        assert(foo() === obj);
        return foo;
    }
    let result = foo(null, obj, [undefined]);
    assert(result() === obj);
});

test(function() {
    let obj = {}
    function foo(a, b, [foo = function() { return e; }], {d = foo()}, e) { }
    foo(null, obj, [], {d:null}, 20);
});

test(function() {
    let obj = {}
    function foo(a, b, [foo = function() { return e; }], {d = foo()}, e) { }
    try {
        foo(null, obj, [], {}, 20);
    } catch(e) {
        assert(e.toString() === "ReferenceError: Cannot access uninitialized variable.");
    }
});

test(function() {
    let obj = {}
    function foo(a, b, [foo = function() { return e; }], e, {d = foo()}) { 
        return d;
    }
    assert(foo(null, obj, [], 20, {}) === 20);
});

test(function() {
    let obj = {}
    function foo(a, b, [foo = function() { return e; }], e, {d = foo()}) { 
        var d;
        assert(d === 20);
        return d;
    }
    assert(foo(null, obj, [], 20, {}) === 20);
});

test(function() {
    function foo(b, {a = function() { return b; }}) { 
        var b = 25;
        assert(b === 25);
        assert(a() === 20);
    }
    foo(20, {});
});

test(function() {
    function foo(b, {a = function() { return typeof inner; }}) { 
        let inner = 25;
        assert(inner === 25);
        assert(a() === "undefined");
    }
    foo(20, {});
});

test(function() {
    let obj = {};
    let inner = obj;
    function foo(b, {a = function() { return inner; }}) { 
        let inner = 25;
        assert(inner === 25);
        assert(a() === obj);
    }
    foo(20, {});
});

test(function() {
    let obj = {};
    let inner = obj;
    let foo = (b, {a = function() { return inner; }}) => {
        let inner = 25;
        assert(inner === 25);
        assert(a() === obj);
    }
    foo(20, {});
});
