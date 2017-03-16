let assert = (a) => {
    if (!a)
        throw new Error("Bad Assertion");
}

assert.sameValue = (a, b) =>  {
    assert(a === b);
}

function validatePropertyDescriptor(o, p) {
    let desc = Object.getOwnPropertyDescriptor(o, p);

    assert(desc.enumerable);
    assert(desc.configurable);
    assert(desc.writable);
}

// Base cases

(() => {
    let obj = {a: 1, b: 2, ...{c: 3, d: 4}};

    assert.sameValue(obj.a, 1);
    assert(obj.b, 2);
    assert(obj.c, 3);
    assert(obj.d, 4);
    validatePropertyDescriptor(obj, "c");
    validatePropertyDescriptor(obj, "d");
    assert(Object.keys(obj), 2);
})();

(() => {
    let o = {c: 3, d: 4};
    let obj = {a: 1, b: 2, ...o};

    assert.sameValue(obj.a, 1);
    assert.sameValue(obj.b, 2);
    assert.sameValue(obj.c, 3);
    assert.sameValue(obj.d, 4);
    assert.sameValue(Object.keys(obj).length, 4);

    validatePropertyDescriptor(obj, "a");
    validatePropertyDescriptor(obj, "b");
    validatePropertyDescriptor(obj, "c");
    validatePropertyDescriptor(obj, "d");
})();

(() => {
    let o = {a: 2, b: 3};
    let o2 = {c: 4, d: 5};

    let obj = {...o, ...o2};

    assert.sameValue(obj.a, 2);
    assert.sameValue(obj.b, 3);
    assert.sameValue(obj.c, 4);
    assert.sameValue(obj.d, 5);
    assert.sameValue(Object.keys(obj).length, 4);
})();

// Empty case

(() => {
    let obj = {a: 1, b: 2, ...{}};

    assert.sameValue(obj.a, 1);
    assert.sameValue(obj.b, 2);
    assert.sameValue(Object.keys(obj).length, 2);
})();

// Ignoring cases

(() => {
    let obj = {a: 1, ...null, b: 2, ...undefined, c: 3, ...{}, ...{...{}}, d: 4};

    assert.sameValue(obj.a, 1);
    assert.sameValue(obj.b, 2);
    assert.sameValue(obj.c, 3);
    assert.sameValue(obj.d, 4);

    let keys = Object.keys(obj);
    assert.sameValue(keys[0], "a");
    assert.sameValue(keys[1], "b");
    assert.sameValue(keys[2], "c");
    assert.sameValue(keys[3], "d");
})();

// Null case

(() => {
    let obj = {a: 1, b: 2, ...null};

    assert.sameValue(obj.a, 1);
    assert.sameValue(obj.b, 2);
    assert.sameValue(Object.keys(obj).length, 2);
})();

(() => {
    let obj = {...null};

    assert.sameValue(Object.keys(obj).length, 0);
})();

// Undefined case

(() => {
    let obj = {a: 1, b: 2, ...undefined};

    assert.sameValue(obj.a, 1);
    assert.sameValue(obj.b, 2);
    assert.sameValue(Object.keys(obj).length, 2);
})();

(() => {
    let obj = {...undefined};

    assert.sameValue(Object.keys(obj).length, 0);
})();

// Getter case

(() => {
    let o = {
        get a() {
            return 42;
        }
    };

    let obj = {...o, c: 4, d: 5};

    assert.sameValue(Object.getOwnPropertyDescriptor(obj, "a").value, 42);
    assert.sameValue(obj.c, 4);
    assert.sameValue(obj.d, 5);
    assert.sameValue(Object.keys(obj).length, 3);

    validatePropertyDescriptor(obj, "a");
})();

(() => {
    let o = {a: 2, b: 3}
    let executedGetter = false;

    let obj = {...o, get c() { executedGetter = true; }};

    assert.sameValue(obj.a, 2);
    assert.sameValue(obj.b, 3);
    assert.sameValue(executedGetter, false)
    assert.sameValue(Object.keys(obj).length, 3);
})();

(() => {
    let getterCallCount = 0;
    let o = {
        get a() {
            return ++getterCallCount;
        }
    };

    let obj = {...o, c: 4, d: 5, a: 42, ...o};

    assert.sameValue(obj.a, 2);
    assert.sameValue(obj.c, 4);
    assert.sameValue(obj.d, 5);
    assert.sameValue(Object.keys(obj).length, 3);
})();

// Manipulate Object case

(() => {
    var o = { a: 0, b: 1 };
    var cthulhu = { get x() {
      delete o.a;
      o.b = 42;
      o.c = "ni";
    }};

    let obj = {...cthulhu, ...o};

    assert.sameValue(obj.hasOwnProperty("a"), false);
    assert.sameValue(obj.b, 42);
    assert.sameValue(obj.c, "ni");
    assert(obj.hasOwnProperty("x"));
    assert.sameValue(Object.keys(obj).length, 3);
})();

// Override

(() => {
    let o = {a: 2, b: 3};

    let obj = {a: 1, b: 7, ...o};

    assert.sameValue(obj.a, 2);
    assert.sameValue(obj.b, 3);
    assert.sameValue(Object.keys(obj).length, 2);
    assert.sameValue(o.a, 2);
    assert.sameValue(o.b, 3);
})();

(() => {
    let o = {a: 2, b: 3, c: 4, e: undefined, f: null, g: false};

    let obj = {...o, a: 1, b: 7, d: 5, h: -0, i: Symbol("foo"), j: o};

    assert.sameValue(obj.a, 1);
    assert.sameValue(obj.b, 7);
    assert.sameValue(obj.c, 4);
    assert.sameValue(obj.d, 5);
    assert(obj.hasOwnProperty("e"));
    assert.sameValue(obj.f, null);
    assert.sameValue(obj.g, false);
    assert.sameValue(obj.h, -0);
    assert.sameValue(obj.i.toString(), "Symbol(foo)");
    assert(Object.is(obj.j, o));
    assert.sameValue(Object.keys(obj).length, 10);
})();

// Override Immutable

(() => {
    let o = {b: 2};
    Object.defineProperty(o, "a", {value: 1, enumerable: true, writable: false, configurable: true});

    let obj = {...o, a: 3};

    assert.sameValue(obj.a, 3)
    assert.sameValue(obj.b, 2);
    validatePropertyDescriptor(obj, "a");
    validatePropertyDescriptor(obj, "b");
})();

// Setter

(() => {
    let executedSetter = false;

    let obj = {set c(v) { executedSetter = true; }, ...{c: 1}};

    assert.sameValue(obj.c, 1);
    assert.sameValue(executedSetter, false);
    assert.sameValue(Object.keys(obj).length, 1);
})();

// Skip non-enumerble

(() => {
    let o = {};
    Object.defineProperty(o, "b", {value: 3, enumerable: false});

    let obj = {...o};

    assert.sameValue(obj.hasOwnProperty("b"), false)
    assert.sameValue(Object.keys(obj).length, 0);
})();

// Spread order

(() => {
    var calls = []
    var o = { get z() { calls.push('z') }, get a() { calls.push('a') } };
    Object.defineProperty(o, 1, { get: () => { calls.push(1) }, enumerable: true });
    Object.defineProperty(o, Symbol('foo'), { get: () => { calls.push("Symbol(foo)") }, enumerable: true });

    let obj = {...o};

    assert.sameValue(calls[0], 1);
    assert.sameValue(calls[1], "z");
    assert.sameValue(calls[2], "a");
    assert.sameValue(calls[3], "Symbol(foo)");
    assert.sameValue(Object.keys(obj).length, 3);
})();

// Symbol property
(() => {
    let symbol = Symbol('foo');
    let o = {};
    o[symbol] = 1;

    let obj = {...o, c: 4, d: 5};

    assert.sameValue(obj[symbol], 1);
    assert.sameValue(obj.c, 4);
    assert.sameValue(obj.d, 5);
    assert.sameValue(Object.keys(obj).length, 2);
})();

// Getter throw

(() => {
    try {
        let obj = {...{ get foo() { throw new Error("Getter Exception"); } }};
        assert(false);
    } catch(e) {
        assert.sameValue(e.message, "Getter Exception");
    }
})();

