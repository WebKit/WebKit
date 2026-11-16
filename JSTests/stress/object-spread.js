let assert = (a) => {
    if (!a)
        throw new Error("Bad Assertion");
}

assert.sameValue = (a, b) =>  {
    assert(a === b);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function assertDataProperty(target, prop, value) {
    shouldBe(target[prop], value);
    let desc = Object.getOwnPropertyDescriptor(target, prop);
    shouldBe(desc.value, value);
    assert(desc.enumerable);
    assert(desc.writable);
    assert(desc.configurable);
}

// Base cases

(() => {
    let obj = {a: 1, b: 2, ...{c: 3, d: 4}};

    assert.sameValue(obj.a, 1);
    assert(obj.b, 2);
    assertDataProperty(obj, "c", 3);
    assertDataProperty(obj, "d", 4);
    assert(Object.keys(obj), 2);
})();

(() => {
    let o = {c: 3, d: 4};
    let obj = {a: 1, b: 2, ...o};

    assertDataProperty(obj, "a", 1);
    assertDataProperty(obj, "b", 2);
    assertDataProperty(obj, "c", 3);
    assertDataProperty(obj, "d", 4);
    assert.sameValue(Object.keys(obj).length, 4);
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

    assertDataProperty(obj, "a", 42);
    assert.sameValue(obj.c, 4);
    assert.sameValue(obj.d, 5);
    assert.sameValue(Object.keys(obj).length, 3);
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

    assertDataProperty(obj, "a", 3);
    assertDataProperty(obj, "b", 2);
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

// Spread overrides properties

(() => {
    var calls = []
    var o = { a: 1, b: 2 };

    let executedGetter = false;
    let executedSetter = false
    let obj = {get a() {executedGetter = true; return this_a;}, ...o, set a(v) { executedSetter = true; this._a = v}};

    obj.a = 3
    assert.sameValue(obj.a, undefined);
    assert(!executedGetter);
    assert(executedSetter);
})();

(function nonEnumerableSymbol() {
    var __s0 = Symbol("s0");
    var source = {};
    Object.defineProperties(source, {
        [__s0]: {value: 0, enumerable: false},
    });

    var target = {[__s0]: 1, ...target};
    assertDataProperty(target, __s0, 1);
})();

(function dunderProto() {
    var source = {};
    var protoValue = {};
    Object.defineProperty(source, "__proto__", {value: protoValue, enumerable: true});
    var target = {...source};

    assertDataProperty(target, "__proto__", protoValue);
    shouldBe(Object.getPrototypeOf(target), Object.prototype);
})();

(function stringPrimitive() {
    var source = "012";
    var target = {get 0() {}, ["2"]: 22, ...source};

    assertDataProperty(target, 0, "0");
    assertDataProperty(target, 1, "1");
    assertDataProperty(target, 2, "2");
    shouldBe(Object.keys(target).join(), "0,1,2");
})();

(function ProxyObject() {
    var __s0 = Symbol("s0");
    var __s1 = Symbol("s1");

    var ownKeysCalls = 0;
    var gopdCalls = [];
    var getCalls = [];

    var source = new Proxy({
        [__s0]: "s0", [__s1]: "s1",
        a: 0, b: 1, c: 2, d: 3,
    }, {
        ownKeys: (t) => {
            ++ownKeysCalls;
            return Reflect.ownKeys(t);
        },
        getOwnPropertyDescriptor: (t, key) => {
            gopdCalls.push(key);
            var desc = Reflect.getOwnPropertyDescriptor(t, key);
            if (key === "b" || key === __s0)
                desc.enumerable = false;
            return desc;
        },
        get: (t, key, receiver) => {
            getCalls.push(key);
            return Reflect.get(t, key, receiver);
        },
    });

    var target = {a() {}, b: 11, [__s1]: "foo", ...source, ["d"]: 33};

    shouldBe(ownKeysCalls, 1);
    shouldBe(gopdCalls.map(String).join(), "a,b,c,d,Symbol(s0),Symbol(s1)");
    shouldBe(getCalls.map(String).join(), "a,c,d,Symbol(s1)");

    assertDataProperty(target, "a", 0);
    assertDataProperty(target, "b", 11);
    assertDataProperty(target, "c", 2);
    assertDataProperty(target, "d", 33);

    shouldBe(Reflect.ownKeys(target).map(String).join(), "a,b,c,d,Symbol(s1)");
})();

(function indexedProperties() {
    var source = [0, 1, 2];
    Object.defineProperty(source, "1", {enumerable: false});
    var target = {set ["2"](_v) {}, ...source};

    assertDataProperty(target, "0", 0);
    assertDataProperty(target, "2", 2);
    shouldBe(Object.keys(target).join(), "0,2");
})();

(function CustomAccessor() {
    var source = $vm.createCustomTestGetterSetter();
    var target = {...source};

    assertDataProperty(target, "customValue", source);
    shouldBe(target.customValueGlobalObject, target.customAccessorGlobalObject);
    shouldBe(target.customValueGlobalObject[Symbol.toStringTag], "global");
    assertDataProperty(target, "customAccessor", source);
})();

(function CustomAccessorInNonReifiedPropertyTable() {
    // make sure we reifyAllStaticProperties() before deciding on the fast/slow path

    var source = $vm.createStaticCustomAccessor();
    source.testField = 42;
    var target = {...source};

    assertDataProperty(target, "testField", 42);
    assertDataProperty(target, "testStaticAccessor", 42);
})();

(function uncacheableDictionary() {
    var source = {a: 0, b: 1, c: 2};
    Object.defineProperty(source, "c", {enumerable: false});
    $vm.toUncacheableDictionary(source);
    var b = 11;
    var target = {get a() {}, b, ...source};

    assertDataProperty(target, "a", 0);
    assertDataProperty(target, "b", 1);
    shouldBe(Object.keys(target).join(), "a,b");
})();
