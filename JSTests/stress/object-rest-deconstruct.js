let assert = (e) => {
    if (!e)
        throw Error("Bad assertion!");
}

let assertPropDescriptor = (restObj, prop) => {
    let desc = Object.getOwnPropertyDescriptor(restObj, prop);
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

    assert(rest.x === 1);
    assert(rest.y === 2);

    assertPropDescriptor(rest, 'x');
    assertPropDescriptor(rest, 'y');
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
        assert(e.message == "Right side of assignment cannot be destructured");
    }

})();

// undefined case
(() => {
    let obj = undefined;

    try {
        let {...rest} = obj;
        assert(false);
    } catch (e) {
        assert(e.message == "Right side of assignment cannot be destructured");
    }

})();

// getter case
(() => {
    let obj = {a: 3, b: 4};
    Object.defineProperty(obj, "x", { get: () => 3, enumerable: true });

    let {a, b, ...rest} = obj;

    assert(a === 3);
    assert(b === 4);

    assert(rest.x === 3);
    assertPropDescriptor(rest, 'x');
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

    assertPropDescriptor(rest, 'a');
    assertPropDescriptor(rest, 'b');
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

