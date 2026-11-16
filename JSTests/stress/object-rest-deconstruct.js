let assert = (e) => {
    if (!e)
        throw Error("Bad assertion!");
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function assertDataProperty(restObj, prop, value) {
    shouldBe(restObj[prop], value);
    let desc = Object.getOwnPropertyDescriptor(restObj, prop);
    shouldBe(desc.value, value);
    assert(desc.enumerable);
    assert(desc.writable);
    assert(desc.configurable);
}

// Base Case
(() => {
    let obj = {x: 1, y: 2, a: 5, b: 3}

    let {a, b, ...rest} = obj;

    assert(a === 5);
    assert(b === 3);

    assertDataProperty(rest, 'x', 1);
    assertDataProperty(rest, 'y', 2);
})();

// Empty Object
(() => {
    let obj = {}

    let {a, b, ...rest} = obj;

    assert(a === undefined);
    assert(b === undefined);

    assert(typeof rest === "object");
})();

// Number case
(() => {
    let obj = 3;

    let {...rest} = obj;

    assert(typeof rest === "object");
})();

// String case
(() => {
    let obj = "foo";

    let {...rest} = obj;

    assert(typeof rest === "object");
})();

// Symbol case
(() => {
    let obj = Symbol("foo");

    let {...rest} = obj;

    assert(typeof rest === "object");
})();

// null case
(() => {
    let obj = null;

    try {
        let {...rest} = obj;
        assert(false);
    } catch (e) {
        assert(e.message == "Cannot destructure null or undefined value");
    }

})();

// undefined case
(() => {
    let obj = undefined;

    try {
        let {...rest} = obj;
        assert(false);
    } catch (e) {
        assert(e.message == "Cannot destructure null or undefined value");
    }

})();

// getter case
(() => {
    let obj = {a: 3, b: 4};
    Object.defineProperty(obj, "x", { get: () => 3, enumerable: true });

    let {a, b, ...rest} = obj;

    assert(a === 3);
    assert(b === 4);

    assertDataProperty(rest, 'x', 3);
})();

// Skip non-enumerable case
(() => {
    let obj = {a: 3, b: 4};
    Object.defineProperty(obj, "x", { value: 4, enumerable: false });

    let {...rest} = obj;

    assert(rest.a === 3);
    assert(rest.b === 4);
    assert(rest.x === undefined);
})();

// Don't copy descriptor case
(() => {
    let obj = {};
    Object.defineProperty(obj, "a", { value: 3, configurable: false, enumerable: true });
    Object.defineProperty(obj, "b", { value: 4, writable: false, enumerable: true });

    let {...rest} = obj;

    assert(rest.a === 3);
    assert(rest.b === 4);

    assertDataProperty(rest, 'a', 3);
    assertDataProperty(rest, 'b', 4);
})();

// Destructuring function parameter

(() => {

    var o = { x: 1, y: 2, w: 3, z: 4 };
    
    function foo({ x, y, ...rest }) {
        assert(x === 1);
        assert(y === 2);
        assert(rest.w === 3);
        assert(rest.z === 4);
    }
    foo(o);
})();

// Destructuring arrow function parameter

(() => {

    var o = { x: 1, y: 2, w: 3, z: 4 };
    
    (({ x, y, ...rest }) => {
        assert(x === 1);
        assert(y === 2);
        assert(rest.w === 3);
        assert(rest.z === 4);
    })(o);
})();

// Destructuring to a property
(() => {

    var o = { x: 1, y: 2};
    
    let settedValue;
    let src = {};
    ({...src.y} = o);
    assert(src.y.x === 1);
    assert(src.y.y === 2);
})();

// Destructuring with setter
(() => {

    var o = { x: 1, y: 2};
    
    let settedValue;
    let src = {
        get y() { throw Error("The property should not be accessed"); },
        set y(v) {
            settedValue = v;
        }
    }
    src.y = undefined;
    ({...src.y} = o);
    assert(settedValue.x === 1);
    assert(settedValue.y === 2);
})();

// Destructuring computed property
(() => {

    var a = "foo";
    
    var {[a]: b, ...r} = {foo: 1, bar: 2, baz: 3};
    assert(b === 1);
    assert(r.bar === 2);
    assert(r.baz === 3);
})();

// Destructuring non-string computed property
(() => {
    var a = 1;
    var {[a]: b, ...r} = {[a]: 1, b: 2, c: 3};
    assert(b === 1);
    assert(r[1] === undefined);
    assert(r.b === 2);
    assert(r.c === 3);
})();

// Destructuring Symbol computed property
(() => {
    var a = Symbol('a');
    var b = Symbol('a');
    var {[a]: c, ...r} = {[b]: 1, b: 2, c: 3};
    assert(c === undefined);
    assert(r[b] === 1);
    assert(r.b === 2);
    assert(r.c === 3);
})();

// Catch case

(() => {
    try {
        throw {foo: 1, bar: 2, baz: 3};
    } catch({foo, ...rest}) {
        assert(foo === 1);
        assert(rest.bar === 2);
        assert(rest.baz === 3);
    }
})();

(function nonEnumerableSymbol() {
    var __s0 = Symbol("s0");
    var __s1 = Symbol("s1");

    var source = {};
    Object.defineProperties(source, {
        [__s0]: {value: 0, enumerable: false},
        [__s1]: {value: 1, enumerable: false},
    });

    var {[__s0]: s0, ...target} = source;

    shouldBe(s0, 0);
    assert(!Object.getOwnPropertySymbols(target).length);
})();

(function dunderProto() {
    var source = {};
    var protoValue = {};
    Object.defineProperty(source, "__proto__", {value: protoValue, enumerable: true});
    var {...target} = source;

    assertDataProperty(target, "__proto__", protoValue);
    shouldBe(Object.getPrototypeOf(target), Object.prototype);
})();

(function stringPrimitive() {
    var source = "012";
    var {0: a, ["2"]: c, ...target} = source;

    shouldBe(a, "0");
    shouldBe(c, "2");
    assertDataProperty(target, 1, "1");
    shouldBe(Object.keys(target).join(), "1");
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

    var {c, ["d"]: d, ...target} = source;

    shouldBe(ownKeysCalls, 1);
    shouldBe(gopdCalls.map(String).join(), "a,b,Symbol(s0),Symbol(s1)");
    shouldBe(getCalls.map(String).join(), "c,d,a,Symbol(s1)");

    assertDataProperty(target, "a", 0);
    shouldBe(c, 2);
    shouldBe(d, 3);
    shouldBe(Object.keys(target).join(), "a");

    var symbols = Object.getOwnPropertySymbols(target);
    shouldBe(symbols[0], __s1);
    shouldBe(symbols.length, 1);
})();

(function indexedProperties() {
    var source = [0, 1, 2, 3];
    Object.defineProperty(source, "1", {enumerable: false});
    var {2: c, ["3"]: d, ...target} = source;

    assertDataProperty(target, "0", 0);
    shouldBe(Object.keys(target).join(), "0");
})();

(function CustomAccessor() {
    var source = $vm.createCustomTestGetterSetter();
    var {customValueGlobalObject, ...target} = source;

    assertDataProperty(target, "customValue", source);
    assert(!target.hasOwnProperty("customValueGlobalObject"));
    shouldBe(customValueGlobalObject[Symbol.toStringTag], "global");
    assertDataProperty(target, "customAccessor", source);
    assertDataProperty(target, "customAccessorGlobalObject", customValueGlobalObject);
})();

(function CustomAccessorInNonReifiedPropertyTable() {
    // make sure we reifyAllStaticProperties() before deciding on the fast/slow path

    var source = $vm.createStaticCustomAccessor();
    source.testField = 42;
    var {...target} = source;

    assertDataProperty(target, "testField", 42);
    assertDataProperty(target, "testStaticAccessor", 42);
})();

(function uncacheableDictionary() {
    var source = {a: 0, b: 1, c: 2, d: 3};
    Object.defineProperty(source, "c", {enumerable: false});
    $vm.toUncacheableDictionary(source);
    var {b, ["d"]: d, ...target} = source;

    assertDataProperty(target, "a", 0);
    shouldBe(b, 1);
    shouldBe(d, 3);
    shouldBe(Object.keys(target).join(), "a");
})();
