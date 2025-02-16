// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1566143;
var summary = "Implement the Optional Chain operator (?.) proposal";

print(BUGNUMBER + ": " + summary);

// These tests are originally from webkit.
// webkit specifics have been removed and error messages have been updated.
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowSyntaxError(script) {
    assertThrowsInstanceOf(() => eval(script), SyntaxError);
}

function shouldNotThrowSyntaxError(script) {
    let error;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }

    if ((error instanceof SyntaxError))
        throw new Error('Unxpected SyntaxError!');
}

function shouldThrowTypeError(func, messagePrefix) {
    assertThrowsInstanceOfWithMessageContains(func, TypeError, messagePrefix);
}

function shouldThrowReferenceError(script) {
    assertThrowsInstanceOf(() => eval(script), ReferenceError);
}

function testBasicSuccessCases() {
    shouldBe(undefined?.valueOf(), undefined);
    shouldBe(null?.valueOf(), undefined);
    shouldBe(true?.valueOf(), true);
    shouldBe(false?.valueOf(), false);
    shouldBe(0?.valueOf(), 0);
    shouldBe(1?.valueOf(), 1);
    shouldBe(''?.valueOf(), '');
    shouldBe('hi'?.valueOf(), 'hi');
    shouldBe(({})?.constructor, Object);
    shouldBe(({ x: 'hi' })?.x, 'hi');
    shouldBe([]?.length, 0);
    shouldBe(['hi']?.length, 1);

    shouldBe(undefined?.['valueOf'](), undefined);
    shouldBe(null?.['valueOf'](), undefined);
    shouldBe(true?.['valueOf'](), true);
    shouldBe(false?.['valueOf'](), false);
    shouldBe(0?.['valueOf'](), 0);
    shouldBe(1?.['valueOf'](), 1);
    shouldBe(''?.['valueOf'](), '');
    shouldBe('hi'?.['valueOf'](), 'hi');
    shouldBe(({})?.['constructor'], Object);
    shouldBe(({ x: 'hi' })?.['x'], 'hi');
    shouldBe([]?.['length'], 0);
    shouldBe(['hi']?.[0], 'hi');

    shouldBe(undefined?.(), undefined);
    shouldBe(null?.(), undefined);
    shouldBe((() => 3)?.(), 3);
}

function testBasicFailureCases() {
    shouldThrowTypeError(() => true?.(), 'true is not a function');
    shouldThrowTypeError(() => false?.(), 'false is not a function');
    shouldThrowTypeError(() => 0?.(), '0 is not a function');
    shouldThrowTypeError(() => 1?.(), '1 is not a function');
    shouldThrowTypeError(() => ''?.(), '"" is not a function');
    shouldThrowTypeError(() => 'hi'?.(), '"hi" is not a function');
    shouldThrowTypeError(() => ({})?.(), '({}) is not a function');
    shouldThrowTypeError(() => ({ x: 'hi' })?.(), '({x:"hi"}) is not a function');
    shouldThrowTypeError(() => []?.(), '[] is not a function');
    shouldThrowTypeError(() => ['hi']?.(), '[...] is not a function');
}

testBasicSuccessCases();

testBasicFailureCases();

shouldThrowTypeError(() => ({})?.i(), '(intermediate value).i is not a function');
shouldBe(({}).i?.(), undefined);
shouldBe(({})?.i?.(), undefined);
shouldThrowTypeError(() => ({})?.['i'](), '(intermediate value)["i"] is not a function');
shouldBe(({})['i']?.(), undefined);
shouldBe(({})?.['i']?.(), undefined);

shouldThrowTypeError(() => ({})?.a['b'], '(intermediate value).a is undefined');
shouldBe(({})?.a?.['b'], undefined);
shouldBe(null?.a['b']().c, undefined);
shouldThrowTypeError(() => ({})?.['a'].b, '(intermediate value)["a"] is undefined');
shouldBe(({})?.['a']?.b, undefined);
shouldBe(null?.['a'].b()['c'], undefined);
shouldBe(null?.()().a['b'], undefined);

const o0 = { a: { b() { return this._b.bind(this); }, _b() { return this.__b; }, __b: { c: 42 } } };
shouldBe(o0?.a?.['b']?.()?.()?.c, 42);
shouldBe(o0?.i?.['j']?.()?.()?.k, undefined);
shouldBe((o0.a?._b)?.().c, 42);
shouldBe((o0.a?._b)().c, 42);
shouldBe((o0.a?.b?.())?.().c, 42);
shouldBe((o0.a?.['b']?.())?.().c, 42);

shouldBe(({ undefined: 3 })?.[null?.a], 3);
shouldBe((() => 3)?.(null?.a), 3);

const o1 = { count: 0, get x() { this.count++; return () => {}; } };
o1.x?.y;
shouldBe(o1.count, 1);
o1.x?.['y'];
shouldBe(o1.count, 2);
o1.x?.();
shouldBe(o1.count, 3);
null?.(o1.x);
shouldBe(o1.count, 3);

shouldBe(delete undefined?.foo, true);
shouldBe(delete null?.foo, true);
shouldBe(delete undefined?.['foo'], true);
shouldBe(delete null?.['foo'], true);
shouldBe(delete undefined?.(), true);
shouldBe(delete null?.(), true);
shouldBe(delete ({}).a?.b?.b, true);
shouldBe(delete ({a : {b: undefined}}).a?.b?.b, true);
shouldBe(delete ({a : {b: undefined}}).a?.["b"]?.["b"], true);

const o2 = { x: 0, y: 0, z() {} };
shouldBe(delete o2?.x, true);
shouldBe(o2.x, undefined);
shouldBe(o2.y, 0);
shouldBe(delete o2?.x, true);
shouldBe(delete o2?.['y'], true);
shouldBe(o2.y, undefined);
shouldBe(delete o2?.['y'], true);
shouldBe(delete o2.z?.(), true);

function greet(name) { return `hey, ${name}${this.suffix ?? '.'}`; }
shouldBe(eval?.('greet("world")'), 'hey, world.');
shouldBe(greet?.call({ suffix: '!' }, 'world'), 'hey, world!');
shouldBe(greet.call?.({ suffix: '!' }, 'world'), 'hey, world!');
shouldBe(null?.call({ suffix: '!' }, 'world'), undefined);
shouldBe(({}).call?.({ suffix: '!' }, 'world'), undefined);
shouldBe(greet?.apply({ suffix: '?' }, ['world']), 'hey, world?');
shouldBe(greet.apply?.({ suffix: '?' }, ['world']), 'hey, world?');
shouldBe(null?.apply({ suffix: '?' }, ['world']), undefined);
shouldBe(({}).apply?.({ suffix: '?' }, ['world']), undefined);
shouldThrowSyntaxError('class C {} class D extends C { foo() { return super?.bar; } }');
shouldThrowSyntaxError('class C {} class D extends C { foo() { return super?.["bar"]; } }');
shouldThrowSyntaxError('class C {} class D extends C { constructor() { super?.(); } }');
shouldThrowSyntaxError('const o = { C: class {} }; new o?.C();')
shouldThrowSyntaxError('const o = { C: class {} }; new o?.["C"]();')
shouldThrowSyntaxError('class C {} new C?.();')
shouldThrowSyntaxError('function foo() { new?.target; }');
shouldThrowSyntaxError('function tag() {} tag?.``;');
shouldThrowSyntaxError('const o = { tag() {} }; o?.tag``;');
shouldThrowReferenceError('`${G}`?.r');

// NOT an optional chain
shouldBe(false?.4:5, 5);

// Special case: binary operators that follow a binary expression
shouldThrowReferenceError('(0 || 1 << x)?.$');
shouldThrowReferenceError('(0 || 1 >> x)?.$');
shouldThrowReferenceError('(0 || 1 >>> x)?.$');
shouldThrowReferenceError('(0 || 1 + x)?.$');
shouldThrowReferenceError('(0 || 1 - x)?.$');
shouldThrowReferenceError('(0 || 1 % x)?.$');
shouldThrowReferenceError('(0 || 1 / x)?.$');
shouldThrowReferenceError('(0 || 1 == x)?.$');
shouldThrowReferenceError('(0 || 1 != x)?.$');
shouldThrowReferenceError('(0 || 1 !== x)?.$');
shouldThrowReferenceError('(0 || 1 === x)?.$');
shouldThrowReferenceError('(0 || 1 <= x)?.$');
shouldThrowReferenceError('(0 || 1 >= x)?.$');
shouldThrowReferenceError('(0 || 1 ** x)?.$');
shouldThrowReferenceError('(0 || 1 | x)?.$');
shouldThrowReferenceError('(0 || 1 & x)?.$');
shouldThrowReferenceError('(0 || 1 instanceof x)?.$');
shouldThrowReferenceError('(0 || "foo" in x)?.$');

function testSideEffectCountFunction() {
  let count = 0;
  let a = {
    b: {
      c: {
        d: () => {
          count++;
          return a;
        }
      }
    }
  }

  a.b.c.d?.()?.b?.c?.d

  shouldBe(count, 1);
}

function testSideEffectCountGetters() {
  let count = 0;
  let a = {
    get b() {
      count++;
      return { c: {} };
    }
  }

  a.b?.c?.d;
  shouldBe(count, 1);
  a.b?.c?.d;
  shouldBe(count, 2);
}

testSideEffectCountFunction();
testSideEffectCountGetters();

// stress test SM
shouldBe(({a : {b: undefined}}).a.b?.()()(), undefined);
shouldBe(({a : {b: undefined}}).a.b?.()?.()(), undefined);
shouldBe(({a : {b: () => undefined}}).a.b?.()?.(), undefined);
shouldThrowTypeError(() => delete ({a : {b: undefined}}).a?.b.b.c, '(intermediate value).a.b is undefined');
shouldBe(delete ({a : {b: undefined}}).a?.["b"]?.["b"], true);
shouldBe(delete undefined ?.x[y+1], true);
shouldThrowTypeError(() => (({a : {b: () => undefined}}).a.b?.())(), 'undefined is not a function');
shouldThrowTypeError(() => (delete[1]?.r[delete[1]?.r1]), "[...].r is undefined");
shouldThrowTypeError(() => (delete[1]?.r[[1]?.r1]), "[...].r is undefined");

print("Tests complete");
