//@ requireOptions("--useErrorPrototypeStackAccessor=true")
"use strict";

function assert(b, msg = "") {
    if (!b)
        throw new Error("Bad assertion: " + msg);
}

function assertThrows(fn, ctor, msg = "") {
    let threw = null;
    try { fn(); } catch (e) { threw = e; }
    assert(threw instanceof ctor, "expected " + ctor.name + " " + msg);
}

// 1. Error.prototype has the accessor; instances do not have an own "stack".
{
    let desc = Object.getOwnPropertyDescriptor(Error.prototype, "stack");
    assert(desc !== undefined, "Error.prototype.stack descriptor exists");
    assert(typeof desc.get === "function", "getter is a function");
    assert(typeof desc.set === "function", "setter is a function");
    assert(desc.enumerable === false, "non-enumerable");
    assert(desc.configurable === true, "configurable");

    let e = new Error("msg");
    assert(!Object.prototype.hasOwnProperty.call(e, "stack"), "no own stack on fresh Error before access");
    void e.stack; // trigger the accessor; should still not install own property
    assert(!Object.prototype.hasOwnProperty.call(e, "stack"), "no own stack on fresh Error after access");
}

// 2. Getter returns a string for ErrorInstance, undefined for non-error objects.
{
    let e = new Error("msg");
    assert(typeof e.stack === "string", "ErrorInstance returns string");
    assert(e.stack.length > 0, "non-empty stack");

    let plain = Object.create(Error.prototype);
    assert(plain.stack === undefined, "object inheriting Error.prototype but with no [[ErrorData]] returns undefined");

    // The prototype itself returns undefined (no [[ErrorData]]).
    assert(Error.prototype.stack === undefined, "Error.prototype.stack returns undefined");
    assert(TypeError.prototype.stack === undefined, "TypeError.prototype.stack returns undefined (inherits accessor)");
}

// 3. Getter throws TypeError when called on a non-object.
{
    let getter = Object.getOwnPropertyDescriptor(Error.prototype, "stack").get;
    assertThrows(() => getter.call(undefined), TypeError, "undefined this");
    assertThrows(() => getter.call(null), TypeError, "null this");
    assertThrows(() => getter.call(5), TypeError, "number this");
    assertThrows(() => getter.call("s"), TypeError, "string this");
}

// 4. Setter throws TypeError on non-object this and non-string value.
{
    let setter = Object.getOwnPropertyDescriptor(Error.prototype, "stack").set;
    assertThrows(() => setter.call(undefined, "x"), TypeError, "undefined this");
    assertThrows(() => setter.call(null, "x"), TypeError, "null this");
    assertThrows(() => setter.call(5, "x"), TypeError, "primitive this");

    assertThrows(() => setter.call({}, 999), TypeError, "non-string number");
    assertThrows(() => setter.call({}, null), TypeError, "non-string null");
    assertThrows(() => setter.call({}, undefined), TypeError, "non-string undefined");
    assertThrows(() => setter.call({}, {}), TypeError, "non-string object");
    assertThrows(() => setter.call({}, Symbol()), TypeError, "non-string symbol");
}

// 5. Setter creates own data property on receiver via SetterThatIgnoresPrototypeProperties.
{
    let e = new Error();
    e.stack = "mystack";
    let desc = Object.getOwnPropertyDescriptor(e, "stack");
    assert(desc !== undefined, "own descriptor exists after setter");
    assert(desc.value === "mystack", "value is mystack");
    assert(desc.writable === true, "writable");
    assert(desc.enumerable === true, "enumerable (createDataProperty default)");
    assert(desc.configurable === true, "configurable");
    assert(e.stack === "mystack", "read returns own value");

    // Second write updates the existing own property.
    e.stack = "second";
    assert(e.stack === "second");
    let desc2 = Object.getOwnPropertyDescriptor(e, "stack");
    assert(desc2.value === "second");
}

// 6. Setter throws when this === Error.prototype itself.
{
    let setter = Object.getOwnPropertyDescriptor(Error.prototype, "stack").set;
    assertThrows(() => setter.call(Error.prototype, "x"), TypeError, "this === %Error.prototype% throws");
}

// 7. All native errors and AggregateError go through the same accessor.
{
    for (let C of [TypeError, RangeError, SyntaxError, ReferenceError, EvalError, URIError]) {
        let e = new C("m");
        assert(typeof e.stack === "string", C.name + " stack is string");
        assert(!Object.prototype.hasOwnProperty.call(e, "stack"), C.name + " has no own stack");
    }
    let agg = new AggregateError([new Error("a")], "b");
    assert(typeof agg.stack === "string", "AggregateError stack");
    assert(!Object.prototype.hasOwnProperty.call(agg, "stack"), "AggregateError no own stack");
}

// 8. Error.captureStackTrace continues to install an own "stack" property (V8 compat).
{
    let target = {};
    Error.captureStackTrace(target);
    assert(Object.prototype.hasOwnProperty.call(target, "stack"), "captureStackTrace installs own");
    assert(typeof target.stack === "string");
}

// 9. After captureStackTrace on an ErrorInstance, the own property shadows the accessor.
{
    let e = new Error();
    Error.captureStackTrace(e);
    let ownDesc = Object.getOwnPropertyDescriptor(e, "stack");
    assert(ownDesc !== undefined && "value" in ownDesc, "own data property exists");
    assert(typeof e.stack === "string", "stack reads return string");
}

// 10. Hot path: many reads through the accessor return content of the same length.
{
    let e = new Error("hot");
    let s1 = e.stack;
    for (let i = 0; i < 10000; i++) {
        let s = e.stack;
        assert(typeof s === "string");
        assert(s.length === s1.length, "stable length under repeated reads");
    }
}

// 11. line, column, sourceURL still install as own properties when stack is read.
{
    let e = new Error("trigger");
    void e.stack; // triggers materialization
    // line/column/sourceURL are JSC extensions installed as own properties on access.
    assert(Object.prototype.hasOwnProperty.call(e, "line"));
    assert(Object.prototype.hasOwnProperty.call(e, "column"));
}

// 12. Object.getOwnPropertyNames does not include "stack" for fresh errors.
{
    let e = new Error("names");
    let names = Object.getOwnPropertyNames(e);
    assert(!names.includes("stack"), "stack is not an own property name");
}

// 13. Setter accepts only String, including primitive strings (not String objects).
{
    let e = new Error();
    e.stack = "primString";
    assert(e.stack === "primString");

    let setter = Object.getOwnPropertyDescriptor(Error.prototype, "stack").set;
    assertThrows(() => setter.call({}, new String("boxed")), TypeError, "boxed String objects rejected");
}

// 14. delete e.stack works (no own property to delete returns true; configurable accessor).
{
    let e = new Error();
    assert(delete e.stack === true, "delete on instance is true (no own property to delete)");
    e.stack = "x";
    assert(delete e.stack === true, "delete on installed own property succeeds");
    assert(!Object.prototype.hasOwnProperty.call(e, "stack"));
}
