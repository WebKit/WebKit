// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

import * as chai from "chai";
const assert = chai.assert;
const expect = chai.expect;
const AssertionError = chai.AssertionError;

const tests = [];

const describe = (name, func) => func();
const it = (name, func) => tests.push({ name, func });

describe("assert", () => {
  it("assert", () => {
    const foo = "bar";
    assert(foo == "bar", "expected foo to equal `bar`");

    expect(() => {
      assert(foo == "baz", "expected foo to equal `baz`");
    }).to.throw(AssertionError, "expected foo to equal `baz`");

    expect(() => {
      assert(foo == "baz", () => "expected foo to equal `baz`");
    }).to.throw(AssertionError, "expected foo to equal `baz`");
  });

  it("fail", () => {
    expect(() => {
      assert.fail(0, 1, "this has failed");
    }).to.throw(AssertionError, "this has failed");
  });

  it("isTrue", () => {
    assert.isTrue(true);

    expect(() => {
      assert.isTrue(false, "blah");
    }).to.throw(AssertionError, "blah: expected false to be true");

    expect(() => {
      assert.isTrue(1);
    }).to.throw(AssertionError, "expected 1 to be true");

    expect(() => {
      assert.isTrue("test");
    }).to.throw(AssertionError, "expected 'test' to be true");
  });

  it("isNotTrue", () => {
    assert.isNotTrue(false);

    expect(() => {
      assert.isNotTrue(true, "blah");
    }).to.throw(AssertionError, "blah: expected true to not equal true");
  });

  it("isOk / ok", () => {
    ["isOk", "ok"].forEach((isOk) => {
      assert[isOk](true);
      assert[isOk](1);
      assert[isOk]("test");

      expect(() => {
        assert[isOk](false, "blah");
      }).to.throw(AssertionError, "blah: expected false to be truthy");

      expect(() => {
        assert[isOk](0);
      }).to.throw(AssertionError, "expected +0 to be truthy");

      expect(() => {
        assert[isOk]("");
      }).to.throw(AssertionError, "expected '' to be truthy");
    });
  });

  it("isNotOk / notOk", () => {
    ["isNotOk", "notOk"].forEach((isNotOk) => {
      assert[isNotOk](false);
      assert[isNotOk](0);
      assert[isNotOk]("");

      expect(() => {
        assert[isNotOk](true, "blah");
      }).to.throw(AssertionError, "blah: expected true to be falsy");

      expect(() => {
        assert[isNotOk](1);
      }).to.throw(AssertionError, "expected 1 to be falsy");

      expect(() => {
        assert[isNotOk]("test");
      }).to.throw(AssertionError, "expected 'test' to be falsy");
    });
  });

  it("isFalse", () => {
    assert.isFalse(false);

    expect(() => {
      assert.isFalse(true, "blah");
    }).to.throw(AssertionError, "blah: expected true to be false");

    expect(() => {
      assert.isFalse(0);
    }).to.throw(AssertionError, "expected +0 to be false");
  });

  it("isNotFalse", () => {
    assert.isNotFalse(true);

    expect(() => {
      assert.isNotFalse(false, "blah");
    }).to.throw(AssertionError, "blah: expected false to not equal false");
  });

  const sym = Symbol();

  it("isEqual", () => {
    assert.equal(0, 0);
    assert.equal(sym, sym);
    assert.equal("test", "test");
    assert.equal(void 0, null);
    assert.equal(void 0, undefined);

    expect(() => {
      assert.equal(NaN, NaN);
    }).to.throw(AssertionError, "expected NaN to equal NaN");

    expect(() => {
      assert.equal(1, 2, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to equal 2");
  });

  it("notEqual", () => {
    assert.notEqual(1, 2);
    assert.notEqual(NaN, NaN);
    assert.notEqual(1, "test");

    expect(() => {
      assert.notEqual("test", "test");
    }).to.throw(AssertionError, "expected 'test' to not equal 'test'");
    expect(() => {
      assert.notEqual(sym, sym);
    }).to.throw(AssertionError, "expected Symbol() to not equal Symbol()");
  });

  it("strictEqual", () => {
    assert.strictEqual(0, 0);
    assert.strictEqual(0, -0);
    assert.strictEqual("foo", "foo");
    assert.strictEqual(sym, sym);

    expect(() => {
      assert.strictEqual("5", 5, "blah");
    }).to.throw(AssertionError, "blah: expected '5' to equal 5");
  });

  it("notStrictEqual", () => {
    assert.notStrictEqual(5, "5");
    assert.notStrictEqual(NaN, NaN);
    assert.notStrictEqual(Symbol(), Symbol());

    expect(() => {
      assert.notStrictEqual(5, 5, "blah");
    }).to.throw(AssertionError, "blah: expected 5 to not equal 5");
  });

  it("deepEqual", () => {
    const obja = Object.create({ tea: "chai" });
    const objb = Object.create({ tea: "chai" });

    assert.deepEqual(/a/, /a/);
    assert.deepEqual(/a/g, /a/g);
    assert.deepEqual(/a/i, /a/i);
    assert.deepEqual(/a/m, /a/m);
    assert.deepEqual(obja, objb);
    assert.deepEqual([NaN], [NaN]);
    assert.deepEqual({ tea: NaN }, { tea: NaN });
    assert.deepEqual({ tea: "chai" }, { tea: "chai" });
    assert.deepEqual({ a: "a", b: "b" }, { b: "b", a: "a" });
    assert.deepEqual(new Date(1, 2, 3), new Date(1, 2, 3));

    expect(() => {
      assert.deepEqual({ tea: "chai" }, { tea: "black" });
    }).to.throw(AssertionError);

    const obj1 = Object.create({ tea: "chai" });
    const obj2 = Object.create({ tea: "black" });

    expect(() => {
      assert.deepEqual(obj1, obj2);
    }).to.throw(AssertionError);

    const circularObject = {};
    const secondCircularObject = {};
    circularObject.field = circularObject;
    secondCircularObject.field = secondCircularObject;

    assert.deepEqual(circularObject, secondCircularObject);

    expect(() => {
      secondCircularObject.field2 = secondCircularObject;
      assert.deepEqual(circularObject, secondCircularObject);
    }).to.throw(AssertionError);
  });

  it("notDeepEqual", () => {
    assert.notDeepEqual({ tea: "jasmine" }, { tea: "chai" });
    assert.notDeepEqual(/a/, /b/);
    assert.notDeepEqual(/a/, {});
    assert.notDeepEqual(/a/g, /b/g);
    assert.notDeepEqual(/a/i, /b/i);
    assert.notDeepEqual(/a/m, /b/m);
    assert.notDeepEqual(new Date(1, 2, 3), new Date(4, 5, 6));
    assert.notDeepEqual(new Date(1, 2, 3), {});

    expect(() => {
      assert.notDeepEqual({ tea: "chai" }, { tea: "chai" });
    }).to.throw(AssertionError);

    const circularObject = {};
    const secondCircularObject = { tea: "jasmine" };
    circularObject.field = circularObject;
    secondCircularObject.field = secondCircularObject;

    assert.notDeepEqual(circularObject, secondCircularObject);

    expect(() => {
      delete secondCircularObject.tea;
      assert.notDeepEqual(circularObject, secondCircularObject);
    }).to.throw(AssertionError);
  });

  it("typeOf", () => {
    assert.typeOf("test", "string");
    assert.typeOf(true, "boolean");
    assert.typeOf(NaN, "number");
    assert.typeOf(sym, "symbol");

    expect(() => {
      assert.typeOf(5, "string", "blah");
    }).to.throw(AssertionError, "blah: expected 5 to be a string");
  });

  it("notTypeOf", () => {
    assert.notTypeOf(5, "string");
    assert.notTypeOf(sym, "string");
    assert.notTypeOf(null, "object");
    assert.notTypeOf("test", "number");

    expect(() => {
      assert.notTypeOf(5, "number", "blah");
    }).to.throw(AssertionError, "blah: expected 5 not to be a number");
  });

  function Foo() {}

  const FakeConstructor = {
    [Symbol.hasInstance](x) {
      return x === 3;
    },
  };

  it("instanceOf", () => {
    assert.instanceOf({}, Object);
    assert.instanceOf(/a/, RegExp);
    assert.instanceOf(new Foo(), Foo);
    assert.instanceOf(3, FakeConstructor);

    expect(() => {
      assert.instanceOf(new Foo(), 1);
    }).to.throw(
      "The instanceof assertion needs a constructor but Number was given."
    );

    expect(() => {
      assert.instanceOf(new Foo(), "Foo");
    }).to.throw(
      "The instanceof assertion needs a constructor but String was given."
    );

    expect(() => {
      assert.instanceOf(4, FakeConstructor);
    }).to.throw("expected 4 to be an instance of an unnamed constructor");
  });

  it("notInstanceOf", () => {
    assert.notInstanceOf({}, Foo);
    assert.notInstanceOf({}, Array);
    assert.notInstanceOf(new Foo(), Array);

    expect(() => {
      assert.notInstanceOf(new Foo(), Foo);
    }).to.throw("expected Foo{} to not be an instance of Foo");

    expect(() => {
      assert.notInstanceOf(3, FakeConstructor);
    }).to.throw("expected 3 to not be an instance of an unnamed constructor");
  });

  it("isObject", () => {
    assert.isObject({});
    assert.isObject(new Foo());

    expect(() => {
      assert.isObject(true);
    }).to.throw(AssertionError, "expected true to be an object");

    expect(() => {
      assert.isObject(Foo);
    }).to.throw(AssertionError, "expected [Function Foo] to be an object");

    expect(() => {
      assert.isObject("foo");
    }).to.throw(AssertionError, "expected 'foo' to be an object");
  });

  it("isNotObject", () => {
    assert.isNotObject(1);
    assert.isNotObject([]);
    assert.isNotObject(/a/);
    assert.isNotObject(Foo);
    assert.isNotObject("foo");

    expect(() => {
      assert.isNotObject({}, "blah");
    }).to.throw(AssertionError, "blah: expected {} not to be an object");
  });

  it("include", () => {
    assert.include("foobar", "bar");
    assert.include("", "");
    assert.include([1, 2, 3], 3);

    // .include should work with Error objects and objects with a custom
    // `@@toStringTag`.
    assert.include(new Error("foo"), { message: "foo" });
    assert.include({ a: 1, [Symbol.toStringTag]: "foo" }, { a: 1 });

    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    assert.include([obj1, obj2], obj1);
    assert.include({ foo: obj1, bar: obj2 }, { foo: obj1 });
    assert.include({ foo: obj1, bar: obj2 }, { foo: obj1, bar: obj2 });

    var map = new Map();
    var val = [{ a: 1 }];
    map.set("a", val);
    map.set("b", 2);
    map.set("c", -0);
    map.set("d", NaN);

    assert.include(map, val);
    assert.include(map, 2);
    assert.include(map, 0);
    assert.include(map, NaN);

    var set = new Set();
    var val = [{ a: 1 }];
    set.add(val);
    set.add(2);
    set.add(-0);
    set.add(NaN);

    assert.include(set, val);
    assert.include(set, 2);
    assert.include(set, NaN);

    var ws = new WeakSet();
    var val = [{ a: 1 }];
    ws.add(val);

    assert.include(ws, val);

    var sym1 = Symbol(),
      sym2 = Symbol();
    assert.include([sym1, sym2], sym1);

    expect(() => {
      assert.include("foobar", "baz", "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to include 'baz'");

    expect(() => {
      assert.include([{ a: 1 }, { b: 2 }], { a: 1 });
    }).to.throw(
      AssertionError,
      "expected [ { a: 1 }, { b: 2 } ] to include { a: 1 }"
    );

    expect(() => {
      assert.include(
        { foo: { a: 1 }, bar: { b: 2 } },
        { foo: { a: 1 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { foo: { a: 1 }, bar: { b: 2 } } to have property 'foo' of { a: 1 }, but got { a: 1 }"
    );

    expect(() => {
      assert.include(true, true, "blah");
    }).to.throw(
      AssertionError,
      "blah: the given combination of arguments (boolean and boolean) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a boolean"
    );

    expect(() => {
      assert.include(42, "bar");
    }).to.throw(
      AssertionError,
      "the given combination of arguments (number and string) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a string"
    );

    expect(() => {
      assert.include(null, 42);
    }).to.throw(
      AssertionError,
      "the given combination of arguments (null and number) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a number"
    );

    expect(() => {
      assert.include(undefined, "bar");
    }).to.throw(
      AssertionError,
      "the given combination of arguments (undefined and string) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a string"
    );
  });

  it("notInclude", () => {
    assert.notInclude("foobar", "baz");
    assert.notInclude([1, 2, 3], 4);

    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    assert.notInclude([obj1, obj2], { a: 1 });
    assert.notInclude({ foo: obj1, bar: obj2 }, { foo: { a: 1 } });
    assert.notInclude({ foo: obj1, bar: obj2 }, { foo: obj1, bar: { b: 2 } });

    var map = new Map();
    var val = [{ a: 1 }];
    map.set("a", val);
    map.set("b", 2);

    assert.notInclude(map, [{ a: 1 }]);
    assert.notInclude(map, 3);

    var set = new Set();
    var val = [{ a: 1 }];
    set.add(val);
    set.add(2);

    assert.include(set, val);
    assert.include(set, 2);

    assert.notInclude(set, [{ a: 1 }]);
    assert.notInclude(set, 3);

    var ws = new WeakSet();
    var val = [{ a: 1 }];
    ws.add(val);

    assert.notInclude(ws, [{ a: 1 }]);
    assert.notInclude(ws, {});

    var sym1 = Symbol(),
      sym2 = Symbol(),
      sym3 = Symbol();
    assert.notInclude([sym1, sym2], sym3);

    expect(() => {
      var obj1 = { a: 1 },
        obj2 = { b: 2 };
      assert.notInclude([obj1, obj2], obj1, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 } ] to not include { a: 1 }"
    );

    expect(() => {
      var obj1 = { a: 1 },
        obj2 = { b: 2 };
      assert.notInclude(
        { foo: obj1, bar: obj2 },
        { foo: obj1, bar: obj2 },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { foo: { a: 1 }, bar: { b: 2 } } to not have property 'foo' of { a: 1 }"
    );

    expect(() => {
      assert.notInclude(true, true, "blah");
    }).to.throw(
      AssertionError,
      "blah: the given combination of arguments (boolean and boolean) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a boolean"
    );

    expect(() => {
      assert.notInclude(42, "bar");
    }).to.throw(
      AssertionError,
      "the given combination of arguments (number and string) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a string"
    );

    expect(() => {
      assert.notInclude(null, 42);
    }).to.throw(
      AssertionError,
      "the given combination of arguments (null and number) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a number"
    );

    expect(() => {
      assert.notInclude(undefined, "bar");
    }).to.throw(
      AssertionError,
      "the given combination of arguments (undefined and string) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a string"
    );

    expect(() => {
      assert.notInclude("foobar", "bar");
    }).to.throw(AssertionError, "expected 'foobar' to not include 'bar'");
  });

  it("deepInclude and notDeepInclude", () => {
    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    assert.deepInclude([obj1, obj2], { a: 1 });
    assert.notDeepInclude([obj1, obj2], { a: 9 });
    assert.notDeepInclude([obj1, obj2], { z: 1 });
    assert.deepInclude({ foo: obj1, bar: obj2 }, { foo: { a: 1 } });
    assert.deepInclude(
      { foo: obj1, bar: obj2 },
      { foo: { a: 1 }, bar: { b: 2 } }
    );
    assert.notDeepInclude({ foo: obj1, bar: obj2 }, { foo: { a: 9 } });
    assert.notDeepInclude({ foo: obj1, bar: obj2 }, { foo: { z: 1 } });
    assert.notDeepInclude({ foo: obj1, bar: obj2 }, { baz: { a: 1 } });
    assert.notDeepInclude(
      { foo: obj1, bar: obj2 },
      { foo: { a: 1 }, bar: { b: 9 } }
    );

    var map = new Map();
    map.set(1, [{ a: 1 }]);

    assert.deepInclude(map, [{ a: 1 }]);

    var set = new Set();
    set.add([{ a: 1 }]);

    assert.deepInclude(set, [{ a: 1 }]);

    expect(() => {
      assert.deepInclude(new WeakSet(), {}, "foo");
    }).to.throw(
      AssertionError,
      "foo: unable to use .deep.include with WeakSet"
    );

    expect(() => {
      assert.deepInclude([obj1, obj2], { a: 9 }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 } ] to deep include { a: 9 }"
    );

    expect(() => {
      assert.notDeepInclude([obj1, obj2], { a: 1 });
    }).to.throw(
      AssertionError,
      "expected [ { a: 1 }, { b: 2 } ] to not deep include { a: 1 }"
    );

    expect(() => {
      assert.deepInclude(
        { foo: obj1, bar: obj2 },
        { foo: { a: 1 }, bar: { b: 9 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { foo: { a: 1 }, bar: { b: 2 } } to have deep property 'bar' of { b: 9 }, but got { b: 2 }"
    );

    expect(() => {
      assert.notDeepInclude(
        { foo: obj1, bar: obj2 },
        { foo: { a: 1 }, bar: { b: 2 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { foo: { a: 1 }, bar: { b: 2 } } to not have deep property 'foo' of { a: 1 }"
    );
  });

  it("nestedInclude and notNestedInclude", () => {
    assert.nestedInclude({ a: { b: ["x", "y"] } }, { "a.b[1]": "y" });
    assert.notNestedInclude({ a: { b: ["x", "y"] } }, { "a.b[1]": "x" });
    assert.notNestedInclude({ a: { b: ["x", "y"] } }, { "a.c": "y" });

    assert.notNestedInclude({ a: { b: [{ x: 1 }] } }, { "a.b[0]": { x: 1 } });

    assert.nestedInclude({ ".a": { "[b]": "x" } }, { "\\.a.\\[b\\]": "x" });
    assert.notNestedInclude({ ".a": { "[b]": "x" } }, { "\\.a.\\[b\\]": "y" });

    expect(() => {
      assert.nestedInclude({ a: { b: ["x", "y"] } }, { "a.b[1]": "x" }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ 'x', 'y' ] } } to have nested property 'a.b[1]' of 'x', but got 'y'"
    );

    expect(() => {
      assert.nestedInclude({ a: { b: ["x", "y"] } }, { "a.b[1]": "x" }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ 'x', 'y' ] } } to have nested property 'a.b[1]' of 'x', but got 'y'"
    );

    expect(() => {
      assert.nestedInclude({ a: { b: ["x", "y"] } }, { "a.c": "y" });
    }).to.throw(
      AssertionError,
      "expected { a: { b: [ 'x', 'y' ] } } to have nested property 'a.c'"
    );

    expect(() => {
      assert.notNestedInclude(
        { a: { b: ["x", "y"] } },
        { "a.b[1]": "y" },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ 'x', 'y' ] } } to not have nested property 'a.b[1]' of 'y'"
    );
  });

  it("deepNestedInclude and notDeepNestedInclude", () => {
    assert.deepNestedInclude({ a: { b: [{ x: 1 }] } }, { "a.b[0]": { x: 1 } });
    assert.notDeepNestedInclude(
      { a: { b: [{ x: 1 }] } },
      { "a.b[0]": { y: 2 } }
    );
    assert.notDeepNestedInclude({ a: { b: [{ x: 1 }] } }, { "a.c": { x: 1 } });

    assert.deepNestedInclude(
      { ".a": { "[b]": { x: 1 } } },
      { "\\.a.\\[b\\]": { x: 1 } }
    );
    assert.notDeepNestedInclude(
      { ".a": { "[b]": { x: 1 } } },
      { "\\.a.\\[b\\]": { y: 2 } }
    );

    expect(() => {
      assert.deepNestedInclude(
        { a: { b: [{ x: 1 }] } },
        { "a.b[0]": { y: 2 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ { x: 1 } ] } } to have deep nested property 'a.b[0]' of { y: 2 }, but got { x: 1 }"
    );

    expect(() => {
      assert.deepNestedInclude(
        { a: { b: [{ x: 1 }] } },
        { "a.b[0]": { y: 2 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ { x: 1 } ] } } to have deep nested property 'a.b[0]' of { y: 2 }, but got { x: 1 }"
    );

    expect(() => {
      assert.deepNestedInclude({ a: { b: [{ x: 1 }] } }, { "a.c": { x: 1 } });
    }).to.throw(
      AssertionError,
      "expected { a: { b: [ { x: 1 } ] } } to have deep nested property 'a.c'"
    );

    expect(() => {
      assert.notDeepNestedInclude(
        { a: { b: [{ x: 1 }] } },
        { "a.b[0]": { x: 1 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ { x: 1 } ] } } to not have deep nested property 'a.b[0]' of { x: 1 }"
    );
  });

  it("ownInclude and notOwnInclude", () => {
    assert.ownInclude({ a: 1 }, { a: 1 });
    assert.notOwnInclude({ a: 1 }, { a: 3 });
    assert.notOwnInclude({ a: 1 }, { toString: Object.prototype.toString });

    assert.notOwnInclude({ a: { b: 2 } }, { a: { b: 2 } });

    expect(() => {
      assert.ownInclude({ a: 1 }, { a: 3 }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: 1 } to have own property 'a' of 3, but got 1"
    );

    expect(() => {
      assert.ownInclude({ a: 1 }, { a: 3 }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: 1 } to have own property 'a' of 3, but got 1"
    );

    expect(() => {
      assert.ownInclude({ a: 1 }, { toString: Object.prototype.toString });
    }).to.throw(
      AssertionError,
      "expected { a: 1 } to have own property 'toString'"
    );

    expect(() => {
      assert.notOwnInclude({ a: 1 }, { a: 1 }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: 1 } to not have own property 'a' of 1"
    );
  });

  it("deepOwnInclude and notDeepOwnInclude", () => {
    assert.deepOwnInclude({ a: { b: 2 } }, { a: { b: 2 } });
    assert.notDeepOwnInclude({ a: { b: 2 } }, { a: { c: 3 } });
    assert.notDeepOwnInclude(
      { a: { b: 2 } },
      { toString: Object.prototype.toString }
    );

    expect(() => {
      assert.deepOwnInclude({ a: { b: 2 } }, { a: { c: 3 } }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: 2 } } to have deep own property 'a' of { c: 3 }, but got { b: 2 }"
    );

    expect(() => {
      assert.deepOwnInclude({ a: { b: 2 } }, { a: { c: 3 } }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: 2 } } to have deep own property 'a' of { c: 3 }, but got { b: 2 }"
    );

    expect(() => {
      assert.deepOwnInclude(
        { a: { b: 2 } },
        { toString: Object.prototype.toString }
      );
    }).to.throw(
      AssertionError,
      "expected { a: { b: 2 } } to have deep own property 'toString'"
    );

    expect(() => {
      assert.notDeepOwnInclude({ a: { b: 2 } }, { a: { b: 2 } }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: 2 } } to not have deep own property 'a' of { b: 2 }"
    );
  });

  it("lengthOf", () => {
    assert.lengthOf([1, 2, 3], 3);
    assert.lengthOf("foobar", 6);

    expect(() => {
      assert.lengthOf("foobar", 5, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foobar' to have a length of 5 but got 6"
    );

    expect(() => {
      assert.lengthOf(1, 5);
    }).to.throw(AssertionError, "expected 1 to have property 'length'");
  });

  it("match", () => {
    assert.match("foobar", /^foo/);
    assert.notMatch("foobar", /^bar/);

    expect(() => {
      assert.match("foobar", /^bar/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to match /^bar/i");

    expect(() => {
      assert.notMatch("foobar", /^foo/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' not to match /^foo/i");
  });
});

describe("expect", () => {
  const sym = Symbol();

  describe("proxify", () => {
    it("throws when invalid property follows expect", function () {
      expect(() => {
        expect(42).pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows language chain", function () {
      expect(() => {
        expect(42).to.pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows property assertion", function () {
      expect(() => {
        expect(42).ok.pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows uncalled method assertion", function () {
      expect(() => {
        expect(42).equal.pizza;
      }).to.throw(
        Error,
        'Invalid Chai property: equal.pizza. See docs for proper usage of "equal".'
      );
    });

    it("throws when invalid property follows called method assertion", function () {
      expect(() => {
        expect(42).equal(42).pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows uncalled chainable method assertion", function () {
      expect(() => {
        expect(42).a.pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows called chainable method assertion", function () {
      expect(() => {
        expect(42).a("number").pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("doesn't throw if invalid property is excluded via config", function () {
      expect(() => {
        expect(42).then;
      }).to.not.throw();
    });
  });

  it("no-op chains", () => {
    [
      "to",
      "be",
      "been",
      "is",
      "and",
      "has",
      "have",
      "with",
      "that",
      "which",
      "at",
      "of",
      "same",
      "but",
      "does",
    ].forEach((chain) => {
      // tests that chain exists
      expect(expect(1)[chain]).not.undefined;

      // tests methods
      expect(1)[chain].equal(1);

      // tests properties that assert
      expect(false)[chain].false;

      // tests not
      expect(false)[chain].not.true;

      // tests chainable methods
      expect([1, 2, 3])[chain].contains(1);
    });
  });

  it("fail", () => {
    expect(() => {
      expect.fail(0, 1, "this has failed");
    }).to.throw(AssertionError, "this has failed");
  });

  it("true", () => {
    expect(true).to.be.true;
    expect(false).to.not.be.true;
    expect(1).to.not.be.true;

    expect(() => {
      expect("test", "blah").to.be.true;
    }).to.throw(AssertionError, "blah: expected 'test' to be true");
  });

  it("ok", () => {
    expect(true).to.be.ok;
    expect(false).to.not.be.ok;
    expect(1).to.be.ok;
    expect(0).to.not.be.ok;

    expect(() => {
      expect("", "blah").to.be.ok;
    }).to.throw(AssertionError, "blah: expected '' to be truthy");

    expect(() => {
      expect("test").to.not.be.ok;
    }).to.throw(AssertionError, "expected 'test' to be falsy");
  });

  it("false", () => {
    expect(false).to.be.false;
    expect(true).to.not.be.false;
    expect(0).to.not.be.false;

    expect(() => {
      expect("", "blah").to.be.false;
    }).to.throw(AssertionError, "blah: expected '' to be false");
  });

  it("null", () => {
    expect(null).to.be.null;
    expect(false).to.not.be.null;

    expect(() => {
      expect("", "blah").to.be.null;
    }).to.throw(AssertionError, "blah: expected '' to be null");
  });

  it("undefined", () => {
    expect(undefined).to.be.undefined;
    expect(null).to.not.be.undefined;

    expect(() => {
      expect("", "blah").to.be.undefined;
    }).to.throw(AssertionError, "blah: expected '' to be undefined");
  });

  it("exist", () => {
    const foo = "bar";
    var bar;

    expect(foo).to.exist;
    expect(bar).to.not.exist;
    expect(0).to.exist;
    expect(false).to.exist;
    expect("").to.exist;

    expect(() => {
      expect(bar, "blah").to.exist;
    }).to.throw(AssertionError, "blah: expected undefined to exist");

    expect(() => {
      expect(foo).to.not.exist(foo);
    }).to.throw(AssertionError, "expected 'bar' to not exist");
  });

  it("arguments", () => {
    var args = (function () {
      return arguments;
    })(1, 2, 3);
    expect(args).to.be.arguments;
    expect([]).to.not.be.arguments;
    expect(args).to.be.an("arguments").and.be.arguments;
    expect([]).to.be.an("array").and.not.be.Arguments;

    expect(() => {
      expect([]).to.be.arguments;
    }).to.throw(AssertionError, "expected [] to be arguments but got Array");
  });

  it("instanceof", () => {
    function Foo() {}
    expect(new Foo()).to.be.an.instanceof(Foo);

    expect(() => {
      expect(new Foo()).to.an.instanceof(1, "blah");
    }).to.throw(
      AssertionError,
      "blah: The instanceof assertion needs a constructor but Number was given."
    );

    expect(() => {
      expect(new Foo(), "blah").to.an.instanceof(1);
    }).to.throw(
      AssertionError,
      "blah: The instanceof assertion needs a constructor but Number was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof("batman");
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but String was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof({});
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but Object was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof(true);
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but Boolean was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof(null);
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but null was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof(undefined);
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but undefined was given."
    );

    expect(() => {
      function Thing() {}
      var t = new Thing();
      Thing.prototype = 1337;
      expect(t).to.an.instanceof(Thing);
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but Function was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof(Symbol());
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but Symbol was given."
    );

    expect(() => {
      var FakeConstructor = {};
      var fakeInstanceB = 4;
      FakeConstructor[Symbol.hasInstance] = function (val) {
        return val === 3;
      };
      expect(fakeInstanceB).to.be.an.instanceof(FakeConstructor);
    }).to.throw(
      AssertionError,
      "expected 4 to be an instance of an unnamed constructor"
    );

    expect(() => {
      var FakeConstructor = {};
      var fakeInstanceB = 4;
      FakeConstructor[Symbol.hasInstance] = function (val) {
        return val === 4;
      };
      expect(fakeInstanceB).to.not.be.an.instanceof(FakeConstructor);
    }).to.throw(
      AssertionError,
      "expected 4 to not be an instance of an unnamed constructor"
    );

    expect(() => {
      expect(3).to.an.instanceof(Foo, "blah");
    }).to.throw(AssertionError, "blah: expected 3 to be an instance of Foo");

    expect(() => {
      expect(3, "blah").to.an.instanceof(Foo);
    }).to.throw(AssertionError, "blah: expected 3 to be an instance of Foo");
  });

  it("within(start, finish)", () => {
    expect(5).to.be.within(5, 10);
    expect(5).to.be.within(3, 6);
    expect(5).to.be.within(3, 5);
    expect(5).to.not.be.within(1, 3);
    expect("foo").to.have.length.within(2, 4);
    expect("foo").to.have.lengthOf.within(2, 4);
    expect([1, 2, 3]).to.have.length.within(2, 4);
    expect([1, 2, 3]).to.have.lengthOf.within(2, 4);

    expect(() => {
      expect(5).to.not.be.within(4, 6, "blah");
    }).to.throw(AssertionError, "blah: expected 5 to not be within 4..6");

    expect(() => {
      expect(5, "blah").to.not.be.within(4, 6);
    }).to.throw(AssertionError, "blah: expected 5 to not be within 4..6");

    expect(() => {
      expect(10).to.be.within(50, 100, "blah");
    }).to.throw(AssertionError, "blah: expected 10 to be within 50..100");

    expect(() => {
      expect("foo").to.have.length.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length within 5..7"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.within(5, 7);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length within 5..7"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length within 5..7"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length within 5..7"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length within 5..7"
    );

    expect(() => {
      expect(null).to.be.within(0, 1, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.within(0, 1);
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.within(null, 1, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1, "blah").to.be.within(null, 1);
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1).to.be.within(0, null, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1, "blah").to.be.within(0, null);
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(null).to.not.be.within(0, 1, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.within(null, 1, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1).to.not.be.within(0, null, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1).to.have.length.within(5, 7, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.within(5, 7);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.within(5, 7, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("within(start, finish) (dates)", () => {
    const now = new Date();
    const oneSecondAgo = new Date(now.getTime() - 1000);
    const oneSecondAfter = new Date(now.getTime() + 1000);
    const nowISO = now.toISOString();
    const beforeISO = oneSecondAgo.toISOString();
    const afterISO = oneSecondAfter.toISOString();

    expect(now).to.be.within(oneSecondAgo, oneSecondAfter);
    expect(now).to.be.within(now, oneSecondAfter);
    expect(now).to.be.within(now, now);
    expect(oneSecondAgo).to.not.be.within(now, oneSecondAfter);

    expect(() => {
      expect(now).to.not.be.within(now, oneSecondAfter, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " +
        nowISO +
        " to not be within " +
        nowISO +
        ".." +
        afterISO
    );

    expect(() => {
      expect(now, "blah").to.not.be.within(oneSecondAgo, oneSecondAfter);
    }).to.throw(
      AssertionError,
      "blah: expected " +
        nowISO +
        " to not be within " +
        beforeISO +
        ".." +
        afterISO
    );

    expect(() => {
      expect(now).to.have.length.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + nowISO + " to have property 'length'"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.within(now, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(now).to.be.within(now, 1, "blah");
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(now).to.be.within(null, now, "blah");
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(now).to.be.within(now, undefined, "blah");
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(now, "blah").to.be.within(1, now);
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(now, "blah").to.be.within(now, 1);
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(null).to.not.be.within(now, oneSecondAfter, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");
  });

  it("above(n)", () => {
    expect(5).to.be.above(2);
    expect(5).to.be.greaterThan(2);
    expect(5).to.not.be.above(5);
    expect(5).to.not.be.above(6);
    expect("foo").to.have.length.above(2);
    expect("foo").to.have.lengthOf.above(2);
    expect([1, 2, 3]).to.have.length.above(2);
    expect([1, 2, 3]).to.have.lengthOf.above(2);

    expect(() => {
      expect(5).to.be.above(6, "blah");
    }).to.throw(AssertionError, "blah: expected 5 to be above 6");

    expect(() => {
      expect(5, "blah").to.be.above(6);
    }).to.throw(AssertionError, "blah: expected 5 to be above 6");

    expect(() => {
      expect(10).to.not.be.above(6, "blah");
    }).to.throw(AssertionError, "blah: expected 10 to be at most 6");

    expect(() => {
      expect("foo").to.have.length.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length above 4 but got 3"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.above(4);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length above 4 but got 3"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length above 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length above 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length above 4 but got 3"
    );

    expect(() => {
      expect(null).to.be.above(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.above(0);
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.above(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to above must be a number");

    expect(() => {
      expect(1, "blah").to.be.above(null);
    }).to.throw(AssertionError, "blah: the argument to above must be a number");

    expect(() => {
      expect(null).to.not.be.above(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.above(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to above must be a number");

    expect(() => {
      expect(1).to.have.length.above(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.above(0);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.above(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("above(n) (dates)", () => {
    const now = new Date();
    const oneSecondAgo = new Date(now.getTime() - 1000);
    const oneSecondAfter = new Date(now.getTime() + 1000);

    expect(now).to.be.above(oneSecondAgo);
    expect(now).to.be.greaterThan(oneSecondAgo);
    expect(now).to.not.be.above(now);
    expect(now).to.not.be.above(oneSecondAfter);

    expect(() => {
      expect(now).to.be.above(oneSecondAfter, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " +
        now.toISOString() +
        " to be above " +
        oneSecondAfter.toISOString()
    );

    expect(() => {
      expect(10).to.not.be.above(6, "blah");
    }).to.throw(AssertionError, "blah: expected 10 to be at most 6");

    expect(() => {
      expect(now).to.have.length.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + now.toISOString() + " to have property 'length'"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.above(now, "blah");
    }).to.throw(AssertionError, "blah: the argument to above must be a number");

    expect(() => {
      expect(null).to.be.above(now, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(now).to.be.above(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to above must be a date");

    expect(() => {
      expect(null).to.have.length.above(0, "blah");
    }).to.throw(AssertionError, "blah: Target cannot be null or undefined.");
  });

  it("least(n)", () => {
    expect(5).to.be.at.least(2);
    expect(5).to.be.at.least(5);
    expect(5).to.not.be.at.least(6);
    expect("foo").to.have.length.of.at.least(2);
    expect("foo").to.have.lengthOf.at.least(2);
    expect([1, 2, 3]).to.have.length.of.at.least(2);
    expect([1, 2, 3]).to.have.lengthOf.at.least(2);

    expect(() => {
      expect(5).to.be.at.least(6, "blah");
    }).to.throw(AssertionError, "blah: expected 5 to be at least 6");

    expect(() => {
      expect(5, "blah").to.be.at.least(6);
    }).to.throw(AssertionError, "blah: expected 5 to be at least 6");

    expect(() => {
      expect(10).to.not.be.at.least(6, "blah");
    }).to.throw(AssertionError, "blah: expected 10 to be below 6");

    expect(() => {
      expect("foo").to.have.length.of.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at least 4 but got 3"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.of.at.least(4);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at least 4 but got 3"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at least 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.of.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length at least 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length at least 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3, 4]).to.not.have.length.of.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3, 4 ] to have a length below 4"
    );

    expect(() => {
      expect([1, 2, 3, 4]).to.not.have.lengthOf.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3, 4 ] to have a length below 4"
    );

    expect(() => {
      expect(null).to.be.at.least(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.at.least(0);
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.at.least(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to least must be a number");

    expect(() => {
      expect(1, "blah").to.be.at.least(null);
    }).to.throw(AssertionError, "blah: the argument to least must be a number");

    expect(() => {
      expect(null).to.not.be.at.least(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.at.least(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to least must be a number");

    expect(() => {
      expect(1).to.have.length.at.least(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.at.least(0);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.at.least(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("below(n)", () => {
    expect(2).to.be.below(5);
    expect(2).to.be.lessThan(5);
    expect(2).to.not.be.below(2);
    expect(2).to.not.be.below(1);
    expect("foo").to.have.length.below(4);
    expect("foo").to.have.lengthOf.below(4);
    expect([1, 2, 3]).to.have.length.below(4);
    expect([1, 2, 3]).to.have.lengthOf.below(4);

    expect(() => {
      expect(6).to.be.below(5, "blah");
    }).to.throw(AssertionError, "blah: expected 6 to be below 5");

    expect(() => {
      expect(6, "blah").to.be.below(5);
    }).to.throw(AssertionError, "blah: expected 6 to be below 5");

    expect(() => {
      expect(6).to.not.be.below(10, "blah");
    }).to.throw(AssertionError, "blah: expected 6 to be at least 10");

    expect(() => {
      expect("foo").to.have.length.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length below 2 but got 3"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.below(2);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length below 2 but got 3"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length below 2 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length below 2 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length below 2 but got 3"
    );

    expect(() => {
      expect(null).to.be.below(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.below(0);
    }, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.below(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a number");

    expect(() => {
      expect(1, "blah").to.be.below(null);
    }).to.throw(AssertionError, "blah: the argument to below must be a number");

    expect(() => {
      expect(null).to.not.be.below(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.below(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a number");

    expect(() => {
      expect(1).to.have.length.below(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.below(0);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.below(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("below(n) (dates)", () => {
    const now = new Date();
    const oneSecondAgo = new Date(now.getTime() - 1000);
    const oneSecondAfter = new Date(now.getTime() + 1000);

    expect(now).to.be.below(oneSecondAfter);
    expect(oneSecondAgo).to.be.lessThan(now);
    expect(now).to.not.be.below(oneSecondAgo);
    expect(oneSecondAfter).to.not.be.below(oneSecondAgo);

    expect(() => {
      expect(now).to.be.below(oneSecondAgo, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " +
        now.toISOString() +
        " to be below " +
        oneSecondAgo.toISOString()
    );

    expect(() => {
      expect(now).to.not.be.below(oneSecondAfter, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " +
        now.toISOString() +
        " to be at least " +
        oneSecondAfter.toISOString()
    );

    expect(() => {
      expect("foo").to.have.length.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length below 2 but got 3"
    );

    expect(() => {
      expect(null).to.be.below(now, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.below(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a number");

    expect(() => {
      expect(now).to.not.be.below(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a date");

    expect(() => {
      expect(now).to.have.length.below(0, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + now.toISOString() + " to have property 'length'"
    );

    expect(() => {
      expect("asdasd").to.have.length.below(now, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a number");
  });

  it("most(n)", () => {
    expect(2).to.be.at.most(5);
    expect(2).to.be.at.most(2);
    expect(2).to.not.be.at.most(1);
    expect("foo").to.have.length.of.at.most(4);
    expect("foo").to.have.lengthOf.at.most(4);
    expect([1, 2, 3]).to.have.length.of.at.most(4);
    expect([1, 2, 3]).to.have.lengthOf.at.most(4);

    expect(() => {
      expect(6).to.be.at.most(5, "blah");
    }).to.throw(AssertionError, "blah: expected 6 to be at most 5");

    expect(() => {
      expect(6, "blah").to.be.at.most(5);
    }).to.throw(AssertionError, "blah: expected 6 to be at most 5");

    expect(() => {
      expect(6).to.not.be.at.most(10, "blah");
    }).to.throw(AssertionError, "blah: expected 6 to be above 10");

    expect(() => {
      expect("foo").to.have.length.of.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at most 2 but got 3"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.of.at.most(2);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at most 2 but got 3"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at most 2 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.of.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length at most 2 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length at most 2 but got 3"
    );

    expect(() => {
      expect([1, 2]).to.not.have.length.of.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2 ] to have a length above 2"
    );

    expect(() => {
      expect([1, 2]).to.not.have.lengthOf.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2 ] to have a length above 2"
    );

    expect(() => {
      expect(null).to.be.at.most(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.at.most(0);
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.at.most(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(1, "blah").to.be.at.most(null);
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(null).to.not.be.at.most(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.at.most(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(1).to.have.length.of.at.most(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.of.at.most(0);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.at.most(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("most(n) (dates)", () => {
    const now = new Date();
    const oneSecondBefore = new Date(now.getTime() - 1000);
    const oneSecondAfter = new Date(now.getTime() + 1000);
    const nowISO = now.toISOString();
    const beforeISO = oneSecondBefore.toISOString();

    expect(now).to.be.at.most(oneSecondAfter);
    expect(now).to.be.at.most(now);
    expect(now).to.not.be.at.most(oneSecondBefore);

    expect(() => {
      expect(now).to.be.at.most(oneSecondBefore, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + nowISO + " to be at most " + beforeISO
    );

    expect(() => {
      expect(now).to.not.be.at.most(now, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + nowISO + " to be above " + nowISO
    );

    expect(() => {
      expect(now).to.have.length.of.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + nowISO + " to have property 'length'"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.of.at.most(now);
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect([1, 2, 3]).to.not.have.length.of.at.most(now, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(null).to.be.at.most(now, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(now, "blah").to.be.at.most(null);
    }).to.throw(AssertionError, "blah: the argument to most must be a date");

    expect(() => {
      expect(1).to.be.at.most(now, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(now, "blah").to.be.at.most(1);
    }).to.throw(AssertionError, "blah: the argument to most must be a date");

    expect(() => {
      expect(now).to.not.be.at.most(undefined, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a date");
  });

  it("match(regexp)", () => {
    expect("foobar").to.match(/^foo/);
    expect("foobar").to.matches(/^foo/);
    expect("foobar").to.not.match(/^bar/);

    expect(() => {
      expect("foobar").to.match(/^bar/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to match /^bar/i");

    expect(() => {
      expect("foobar", "blah").to.match(/^bar/i);
    }).to.throw(AssertionError, "blah: expected 'foobar' to match /^bar/i");

    expect(() => {
      expect("foobar").to.matches(/^bar/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to match /^bar/i");

    expect(() => {
      expect("foobar").to.not.match(/^foo/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' not to match /^foo/i");
  });

  it("lengthOf(n)", () => {
    expect("test").to.have.length(4);
    expect("test").to.have.lengthOf(4);
    expect("test").to.not.have.length(3);
    expect("test").to.not.have.lengthOf(3);
    expect([1, 2, 3]).to.have.length(3);
    expect([1, 2, 3]).to.have.lengthOf(3);

    expect(() => {
      expect(4).to.have.length(3, "blah");
    }).to.throw(AssertionError, "blah: expected 4 to have property 'length'");

    expect(() => {
      expect(4, "blah").to.have.length(3);
    }).to.throw(AssertionError, "blah: expected 4 to have property 'length'");

    expect(() => {
      expect(4).to.have.lengthOf(3, "blah");
    }).to.throw(AssertionError, "blah: expected 4 to have property 'length'");

    expect(() => {
      expect("asd").to.not.have.length(3, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'asd' to not have a length of 3"
    );
    expect(() => {
      expect("asd").to.not.have.lengthOf(3, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'asd' to not have a length of 3"
    );
  });

  it("eql(val)", () => {
    expect("test").to.eql("test");
    expect({ foo: "bar" }).to.eql({ foo: "bar" });
    expect(1).to.eql(1);
    expect("4").to.not.eql(4);
    expect(sym).to.eql(sym);

    expect(() => {
      expect(4).to.eql(3, "blah");
    }).to.throw(AssertionError, "blah: expected 4 to deeply equal 3");
  });

  it("equal(val)", () => {
    expect("test").to.equal("test");
    expect(1).to.equal(1);
    expect(sym).to.equal(sym);

    expect(() => {
      expect(4).to.equal(3, "blah");
    }).to.throw(AssertionError, "blah: expected 4 to equal 3");

    expect(() => {
      expect(4, "blah").to.equal(3);
    }).to.throw(AssertionError, "blah: expected 4 to equal 3");

    expect(() => {
      expect("4").to.equal(4, "blah");
    }).to.throw(AssertionError, "blah: expected '4' to equal 4");
  });

  it("deep.equal(val)", () => {
    expect({ foo: "bar" }).to.deep.equal({ foo: "bar" });
    expect({ foo: "bar" }).not.to.deep.equal({ foo: "baz" });
  });

  it("deep.equal(/regexp/)", () => {
    expect(/a/).to.deep.equal(/a/);
    expect(/a/).not.to.deep.equal(/b/);
    expect(/a/).not.to.deep.equal({});
    expect(/a/g).to.deep.equal(/a/g);
    expect(/a/g).not.to.deep.equal(/b/g);
    expect(/a/i).to.deep.equal(/a/i);
    expect(/a/i).not.to.deep.equal(/b/i);
    expect(/a/m).to.deep.equal(/a/m);
    expect(/a/m).not.to.deep.equal(/b/m);
  });

  it("deep.equal(Date)", () => {
    var a = new Date(1, 2, 3),
      b = new Date(4, 5, 6);
    expect(a).to.deep.equal(a);
    expect(a).not.to.deep.equal(b);
    expect(a).not.to.deep.equal({});
  });

  it("empty", () => {
    function FakeArgs() {}
    FakeArgs.prototype.length = 0;

    expect("").to.be.empty;
    expect("foo").not.to.be.empty;
    expect([]).to.be.empty;
    expect(["foo"]).not.to.be.empty;
    expect(new FakeArgs()).to.be.empty;
    expect({ arguments: 0 }).not.to.be.empty;
    expect({}).to.be.empty;
    expect({ foo: "bar" }).not.to.be.empty;

    expect(() => {
      expect(new WeakMap(), "blah").not.to.be.empty;
    }).to.throw(AssertionError, "blah: .empty was passed a weak collection");

    expect(() => {
      expect(new WeakSet(), "blah").not.to.be.empty;
    }).to.throw(AssertionError, "blah: .empty was passed a weak collection");

    expect(new Map()).to.be.empty;

    // Not using Map constructor args because not supported in IE 11.
    var map = new Map();
    map.set("a", 1);
    expect(map).not.to.be.empty;

    expect(() => {
      expect(new Map()).not.to.be.empty;
    }).to.throw(AssertionError, "expected Map{} not to be empty");

    map = new Map();
    map.key = "val";
    expect(map).to.be.empty;

    expect(() => {
      expect(map).not.to.be.empty;
    }).to.throw(AssertionError, "expected Map{} not to be empty");

    expect(new Set()).to.be.empty;

    // Not using Set constructor args because not supported in IE 11.
    var set = new Set();
    set.add(1);
    expect(set).not.to.be.empty;

    expect(() => {
      expect(new Set()).not.to.be.empty;
    }).to.throw(AssertionError, "expected Set{} not to be empty");

    set = new Set();
    set.key = "val";
    expect(set).to.be.empty;

    expect(() => {
      expect(set).not.to.be.empty;
    }).to.throw(AssertionError, "expected Set{} not to be empty");

    expect(() => {
      expect("", "blah").not.to.be.empty;
    }).to.throw(AssertionError, "blah: expected '' not to be empty");

    expect(() => {
      expect("foo").to.be.empty;
    }).to.throw(AssertionError, "expected 'foo' to be empty");

    expect(() => {
      expect([]).not.to.be.empty;
    }).to.throw(AssertionError, "expected [] not to be empty");

    expect(() => {
      expect(["foo"]).to.be.empty;
    }).to.throw(AssertionError, "expected [ 'foo' ] to be empty");

    expect(() => {
      expect(new FakeArgs()).not.to.be.empty;
    }).to.throw(AssertionError, "expected FakeArgs{} not to be empty");

    expect(() => {
      expect({ arguments: 0 }).to.be.empty;
    }).to.throw(AssertionError, "expected { arguments: +0 } to be empty");

    expect(() => {
      expect({}).not.to.be.empty;
    }).to.throw(AssertionError, "expected {} not to be empty");

    expect(() => {
      expect({ foo: "bar" }).to.be.empty;
    }).to.throw(AssertionError, "expected { foo: 'bar' } to be empty");

    expect(() => {
      expect(null, "blah").to.be.empty;
    }).to.throw(
      AssertionError,
      "blah: .empty was passed non-string primitive null"
    );

    expect(() => {
      expect(undefined).to.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive undefined"
    );

    expect(() => {
      expect().to.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive undefined"
    );

    expect(() => {
      expect(null).to.not.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive null");

    expect(() => {
      expect(undefined).to.not.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive undefined"
    );

    expect(() => {
      expect().to.not.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive undefined"
    );

    expect(() => {
      expect(0).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive +0");

    expect(() => {
      expect(1).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive 1");

    expect(() => {
      expect(true).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive true");

    expect(() => {
      expect(false).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive false");

    expect(() => {
      expect(Symbol()).to.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive Symbol()"
    );

    expect(() => {
      expect(Symbol.iterator).to.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive Symbol(Symbol.iterator)"
    );

    expect(() => {
      expect(function () {}, "blah").to.be.empty;
    }).to.throw(AssertionError, "blah: .empty was passed a function");

    expect(() => {
      expect(FakeArgs).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed a function FakeArgs");
  });

  it("string()", () => {
    expect("foobar").to.have.string("bar");
    expect("foobar").to.have.string("foo");
    expect("foobar").to.not.have.string("baz");

    expect(() => {
      expect(3).to.have.string("baz", "blah");
    }).to.throw(AssertionError, "blah: expected 3 to be a string");

    expect(() => {
      expect(3, "blah").to.have.string("baz");
    }).to.throw(AssertionError, "blah: expected 3 to be a string");

    expect(() => {
      expect("foobar").to.have.string("baz", "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to contain 'baz'");

    expect(() => {
      expect("foobar", "blah").to.have.string("baz");
    }).to.throw(AssertionError, "blah: expected 'foobar' to contain 'baz'");

    expect(() => {
      expect("foobar").to.not.have.string("bar", "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to not contain 'bar'");
  });

  it("NaN", () => {
    expect(NaN).to.be.NaN;
    expect(undefined).not.to.be.NaN;
    expect(Infinity).not.to.be.NaN;
    expect("foo").not.to.be.NaN;
    expect({}).not.to.be.NaN;
    expect(4).not.to.be.NaN;
    expect([]).not.to.be.NaN;

    expect(() => {
      expect(NaN, "blah").not.to.be.NaN;
    }).to.throw(AssertionError, "blah: expected NaN not to be NaN");

    expect(() => {
      expect(undefined).to.be.NaN;
    }).to.throw(AssertionError, "expected undefined to be NaN");

    expect(() => {
      expect(Infinity).to.be.NaN;
    }).to.throw(AssertionError, "expected Infinity to be NaN");

    expect(() => {
      expect("foo").to.be.NaN;
    }).to.throw(AssertionError, "expected 'foo' to be NaN");

    expect(() => {
      expect({}).to.be.NaN;
    }).to.throw(AssertionError, "expected {} to be NaN");

    expect(() => {
      expect(4).to.be.NaN;
    }).to.throw(AssertionError, "expected 4 to be NaN");

    expect(() => {
      expect([]).to.be.NaN;
    }).to.throw(AssertionError, "expected [] to be NaN");
  });

  it("finite", function () {
    expect(4).to.be.finite;
    expect(-10).to.be.finite;

    expect(() => {
      expect(NaN, "blah").to.be.finite;
    }).to.throw(AssertionError, "blah: expected NaN to be a finite number");

    expect(() => {
      expect(Infinity).to.be.finite;
    }).to.throw(AssertionError, "expected Infinity to be a finite number");

    expect(() => {
      expect("foo").to.be.finite;
    }).to.throw(AssertionError, "expected 'foo' to be a finite number");

    expect(() => {
      expect([]).to.be.finite;
    }).to.throw(AssertionError, "expected [] to be a finite number");

    expect(() => {
      expect({}).to.be.finite;
    }).to.throw(AssertionError, "expected {} to be a finite number");
  });

  it("property(name)", function () {
    expect("test").to.have.property("length");
    expect({ a: 1 }).to.have.property("toString");
    expect(4).to.not.have.property("length");

    expect({ "foo.bar": "baz" }).to.have.property("foo.bar");
    expect({ foo: { bar: "baz" } }).to.not.have.property("foo.bar");

    // Properties with the value 'undefined' are still properties
    var obj = { foo: undefined };
    Object.defineProperty(obj, "bar", {
      get: function () {},
    });
    expect(obj).to.have.property("foo");
    expect(obj).to.have.property("bar");

    expect({ "foo.bar[]": "baz" }).to.have.property("foo.bar[]");

    expect(() => {
      expect("asd").to.have.property("foo");
    }).to.throw(AssertionError, "expected 'asd' to have property 'foo'");

    expect(() => {
      expect("asd", "blah").to.have.property("foo");
    }).to.throw(AssertionError, "blah: expected 'asd' to have property 'foo'");

    expect(() => {
      expect({ foo: { bar: "baz" } }).to.have.property("foo.bar");
    }).to.throw(
      AssertionError,
      "expected { foo: { bar: 'baz' } } to have property 'foo.bar'"
    );

    expect(() => {
      expect({ a: { b: 1 } }).to.have.own.nested.property("a.b");
    }).to.throw(
      AssertionError,
      'The "nested" and "own" flags cannot be combined.'
    );

    expect(() => {
      expect({ a: { b: 1 } }, "blah").to.have.own.nested.property("a.b");
    }).to.throw(
      AssertionError,
      'blah: The "nested" and "own" flags cannot be combined.'
    );

    expect(() => {
      expect(null, "blah").to.have.property("a");
    }).to.throw(AssertionError, "blah: Target cannot be null or undefined.");

    expect(() => {
      expect(undefined, "blah").to.have.property("a");
    }).to.throw(AssertionError, "blah: Target cannot be null or undefined.");
  });

  it("include()", () => {
    expect(["foo", "bar"]).to.include("foo");
    expect(["foo", "bar"]).to.include("foo");
    expect(["foo", "bar"]).to.include("bar");
    expect([1, 2]).to.include(1);
    expect(["foo", "bar"]).to.not.include("baz");
    expect(["foo", "bar"]).to.not.include(1);

    expect({ a: 1 }).to.include({ toString: Object.prototype.toString });

    // .include should work with Error objects and objects with a custom
    // `@@toStringTag`.
    expect(new Error("foo")).to.include({ message: "foo" });
    var customObj = { a: 1 };
    customObj[Symbol.toStringTag] = "foo";

    expect(customObj).to.include({ a: 1 });

    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    expect([obj1, obj2]).to.include(obj1);
    expect([obj1, obj2]).to.not.include({ a: 1 });
    expect({ foo: obj1, bar: obj2 }).to.include({ foo: obj1 });
    expect({ foo: obj1, bar: obj2 }).to.include({ foo: obj1, bar: obj2 });
    expect({ foo: obj1, bar: obj2 }).to.not.include({ foo: { a: 1 } });
    expect({ foo: obj1, bar: obj2 }).to.not.include({
      foo: obj1,
      bar: { b: 2 },
    });

    var map = new Map();
    var val = [{ a: 1 }];
    map.set("a", val);
    map.set("b", 2);
    map.set("c", -0);
    map.set("d", NaN);

    expect(map).to.include(val);
    expect(map).to.not.include([{ a: 1 }]);
    expect(map).to.include(2);
    expect(map).to.not.include(3);
    expect(map).to.include(0);
    expect(map).to.include(NaN);

    var set = new Set();
    var val = [{ a: 1 }];
    set.add(val);
    set.add(2);
    set.add(-0);
    set.add(NaN);

    expect(set).to.include(val);
    expect(set).to.not.include([{ a: 1 }]);
    expect(set).to.include(2);
    expect(set).to.not.include(3);
    expect(set).to.include(NaN);

    var ws = new WeakSet();
    var val = [{ a: 1 }];
    ws.add(val);

    expect(ws).to.include(val);
    expect(ws).to.not.include([{ a: 1 }]);
    expect(ws).to.not.include({});

    var sym1 = Symbol(),
      sym2 = Symbol(),
      sym3 = Symbol();
    expect([sym1, sym2]).to.include(sym1);
    expect([sym1, sym2]).to.not.include(sym3);
  });

  it("deep.include()", () => {
    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    expect([obj1, obj2]).to.deep.include({ a: 1 });
    expect([obj1, obj2]).to.not.deep.include({ a: 9 });
    expect([obj1, obj2]).to.not.deep.include({ z: 1 });
    expect({ foo: obj1, bar: obj2 }).to.deep.include({ foo: { a: 1 } });
    expect({ foo: obj1, bar: obj2 }).to.deep.include({
      foo: { a: 1 },
      bar: { b: 2 },
    });
    expect({ foo: obj1, bar: obj2 }).to.not.deep.include({ foo: { a: 9 } });
    expect({ foo: obj1, bar: obj2 }).to.not.deep.include({ foo: { z: 1 } });
    expect({ foo: obj1, bar: obj2 }).to.not.deep.include({ baz: { a: 1 } });
    expect({ foo: obj1, bar: obj2 }).to.not.deep.include({
      foo: { a: 1 },
      bar: { b: 9 },
    });

    var map = new Map();
    map.set(1, [{ a: 1 }]);

    expect(map).to.deep.include([{ a: 1 }]);

    var set = new Set();
    set.add([{ a: 1 }]);

    expect(set).to.deep.include([{ a: 1 }]);
  });

  it("nested.include()", () => {
    expect({ a: { b: ["x", "y"] } }).to.nested.include({ "a.b[1]": "y" });
    expect({ a: { b: ["x", "y"] } }).to.not.nested.include({ "a.b[1]": "x" });
    expect({ a: { b: ["x", "y"] } }).to.not.nested.include({ "a.c": "y" });

    expect({ a: { b: [{ x: 1 }] } }).to.not.nested.include({
      "a.b[0]": { x: 1 },
    });

    expect({ ".a": { "[b]": "x" } }).to.nested.include({ "\\.a.\\[b\\]": "x" });
    expect({ ".a": { "[b]": "x" } }).to.not.nested.include({
      "\\.a.\\[b\\]": "y",
    });
  });

  it("deep.nested.include()", () => {
    expect({ a: { b: [{ x: 1 }] } }).to.deep.nested.include({
      "a.b[0]": { x: 1 },
    });
    expect({ a: { b: [{ x: 1 }] } }).to.not.deep.nested.include({
      "a.b[0]": { y: 2 },
    });
    expect({ a: { b: [{ x: 1 }] } }).to.not.deep.nested.include({
      "a.c": { x: 1 },
    });

    expect({ ".a": { "[b]": { x: 1 } } }).to.deep.nested.include({
      "\\.a.\\[b\\]": { x: 1 },
    });
    expect({ ".a": { "[b]": { x: 1 } } }).to.not.deep.nested.include({
      "\\.a.\\[b\\]": { y: 2 },
    });
  });

  it("own.include()", () => {
    expect({ a: 1 }).to.own.include({ a: 1 });
    expect({ a: 1 }).to.not.own.include({ a: 3 });
    expect({ a: 1 }).to.not.own.include({
      toString: Object.prototype.toString,
    });

    expect({ a: { b: 2 } }).to.not.own.include({ a: { b: 2 } });
  });

  it("deep.own.include()", () => {
    expect({ a: { b: 2 } }).to.deep.own.include({ a: { b: 2 } });
    expect({ a: { b: 2 } }).to.not.deep.own.include({ a: { c: 3 } });
    expect({ a: { b: 2 } }).to.not.deep.own.include({
      toString: Object.prototype.toString,
    });
  });

  it("keys(array|Object|arguments)", () => {
    expect({ foo: 1 }).to.have.keys(["foo"]);
    expect({ foo: 1 }).have.keys({ foo: 6 });
    expect({ foo: 1, bar: 2 }).to.have.keys(["foo", "bar"]);
    expect({ foo: 1, bar: 2 }).to.have.keys("foo", "bar");
    expect({ foo: 1, bar: 2 }).have.keys({ foo: 6, bar: 7 });
    expect({ foo: 1, bar: 2, baz: 3 }).to.contain.keys("foo", "bar");
    expect({ foo: 1, bar: 2, baz: 3 }).to.contain.keys("bar", "foo");
    expect({ foo: 1, bar: 2, baz: 3 }).to.contain.keys("baz");
    expect({ foo: 1, bar: 2 }).contain.keys({ foo: 6 });
    expect({ foo: 1, bar: 2 }).contain.keys({ bar: 7 });
    expect({ foo: 1, bar: 2 }).contain.keys({ foo: 6 });

    expect({ foo: 1, bar: 2 }).to.contain.keys("foo");
    expect({ foo: 1, bar: 2 }).to.contain.keys("bar", "foo");
    expect({ foo: 1, bar: 2 }).to.contain.keys(["foo"]);
    expect({ foo: 1, bar: 2 }).to.contain.keys(["bar"]);
    expect({ foo: 1, bar: 2 }).to.contain.keys(["bar", "foo"]);
    expect({ foo: 1, bar: 2, baz: 3 }).to.contain.all.keys(["bar", "foo"]);

    expect({ foo: 1, bar: 2 }).to.not.have.keys("baz");
    expect({ foo: 1, bar: 2 }).to.not.have.keys("foo");
    expect({ foo: 1, bar: 2 }).to.not.have.keys("foo", "baz");
    expect({ foo: 1, bar: 2 }).to.not.contain.keys("baz");
    expect({ foo: 1, bar: 2 }).to.not.contain.keys("foo", "baz");
    expect({ foo: 1, bar: 2 }).to.not.contain.keys("baz", "foo");

    expect({ foo: 1, bar: 2 }).to.have.any.keys("foo", "baz");
    expect({ foo: 1, bar: 2 }).to.have.any.keys("foo");
    expect({ foo: 1, bar: 2 }).to.contain.any.keys("bar", "baz");
    expect({ foo: 1, bar: 2 }).to.contain.any.keys(["foo"]);
    expect({ foo: 1, bar: 2 }).to.have.all.keys(["bar", "foo"]);
    expect({ foo: 1, bar: 2 }).to.contain.all.keys(["bar", "foo"]);
    expect({ foo: 1, bar: 2 }).contain.any.keys({ foo: 6 });
    expect({ foo: 1, bar: 2 }).have.all.keys({ foo: 6, bar: 7 });
    expect({ foo: 1, bar: 2 }).contain.all.keys({ bar: 7, foo: 6 });

    expect({ foo: 1, bar: 2 }).to.not.have.any.keys("baz", "abc", "def");
    expect({ foo: 1, bar: 2 }).to.not.have.any.keys("baz");
    expect({ foo: 1, bar: 2 }).to.not.contain.any.keys("baz");
    expect({ foo: 1, bar: 2 }).to.not.have.all.keys(["baz", "foo"]);
    expect({ foo: 1, bar: 2 }).to.not.contain.all.keys(["baz", "foo"]);
    expect({ foo: 1, bar: 2 }).not.have.all.keys({ baz: 8, foo: 7 });
    expect({ foo: 1, bar: 2 }).not.contain.all.keys({ baz: 8, foo: 7 });

    var enumProp1 = "enumProp1",
      enumProp2 = "enumProp2",
      nonEnumProp = "nonEnumProp",
      obj = {};

    obj[enumProp1] = "enumProp1";
    obj[enumProp2] = "enumProp2";

    Object.defineProperty(obj, nonEnumProp, {
      enumerable: false,
      value: "nonEnumProp",
    });

    expect(obj).to.have.all.keys([enumProp1, enumProp2]);
    expect(obj).to.not.have.all.keys([enumProp1, enumProp2, nonEnumProp]);

    var sym1 = Symbol("sym1"),
      sym2 = Symbol("sym2"),
      sym3 = Symbol("sym3"),
      str = "str",
      obj = {};

    obj[sym1] = "sym1";
    obj[sym2] = "sym2";
    obj[str] = "str";

    Object.defineProperty(obj, sym3, {
      enumerable: false,
      value: "sym3",
    });

    expect(obj).to.have.all.keys([sym1, sym2, str]);
    expect(obj).to.not.have.all.keys([sym1, sym2, sym3, str]);

    // Not using Map constructor args because not supported in IE 11.
    var aKey = { thisIs: "anExampleObject" },
      anotherKey = { doingThisBecauseOf: "referential equality" },
      testMap = new Map();

    testMap.set(aKey, "aValue");
    testMap.set(anotherKey, "anotherValue");

    expect(testMap).to.have.any.keys(aKey);
    expect(testMap).to.have.any.keys("thisDoesNotExist", "thisToo", aKey);
    expect(testMap).to.have.all.keys(aKey, anotherKey);

    expect(testMap).to.contain.all.keys(aKey);
    expect(testMap).to.not.contain.all.keys(aKey, "thisDoesNotExist");

    expect(testMap).to.not.have.any.keys({ iDoNot: "exist" });
    expect(testMap).to.not.have.any.keys(
      "thisIsNotAkey",
      { iDoNot: "exist" },
      { 33: 20 }
    );
    expect(testMap).to.not.have.all.keys(
      "thisDoesNotExist",
      "thisToo",
      anotherKey
    );

    expect(testMap).to.have.any.keys([aKey]);
    expect(testMap).to.have.any.keys([20, 1, aKey]);
    expect(testMap).to.have.all.keys([aKey, anotherKey]);

    expect(testMap).to.not.have.any.keys([
      { 13: 37 },
      "thisDoesNotExist",
      "thisToo",
    ]);
    expect(testMap).to.not.have.any.keys([20, 1, { 13: 37 }]);
    expect(testMap).to.not.have.all.keys([aKey, { iDoNot: "exist" }]);

    // Using the same assertions as above but with `.deep` flag instead of using referential equality
    expect(testMap).to.have.any.deep.keys({ thisIs: "anExampleObject" });
    expect(testMap).to.have.any.deep.keys("thisDoesNotExist", "thisToo", {
      thisIs: "anExampleObject",
    });

    expect(testMap).to.contain.all.deep.keys({ thisIs: "anExampleObject" });
    expect(testMap).to.not.contain.all.deep.keys(
      { thisIs: "anExampleObject" },
      "thisDoesNotExist"
    );

    expect(testMap).to.not.have.any.deep.keys({ iDoNot: "exist" });
    expect(testMap).to.not.have.any.deep.keys(
      "thisIsNotAkey",
      { iDoNot: "exist" },
      { 33: 20 }
    );
    expect(testMap).to.not.have.all.deep.keys("thisDoesNotExist", "thisToo", {
      doingThisBecauseOf: "referential equality",
    });

    expect(testMap).to.have.any.deep.keys([{ thisIs: "anExampleObject" }]);
    expect(testMap).to.have.any.deep.keys([
      20,
      1,
      { thisIs: "anExampleObject" },
    ]);

    expect(testMap).to.have.all.deep.keys(
      { thisIs: "anExampleObject" },
      { doingThisBecauseOf: "referential equality" }
    );

    expect(testMap).to.not.have.any.deep.keys([
      { 13: 37 },
      "thisDoesNotExist",
      "thisToo",
    ]);
    expect(testMap).to.not.have.any.deep.keys([20, 1, { 13: 37 }]);
    expect(testMap).to.not.have.all.deep.keys([
      { thisIs: "anExampleObject" },
      { iDoNot: "exist" },
    ]);

    var weirdMapKey1 = Object.create(null),
      weirdMapKey2 = { toString: NaN },
      weirdMapKey3 = [],
      weirdMap = new Map();

    weirdMap.set(weirdMapKey1, "val1");
    weirdMap.set(weirdMapKey2, "val2");

    expect(weirdMap).to.have.all.keys([weirdMapKey1, weirdMapKey2]);
    expect(weirdMap).to.not.have.all.keys([weirdMapKey1, weirdMapKey3]);

    var symMapKey1 = Symbol(),
      symMapKey2 = Symbol(),
      symMapKey3 = Symbol(),
      symMap = new Map();

    symMap.set(symMapKey1, "val1");
    symMap.set(symMapKey2, "val2");

    expect(symMap).to.have.all.keys(symMapKey1, symMapKey2);
    expect(symMap).to.have.any.keys(symMapKey1, symMapKey3);
    expect(symMap).to.contain.all.keys(symMapKey2, symMapKey1);
    expect(symMap).to.contain.any.keys(symMapKey3, symMapKey1);

    expect(symMap).to.not.have.all.keys(symMapKey1, symMapKey3);
    expect(symMap).to.not.have.any.keys(symMapKey3);
    expect(symMap).to.not.contain.all.keys(symMapKey3, symMapKey1);
    expect(symMap).to.not.contain.any.keys(symMapKey3);

    var aKey = { thisIs: "anExampleObject" },
      anotherKey = { doingThisBecauseOf: "referential equality" },
      testSet = new Set();

    testSet.add(aKey);
    testSet.add(anotherKey);

    expect(testSet).to.have.any.keys(aKey);
    expect(testSet).to.have.any.keys("thisDoesNotExist", "thisToo", aKey);
    expect(testSet).to.have.all.keys(aKey, anotherKey);

    expect(testSet).to.contain.all.keys(aKey);
    expect(testSet).to.not.contain.all.keys(aKey, "thisDoesNotExist");

    expect(testSet).to.not.have.any.keys({ iDoNot: "exist" });
    expect(testSet).to.not.have.any.keys(
      "thisIsNotAkey",
      { iDoNot: "exist" },
      { 33: 20 }
    );
    expect(testSet).to.not.have.all.keys(
      "thisDoesNotExist",
      "thisToo",
      anotherKey
    );

    expect(testSet).to.have.any.keys([aKey]);
    expect(testSet).to.have.any.keys([20, 1, aKey]);
    expect(testSet).to.have.all.keys([aKey, anotherKey]);

    expect(testSet).to.not.have.any.keys([
      { 13: 37 },
      "thisDoesNotExist",
      "thisToo",
    ]);
    expect(testSet).to.not.have.any.keys([20, 1, { 13: 37 }]);
    expect(testSet).to.not.have.all.keys([aKey, { iDoNot: "exist" }]);

    // Using the same assertions as above but with `.deep` flag instead of using referential equality
    expect(testSet).to.have.any.deep.keys({ thisIs: "anExampleObject" });
    expect(testSet).to.have.any.deep.keys("thisDoesNotExist", "thisToo", {
      thisIs: "anExampleObject",
    });

    expect(testSet).to.contain.all.deep.keys({ thisIs: "anExampleObject" });
    expect(testSet).to.not.contain.all.deep.keys(
      { thisIs: "anExampleObject" },
      "thisDoesNotExist"
    );

    expect(testSet).to.not.have.any.deep.keys({ iDoNot: "exist" });
    expect(testSet).to.not.have.any.deep.keys(
      "thisIsNotAkey",
      { iDoNot: "exist" },
      { 33: 20 }
    );
    expect(testSet).to.not.have.all.deep.keys("thisDoesNotExist", "thisToo", {
      doingThisBecauseOf: "referential equality",
    });

    expect(testSet).to.have.any.deep.keys([{ thisIs: "anExampleObject" }]);
    expect(testSet).to.have.any.deep.keys([
      20,
      1,
      { thisIs: "anExampleObject" },
    ]);

    expect(testSet).to.have.all.deep.keys([
      { thisIs: "anExampleObject" },
      { doingThisBecauseOf: "referential equality" },
    ]);

    expect(testSet).to.not.have.any.deep.keys([
      { 13: 37 },
      "thisDoesNotExist",
      "thisToo",
    ]);
    expect(testSet).to.not.have.any.deep.keys([20, 1, { 13: 37 }]);
    expect(testSet).to.not.have.all.deep.keys([
      { thisIs: "anExampleObject" },
      { iDoNot: "exist" },
    ]);

    var weirdSetKey1 = Object.create(null),
      weirdSetKey2 = { toString: NaN },
      weirdSetKey3 = [],
      weirdSet = new Set();

    weirdSet.add(weirdSetKey1);
    weirdSet.add(weirdSetKey2);

    expect(weirdSet).to.have.all.keys([weirdSetKey1, weirdSetKey2]);
    expect(weirdSet).to.not.have.all.keys([weirdSetKey1, weirdSetKey3]);

    var symSetKey1 = Symbol(),
      symSetKey2 = Symbol(),
      symSetKey3 = Symbol(),
      symSet = new Set();

    symSet.add(symSetKey1);
    symSet.add(symSetKey2);

    expect(symSet).to.have.all.keys(symSetKey1, symSetKey2);
    expect(symSet).to.have.any.keys(symSetKey1, symSetKey3);
    expect(symSet).to.contain.all.keys(symSetKey2, symSetKey1);
    expect(symSet).to.contain.any.keys(symSetKey3, symSetKey1);

    expect(symSet).to.not.have.all.keys(symSetKey1, symSetKey3);
    expect(symSet).to.not.have.any.keys(symSetKey3);
    expect(symSet).to.not.contain.all.keys(symSetKey3, symSetKey1);
    expect(symSet).to.not.contain.any.keys(symSetKey3);
  });

  it("keys(array) will not mutate array (#359)", () => {
    var expected = ["b", "a"];
    var original_order = ["b", "a"];
    var obj = { b: 1, a: 1 };
    expect(expected).deep.equal(original_order);
    expect(obj).keys(original_order);
    expect(expected).deep.equal(original_order);
  });

  it("chaining", () => {
    var tea = { name: "chai", extras: ["milk", "sugar", "smile"] };
    expect(tea).to.have.property("extras").with.lengthOf(3);

    expect(tea).to.have.property("extras").which.contains("smile");

    expect(() => {
      expect(tea).to.have.property("extras").with.lengthOf(4);
    }).to.throw(
      AssertionError,
      "expected [ 'milk', 'sugar', 'smile' ] to have a length of 4 but got 3"
    );

    expect(tea).to.be.a("object").and.have.property("name", "chai");

    var badFn = function () {
      throw new Error("testing");
    };

    expect(badFn).to.throw(Error).with.property("message", "testing");
  });

  it("throw", function () {
    // See GH-45: some poorly-constructed custom errors don't have useful names
    // on either their constructor or their constructor prototype, but instead
    // only set the name inside the constructor itself.
    var PoorlyConstructedError = function () {
      this.name = "PoorlyConstructedError";
    };
    PoorlyConstructedError.prototype = Object.create(Error.prototype);

    function CustomError(message) {
      this.name = "CustomError";
      this.message = message;
    }
    CustomError.prototype = Error.prototype;

    var specificError = new RangeError("boo");

    var goodFn = function () {
        1 == 1;
      },
      badFn = function () {
        throw new Error("testing");
      },
      refErrFn = function () {
        throw new ReferenceError("hello");
      },
      ickyErrFn = function () {
        throw new PoorlyConstructedError();
      },
      specificErrFn = function () {
        throw specificError;
      },
      customErrFn = function () {
        throw new CustomError("foo");
      },
      emptyErrFn = function () {
        throw new Error();
      },
      emptyStringErrFn = function () {
        throw new Error("");
      };

    expect(goodFn).to.not.throw();
    expect(goodFn).to.not.throw(Error);
    expect(goodFn).to.not.throw(specificError);
    expect(badFn).to.throw();
    expect(badFn).to.throw(Error);
    expect(badFn).to.not.throw(ReferenceError);
    expect(badFn).to.not.throw(specificError);
    expect(refErrFn).to.throw();
    expect(refErrFn).to.throw(ReferenceError);
    expect(refErrFn).to.throw(Error);
    expect(refErrFn).to.not.throw(TypeError);
    expect(refErrFn).to.not.throw(specificError);
    expect(ickyErrFn).to.throw();
    expect(ickyErrFn).to.throw(PoorlyConstructedError);
    expect(ickyErrFn).to.throw(Error);
    expect(ickyErrFn).to.not.throw(specificError);
    expect(specificErrFn).to.throw(specificError);

    expect(goodFn).to.not.throw("testing");
    expect(goodFn).to.not.throw(/testing/);
    expect(badFn).to.throw(/testing/);
    expect(badFn).to.not.throw(/hello/);
    expect(badFn).to.throw("testing");
    expect(badFn).to.not.throw("hello");
    expect(emptyStringErrFn).to.throw("");
    expect(emptyStringErrFn).to.not.throw("testing");
    expect(badFn).to.throw("");

    expect(badFn).to.throw(Error, /testing/);
    expect(badFn).to.throw(Error, "testing");
    expect(emptyErrFn).to.not.throw(Error, "testing");

    expect(badFn).to.not.throw(Error, "I am the wrong error message");
    expect(badFn).to.not.throw(TypeError, "testing");

    expect(() => {
      expect(goodFn, "blah").to.throw();
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( goodFn)*\] to throw an error$/
    );

    expect(() => {
      expect(goodFn, "blah").to.throw(ReferenceError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( goodFn)*\] to throw ReferenceError$/
    );

    expect(() => {
      expect(goodFn, "blah").to.throw(specificError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( goodFn)*\] to throw 'RangeError: boo'$/
    );

    expect(() => {
      expect(badFn, "blah").to.not.throw();
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to not throw an error but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(ReferenceError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw 'ReferenceError' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(specificError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw 'RangeError: boo' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.not.throw(Error);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to not throw 'Error' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(refErrFn, "blah").to.not.throw(ReferenceError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( refErrFn)*\] to not throw 'ReferenceError' but 'ReferenceError: hello' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(PoorlyConstructedError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw 'PoorlyConstructedError' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(ickyErrFn, "blah").to.not.throw(PoorlyConstructedError);
    }).to.throw(
      AssertionError,
      /^blah: (expected \[Function( ickyErrFn)*\] to not throw 'PoorlyConstructedError' but)(.*)(PoorlyConstructedError|\{ Object \()(.*)(was thrown)$/
    );

    expect(() => {
      expect(ickyErrFn, "blah").to.throw(ReferenceError);
    }).to.throw(
      AssertionError,
      /^blah: (expected \[Function( ickyErrFn)*\] to throw 'ReferenceError' but)(.*)(PoorlyConstructedError|\{ Object \()(.*)(was thrown)$/
    );

    expect(() => {
      expect(specificErrFn, "blah").to.throw(new ReferenceError("eek"));
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( specificErrFn)*\] to throw 'ReferenceError: eek' but 'RangeError: boo' was thrown$/
    );

    expect(() => {
      expect(specificErrFn, "blah").to.not.throw(specificError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( specificErrFn)*\] to not throw 'RangeError: boo'$/
    );

    expect(() => {
      expect(badFn, "blah").to.not.throw(/testing/);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error not matching \/testing\/$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(/hello/);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error matching \/hello\/ but got 'testing'$/
    );

    expect(() => {
      expect(badFn).to.throw(Error, /hello/, "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error matching \/hello\/ but got 'testing'$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(Error, /hello/);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error matching \/hello\/ but got 'testing'$/
    );

    expect(() => {
      expect(badFn).to.throw(Error, "hello", "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error including 'hello' but got 'testing'$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(Error, "hello");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error including 'hello' but got 'testing'$/
    );

    expect(() => {
      expect(customErrFn, "blah").to.not.throw();
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( customErrFn)*\] to not throw an error but 'CustomError: foo' was thrown$/
    );

    expect(() => {
      expect(badFn).to.not.throw(Error, "testing", "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to not throw 'Error' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.not.throw(Error, "testing");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to not throw 'Error' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(emptyStringErrFn).to.not.throw(Error, "", "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( emptyStringErrFn)*\] to not throw 'Error' but 'Error' was thrown$/
    );

    expect(() => {
      expect(emptyStringErrFn, "blah").to.not.throw(Error, "");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( emptyStringErrFn)*\] to not throw 'Error' but 'Error' was thrown$/
    );

    expect(() => {
      expect(emptyStringErrFn, "blah").to.not.throw("");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( emptyStringErrFn)*\] to throw error not including ''$/
    );

    expect(() => {
      expect({}, "blah").to.throw();
    }).to.throw(AssertionError, "blah: expected {} to be a function");

    expect(() => {
      expect({}).to.throw(Error, "testing", "blah");
    }).to.throw(AssertionError, "blah: expected {} to be a function");
  });

  it("respondTo", () => {
    function Foo() {}
    Foo.prototype.bar = function () {};
    Foo.func = function () {};

    var bar = {};
    bar.foo = function () {};

    expect(Foo).to.respondTo("bar");
    expect(Foo).to.not.respondTo("foo");
    expect(Foo).itself.to.respondTo("func");
    expect(Foo).itself.not.to.respondTo("bar");

    expect(bar).to.respondTo("foo");

    expect(() => {
      expect(Foo).to.respondTo("baz", "constructor");
    }).to.throw(
      AssertionError,
      /^(constructor: expected)(.*)(\[Function Foo\])(.*)(to respond to \'baz\')$/
    );

    expect(() => {
      expect(Foo, "constructor").to.respondTo("baz");
    }).to.throw(
      AssertionError,
      /^(constructor: expected)(.*)(\[Function Foo\])(.*)(to respond to \'baz\')$/
    );

    expect(() => {
      expect(bar).to.respondTo("baz", "object");
    }).to.throw(
      AssertionError,
      /^(object: expected)(.*)(\{ foo: \[Function\] \}|\{ Object \()(.*)(to respond to \'baz\')$/
    );

    expect(() => {
      expect(bar, "object").to.respondTo("baz");
    }).to.throw(
      AssertionError,
      /^(object: expected)(.*)(\{ foo: \[Function\] \}|\{ Object \()(.*)(to respond to \'baz\')$/
    );
  });

  it("satisfy", () => {
    var matcher = function (num) {
      return num === 1;
    };

    expect(1).to.satisfy(matcher);

    expect(() => {
      expect(2).to.satisfy(matcher, "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected 2 to satisfy \[Function( matcher)*\]$/
    );

    expect(() => {
      expect(2, "blah").to.satisfy(matcher);
    }).to.throw(
      AssertionError,
      /^blah: expected 2 to satisfy \[Function( matcher)*\]$/
    );
  });

  it("closeTo", () => {
    expect(1.5).to.be.closeTo(1.0, 0.5);
    expect(10).to.be.closeTo(20, 20);
    expect(-10).to.be.closeTo(20, 30);
  });

  it("approximately", () => {
    expect(1.5).to.be.approximately(1.0, 0.5);
    expect(10).to.be.approximately(20, 20);
    expect(-10).to.be.approximately(20, 30);
  });

  it("oneOf", () => {
    expect(1).to.be.oneOf([1, 2, 3]);
    expect("1").to.not.be.oneOf([1, 2, 3]);
    expect([3, [4]]).to.not.be.oneOf([1, 2, [3, 4]]);
    var threeFour = [3, [4]];
    expect(threeFour).to.be.oneOf([1, 2, threeFour]);
  });

  it("include.members", () => {
    expect([1, 2, 3]).to.include.members([]);
    expect([1, 2, 3]).to.include.members([3, 2]);
    expect([1, 2, 3]).to.include.members([3, 2, 2]);
    expect([1, 2, 3]).to.not.include.members([8, 4]);
    expect([1, 2, 3]).to.not.include.members([1, 2, 3, 4]);
    expect([{ a: 1 }]).to.not.include.members([{ a: 1 }]);

    expect(() => {
      expect([1, 2, 3]).to.include.members([2, 5], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to be a superset of [ 2, 5 ]"
    );

    expect(() => {
      expect([1, 2, 3], "blah").to.include.members([2, 5]);
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to be a superset of [ 2, 5 ]"
    );

    expect(() => {
      expect([1, 2, 3]).to.not.include.members([2, 1]);
    }).to.throw(
      AssertionError,
      "expected [ 1, 2, 3 ] to not be a superset of [ 2, 1 ]"
    );
  });

  it("same.members", () => {
    expect([5, 4]).to.have.same.members([4, 5]);
    expect([5, 4]).to.have.same.members([5, 4]);
    expect([5, 4, 4]).to.have.same.members([5, 4, 4]);
    expect([5, 4]).to.not.have.same.members([]);
    expect([5, 4]).to.not.have.same.members([6, 3]);
    expect([5, 4]).to.not.have.same.members([5, 4, 2]);
    expect([5, 4]).to.not.have.same.members([5, 4, 4]);
    expect([5, 4, 4]).to.not.have.same.members([5, 4]);
    expect([5, 4, 4]).to.not.have.same.members([5, 4, 3]);
    expect([5, 4, 3]).to.not.have.same.members([5, 4, 4]);
  });

  it("members", () => {
    expect([5, 4]).members([4, 5]);
    expect([5, 4]).members([5, 4]);
    expect([5, 4, 4]).members([5, 4, 4]);
    expect([5, 4]).not.members([]);
    expect([5, 4]).not.members([6, 3]);
    expect([5, 4]).not.members([5, 4, 2]);
    expect([5, 4]).not.members([5, 4, 4]);
    expect([5, 4, 4]).not.members([5, 4]);
    expect([5, 4, 4]).not.members([5, 4, 3]);
    expect([5, 4, 3]).not.members([5, 4, 4]);
    expect([{ id: 1 }]).not.members([{ id: 1 }]);

    expect(() => {
      expect([1, 2, 3]).members([2, 1, 5], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have the same members as [ 2, 1, 5 ]"
    );

    expect(() => {
      expect([1, 2, 3], "blah").members([2, 1, 5]);
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have the same members as [ 2, 1, 5 ]"
    );

    expect(() => {
      expect([1, 2, 3]).not.members([2, 1, 3]);
    }).to.throw(
      AssertionError,
      "expected [ 1, 2, 3 ] to not have the same members as [ 2, 1, 3 ]"
    );

    expect(() => {
      expect({}).members([], "blah");
    }).to.throw(AssertionError, "blah: expected {} to be an iterable");

    expect(() => {
      expect({}, "blah").members([]);
    }).to.throw(AssertionError, "blah: expected {} to be an iterable");

    expect(() => {
      expect([]).members({}, "blah");
    }).to.throw(AssertionError, "blah: expected {} to be an iterable");

    expect(() => {
      expect([], "blah").members({});
    }).to.throw(AssertionError, "blah: expected {} to be an iterable");
  });

  it("deep.members", () => {
    expect([{ id: 1 }]).deep.members([{ id: 1 }]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).deep.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect([{ id: 2 }]).not.deep.members([{ id: 1 }]);
    expect([{ a: 1 }, { b: 2 }]).not.deep.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).not.deep.members([
      { a: 1 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).not.deep.members([
      { a: 1 },
      { b: 2 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.deep.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect(() => {
      expect([{ id: 1 }]).deep.members([{ id: 2 }], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ { id: 1 } ] to have the same members as [ { id: 2 } ]"
    );

    expect(() => {
      expect([{ id: 1 }], "blah").deep.members([{ id: 2 }]);
    }).to.throw(
      AssertionError,
      "blah: expected [ { id: 1 } ] to have the same members as [ { id: 2 } ]"
    );
  });

  it("include.deep.members", () => {
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.members([
      { b: 2 },
      { a: 1 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.members([
      { b: 2 },
      { a: 1 },
      { a: 1 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.members([
      { b: 2 },
      { a: 1 },
      { f: 5 },
    ]);

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.members(
        [{ b: 2 }, { a: 1 }, { f: 5 }],
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to be a superset of [ { b: 2 }, { a: 1 }, { f: 5 } ]"
    );

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }], "blah").include.deep.members([
        { b: 2 },
        { a: 1 },
        { f: 5 },
      ]);
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to be a superset of [ { b: 2 }, { a: 1 }, { f: 5 } ]"
    );
  });

  it("ordered.members", () => {
    expect([1, 2, 3]).ordered.members([1, 2, 3]);
    expect([1, 2, 2]).ordered.members([1, 2, 2]);

    expect([1, 2, 3]).not.ordered.members([2, 1, 3]);
    expect([1, 2, 3]).not.ordered.members([1, 2]);
    expect([1, 2]).not.ordered.members([1, 2, 2]);
    expect([1, 2, 2]).not.ordered.members([1, 2]);
    expect([1, 2, 2]).not.ordered.members([1, 2, 3]);
    expect([1, 2, 3]).not.ordered.members([1, 2, 2]);

    expect(() => {
      expect([1, 2, 3]).ordered.members([2, 1, 3], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have the same ordered members as [ 2, 1, 3 ]"
    );

    expect(() => {
      expect([1, 2, 3], "blah").ordered.members([2, 1, 3]);
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have the same ordered members as [ 2, 1, 3 ]"
    );

    expect(() => {
      expect([1, 2, 3]).not.ordered.members([1, 2, 3]);
    }).to.throw(
      AssertionError,
      "expected [ 1, 2, 3 ] to not have the same ordered members as [ 1, 2, 3 ]"
    );
  });

  it("include.ordered.members", () => {
    expect([1, 2, 3]).include.ordered.members([1, 2]);
    expect([1, 2, 3]).not.include.ordered.members([2, 1]);
    expect([1, 2, 3]).not.include.ordered.members([2, 3]);
    expect([1, 2, 3]).not.include.ordered.members([1, 2, 2]);

    expect(() => {
      expect([1, 2, 3]).include.ordered.members([2, 1], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to be an ordered superset of [ 2, 1 ]"
    );

    expect(() => {
      expect([1, 2, 3], "blah").include.ordered.members([2, 1]);
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to be an ordered superset of [ 2, 1 ]"
    );

    expect(() => {
      expect([1, 2, 3]).not.include.ordered.members([1, 2]);
    }).to.throw(
      AssertionError,
      "expected [ 1, 2, 3 ] to not be an ordered superset of [ 1, 2 ]"
    );
  });

  it("deep.ordered.members", () => {
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.deep.ordered.members([
      { b: 2 },
      { a: 1 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }]).not.deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).not.deep.ordered.members([
      { a: 1 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).not.deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).deep.ordered.members(
        [{ b: 2 }, { a: 1 }, { c: 3 }],
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to have the same ordered members as [ { b: 2 }, { a: 1 }, { c: 3 } ]"
    );

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }], "blah").deep.ordered.members([
        { b: 2 },
        { a: 1 },
        { c: 3 },
      ]);
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to have the same ordered members as [ { b: 2 }, { a: 1 }, { c: 3 } ]"
    );

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.deep.ordered.members([
        { a: 1 },
        { b: 2 },
        { c: 3 },
      ]);
    }).to.throw(
      AssertionError,
      "expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to not have the same ordered members as [ { a: 1 }, { b: 2 }, { c: 3 } ]"
    );
  });

  it("include.deep.ordered.members", () => {
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.ordered.members([
      { a: 1 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.ordered.members([
      { b: 2 },
      { a: 1 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.ordered.members([
      { b: 2 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.ordered.members(
        [{ b: 2 }, { a: 1 }],
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to be an ordered superset of [ { b: 2 }, { a: 1 } ]"
    );

    expect(() => {
      expect(
        [{ a: 1 }, { b: 2 }, { c: 3 }],
        "blah"
      ).include.deep.ordered.members([{ b: 2 }, { a: 1 }]);
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to be an ordered superset of [ { b: 2 }, { a: 1 } ]"
    );

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.ordered.members([
        { a: 1 },
        { b: 2 },
      ]);
    }).to.throw(
      AssertionError,
      "expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to not be an ordered superset of [ { a: 1 }, { b: 2 } ]"
    );
  });

  it("change", () => {
    const obj = { value: 10, str: "foo" };
    const heroes = ["spiderman", "superman"];
    const fn = () => (obj.value += 5);
    const decFn = () => (obj.value -= 20);
    const sameFn = () => "foo" + "bar";
    const bangFn = () => (obj.str += "!");
    const batFn = () => heroes.push("batman");
    const lenFn = () => heroes.length;

    expect(fn).to.change(obj, "value");
    expect(fn).to.change(obj, "value").by(5);
    expect(fn).to.change(obj, "value").by(-5);

    expect(decFn).to.change(obj, "value").by(20);
    expect(decFn).to.change(obj, "value").but.not.by(21);

    expect(sameFn).to.not.change(obj, "value");

    expect(sameFn).to.not.change(obj, "str");
    expect(bangFn).to.change(obj, "str");

    expect(batFn).to.change(lenFn).by(1);
    expect(batFn).to.change(lenFn).but.not.by(2);

    expect(() => {
      expect(sameFn).to.change(obj, "value", "blah");
    }).to.throw(AssertionError, "blah: expected .value to change");

    expect(() => {
      expect(sameFn, "blah").to.change(obj, "value");
    }).to.throw(AssertionError, "blah: expected .value to change");

    expect(() => {
      expect(fn).to.not.change(obj, "value", "blah");
    }).to.throw(AssertionError, "blah: expected .value to not change");

    expect(() => {
      expect({}).to.change(obj, "value", "blah");
    }).to.throw(AssertionError, "blah: expected {} to be a function");

    expect(() => {
      expect({}, "blah").to.change(obj, "value");
    }).to.throw(AssertionError, "blah: expected {} to be a function");

    expect(() => {
      expect(fn).to.change({}, "badprop", "blah");
    }).to.throw(AssertionError, "blah: expected {} to have property 'badprop'");

    expect(() => {
      expect(fn, "blah").to.change({}, "badprop");
    }).to.throw(AssertionError, "blah: expected {} to have property 'badprop'");

    expect(() => {
      expect(fn, "blah").to.change({});
    }).to.throw(AssertionError, "blah: expected {} to be a function");

    expect(() => {
      expect(fn).to.change(obj, "value").by(10, "blah");
    }).to.throw(AssertionError, "blah: expected .value to change by 10");

    expect(() => {
      expect(fn, "blah").to.change(obj, "value").by(10);
    }).to.throw(AssertionError, "blah: expected .value to change by 10");

    expect(() => {
      expect(fn).to.change(obj, "value").but.not.by(5, "blah");
    }).to.throw(AssertionError, "blah: expected .value to not change by 5");
  });

  it("increase, decrease", () => {
    var obj = { value: 10, noop: null },
      arr = ["one", "two"],
      pFn = function () {
        arr.push("three");
      },
      popFn = function () {
        arr.pop();
      },
      nFn = function () {
        return null;
      },
      lenFn = function () {
        return arr.length;
      },
      incFn = function () {
        obj.value += 2;
      },
      decFn = function () {
        obj.value -= 3;
      },
      smFn = function () {
        obj.value += 0;
      };

    expect(smFn).to.not.increase(obj, "value");
    expect(decFn).to.not.increase(obj, "value");
    expect(incFn).to.increase(obj, "value");
    expect(incFn).to.increase(obj, "value").by(2);
    expect(incFn).to.increase(obj, "value").but.not.by(1);

    expect(smFn).to.not.decrease(obj, "value");
    expect(incFn).to.not.decrease(obj, "value");
    expect(decFn).to.decrease(obj, "value");
    expect(decFn).to.decrease(obj, "value").by(3);
    expect(decFn).to.decrease(obj, "value").but.not.by(2);

    expect(popFn).to.not.increase(lenFn);
    expect(nFn).to.not.increase(lenFn);
    expect(pFn).to.increase(lenFn);
    expect(pFn).to.increase(lenFn).by(1);
    expect(pFn).to.increase(lenFn).but.not.by(2);

    expect(popFn).to.decrease(lenFn);
    expect(popFn).to.decrease(lenFn).by(1);
    expect(popFn).to.decrease(lenFn).but.not.by(2);
    expect(nFn).to.not.decrease(lenFn);
    expect(pFn).to.not.decrease(lenFn);
  });

  it("extensible", function () {
    const nonExtensibleObject = Object.preventExtensions({});

    expect({}).to.be.extensible;
    expect(nonExtensibleObject).to.not.be.extensible;

    expect(() => {
      expect(nonExtensibleObject, "blah").to.be.extensible;
    }).to.throw(AssertionError, "blah: expected {} to be extensible");

    expect(() => {
      expect({}).to.not.be.extensible;
    }).to.throw(AssertionError, "expected {} to not be extensible");

    expect(42).to.not.be.extensible;
    expect(null).to.not.be.extensible;
    expect("foo").to.not.be.extensible;
    expect(false).to.not.be.extensible;
    expect(undefined).to.not.be.extensible;
    expect(sym).to.not.be.extensible;

    expect(() => {
      expect(42).to.be.extensible;
    }).to.throw(AssertionError, "expected 42 to be extensible");

    expect(() => {
      expect(null).to.be.extensible;
    }).to.throw(AssertionError, "expected null to be extensible");

    expect(() => {
      expect("foo").to.be.extensible;
    }).to.throw(AssertionError, "expected 'foo' to be extensible");

    expect(() => {
      expect(false).to.be.extensible;
    }).to.throw(AssertionError, "expected false to be extensible");

    expect(() => {
      expect(undefined).to.be.extensible;
    }).to.throw(AssertionError, "expected undefined to be extensible");

    const proxy = new Proxy(
      {},
      {
        isExtensible() {
          throw new TypeError();
        },
      }
    );

    expect(() => {
      expect(proxy).to.be.extensible;
    }).to.throw(TypeError);
  });

  it("sealed", function () {
    const sealedObject = Object.seal({});

    expect(sealedObject).to.be.sealed;
    expect({}).to.not.be.sealed;

    expect(() => {
      expect({}).to.be.sealed;
    }).to.throw(AssertionError, "expected {} to be sealed");

    expect(() => {
      expect(sealedObject).to.not.be.sealed;
    }).to.throw(AssertionError, "expected {} to not be sealed");

    expect(42).to.be.sealed;
    expect(null).to.be.sealed;
    expect("foo").to.be.sealed;
    expect(false).to.be.sealed;
    expect(undefined).to.be.sealed;
    expect(sym).to.be.sealed;

    expect(() => {
      expect(42).to.not.be.sealed;
    }).to.throw(AssertionError, "expected 42 to not be sealed");

    expect(() => {
      expect(null).to.not.be.sealed;
    }).to.throw(AssertionError, "expected null to not be sealed");

    expect(() => {
      expect("foo").to.not.be.sealed;
    }).to.throw(AssertionError, "expected 'foo' to not be sealed");

    expect(() => {
      expect(false).to.not.be.sealed;
    }).to.throw(AssertionError, "expected false to not be sealed");

    expect(() => {
      expect(undefined).to.not.be.sealed;
    }).to.throw(AssertionError, "expected undefined to not be sealed");

    const proxy = new Proxy(
      {},
      {
        ownKeys() {
          throw new TypeError();
        },
      }
    );

    Object.preventExtensions(proxy);

    expect(() => {
      expect(proxy).to.be.sealed;
    }).to.throw(TypeError);
  });

  it("frozen", function () {
    const frozenObject = Object.freeze({});

    expect(frozenObject).to.be.frozen;
    expect({}).to.not.be.frozen;

    expect(() => {
      expect({}).to.be.frozen;
    }).to.throw(AssertionError, "expected {} to be frozen");

    expect(() => {
      expect(frozenObject).to.not.be.frozen;
    }).to.throw(AssertionError, "expected {} to not be frozen");

    expect(42).to.be.frozen;
    expect(null).to.be.frozen;
    expect("foo").to.be.frozen;
    expect(false).to.be.frozen;
    expect(undefined).to.be.frozen;
    expect(sym).to.be.frozen;

    expect(() => {
      expect(42).to.not.be.frozen;
    }).to.throw(AssertionError, "expected 42 to not be frozen");

    expect(() => {
      expect(null).to.not.be.frozen;
    }).to.throw(AssertionError, "expected null to not be frozen");

    expect(() => {
      expect("foo").to.not.be.frozen;
    }).to.throw(AssertionError, "expected 'foo' to not be frozen");

    expect(() => {
      expect(false).to.not.be.frozen;
    }).to.throw(AssertionError, "expected false to not be frozen");

    expect(() => {
      expect(undefined).to.not.be.frozen;
    }).to.throw(AssertionError, "expected undefined to not be frozen");

    const proxy = new Proxy(
      {},
      {
        ownKeys() {
          throw new TypeError();
        },
      }
    );

    Object.preventExtensions(proxy);

    expect(() => {
      expect(proxy).to.be.frozen;
    }).to.throw(TypeError);
  });
});

export function runTest(fileData) {
  tests.forEach((test) => test.func());
}
