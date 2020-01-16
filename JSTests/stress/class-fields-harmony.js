//@ requireOptions("--useClassFields=1")
//@ defaultNoEagerRun

// Copyright 2017 the V8 project authors. All rights reserved.
// Copyright 2019 Igalia S.L.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"use strict";

function classOf(object) {
   // Argument must not be null or undefined.
   var string = Object.prototype.toString.call(object);
   // String has format [object <ClassName>].
   return string.substring(8, string.length - 1);
}

/**
 * Compares two objects for key/value equality.
 * Returns true if they are equal, false otherwise.
 */
function deepObjectEquals(a, b) {
  var aProps = Object.keys(a);
  aProps.sort();
  var bProps = Object.keys(b);
  bProps.sort();
  if (!deepEquals(aProps, bProps)) {
    return false;
  }
  for (var i = 0; i < aProps.length; i++) {
    if (!deepEquals(a[aProps[i]], b[aProps[i]])) {
      return false;
    }
  }
  return true;
}

/**
 * Compares two JavaScript values for type and value equality.
 * It checks internals of arrays and objects.
 */
function deepEquals(a, b) {
  if (a === b) {
    // Check for -0.
    if (a === 0) return (1 / a) === (1 / b);
    return true;
  }
  if (typeof a != typeof b) return false;
  if (typeof a == 'number') return isNaN(a) && isNaN(b);
  if (typeof a !== 'object' && typeof a !== 'function') return false;
  // Neither a nor b is primitive.
  var objectClass = classOf(a);
  if (objectClass !== classOf(b)) return false;
  if (objectClass === 'RegExp') {
    // For RegExp, just compare pattern and flags using its toString.
    return (a.toString() === b.toString());
  }
  // Functions are only identical to themselves.
  if (objectClass === 'Function') return false;
  if (objectClass === 'Array') {
    var elementCount = 0;
    if (a.length != b.length) {
      return false;
    }
    for (var i = 0; i < a.length; i++) {
      if (!deepEquals(a[i], b[i])) return false;
    }
    return true;
  }
  if (objectClass == 'String' || objectClass == 'Number' ||
      objectClass == 'Boolean' || objectClass == 'Date') {
    if (a.valueOf() !== b.valueOf()) return false;
  }
  return deepObjectEquals(a, b);
}

/**
 * Throws an exception containing the user_message (if any) and the values.
 */
function fail(expected, found, user_message = '') {
  // TODO(cira): Replace String with PrettyPrint for objects and arrays.
  var message = 'Failure' + (user_message ? ' (' + user_message + ')' : '') +
      ': expected <' + String(expected) + '>, found <' + String(found) + '>.';
  throw new Error(message);
}

/**
 * Throws if obj is not of given type.
 */
function assertInstanceof(obj, type) {
  if (!(obj instanceof type)) {
    var actualTypeName = null;
    var actualConstructor = Object.getPrototypeOf(obj).constructor;
    if (typeof actualConstructor == "function") {
      actualTypeName = actualConstructor.name || String(actualConstructor);
    }
    throw new Error('Object <' + obj + '> is not an instance of <' +
                    (type.name || type) + '>' +
                    (actualTypeName ? ' but of < ' + actualTypeName + '>' : ''));
  }
}

/**
 * Throws if obj is not the same as expected.
 */
function assertSame(expected, found, name_opt) {
  if (found === expected) {
    if (expected !== 0 || (1 / expected) === (1 / found))
      return;
  } else if ((expected !== expected) && (found !== found)) {
      return;
  }
  fail(expected, found, name_opt);
}

/**
 * Throws if two variables have different types or values.
 */

function assertEquals(expected, found, user_message = '') {
  if (!deepEquals(expected, found)) {
    fail(expected, found, user_message);
  }
}

/**
 * Throws if two variables have equal types or values.
 */

function assertNotEquals(expected, found, user_message = '') {
  if (deepEquals(expected, found)) {
    fail(expected, found, user_message);
  }
}

/**
 * Throws if value is false.
 */
function assertTrue(value, user_message = '') {
  assertEquals(true, value, user_message);
}


/**
 * Throws if value is true.
 */
function assertFalse(value, user_message = '') {
  assertEquals(false, value, user_message);
}

/**
 * Runs code() and asserts that it throws the specified exception.
 */
function assertThrows(code, type_opt, cause_opt) {
  try {
    if (typeof code == 'function') {
      code();
    } else {
      eval(code);
    }
  } catch (e) {
    if (typeof type_opt == 'function') {
      assertInstanceof(e, type_opt);
    }
    if (arguments.length >= 3) {
      assertEquals(cause_opt, e.type, 'thrown exception type mismatch');
    }
    // Success.
    return;
  }
  var expected = arguments.length >= 3 ? cause_opt :
      typeof type_opt == 'function' ? type_opt : 'any exception';
  fail(expected, 'no exception', 'expected thrown exception');
}

{
  class C {
    a;
  }

  assertEquals(undefined, C.a);

  let c = new C;
  let descriptor = Object.getOwnPropertyDescriptor(c, 'a');
  assertTrue(c.hasOwnProperty('a'));
  assertTrue(descriptor.writable);
  assertTrue(descriptor.enumerable);
  assertTrue(descriptor.configurable);
  assertEquals(undefined, c.a);
}

{
  class C {
    x = 1;
    constructor() {}
  }

  let c = new C;
  assertEquals(1, c.x);
}

{
  function t() {
    class X {
      x = 1;
      constructor() {}
    }

    var x = new X;
    return x.x;
  }

  assertEquals(1, t());
}

{
  let x = 'a';
  class C {
    a;
    b = x;
    c = 1;
    hasOwnProperty() { return 1;}
  }

  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals('a', c.b);
  assertEquals(1, c.c);
  assertEquals(undefined, c.d);
  assertEquals(1, c.hasOwnProperty());
}

{
  class C {
    x = Object.freeze(this);
    c = 42;
  }
  assertThrows(() => { new C; }, TypeError);
}

{
  class C {
    c = this;
    d = () => this;
  }

  let c = new C;
  assertEquals(c, c.c);
  assertEquals(c, c.d());

  assertEquals(undefined, C.c);
  assertEquals(undefined, C.d);
}

{
  class C {
    c = 1;
    d = this.c;
  }

  let c = new C;
  assertEquals(1, c.c);
  assertEquals(1, c.d);

  assertEquals(undefined, C.c);
  assertEquals(undefined, C.d);
}

{
  class C {
    b = 1;
    c = () => this.b;
  }

  let c = new C;
  assertEquals(1, c.b);
  assertEquals(1, c.c());

  assertEquals(undefined, C.c);
  assertEquals(undefined, C.b);
}

{
  let x = 'a';
  class C {
    b = 1;
    c = () => this.b;
    e = () => x;
  }

  let c = new C;
  assertEquals(1, c.b);
  assertEquals('a', c.e());

  let a = {b : 2 };
  assertEquals(1, c.c.call(a));

  assertEquals(undefined, C.b);
  assertEquals(undefined, C.c);
}

{
  let x = 'a';
  class C {
    c = 1;
    d = function() { return this.c; };
    e = function() { return x; };
  }

  let c = new C;
  assertEquals(1, c.c);
  assertEquals(1, c.d());
  assertEquals('a', c.e());

  c.c = 2;
  assertEquals(2, c.d());

  let a = {c : 3 };
  assertEquals(3, c.d.call(a));

  assertThrows(c.d.bind(undefined));

  assertEquals(undefined, C.c);
  assertEquals(undefined, C.d);
  assertEquals(undefined, C.e);
}

{
  class C {
    c = function() { return 1 };
  }

  let c = new C;
  assertEquals('c', c.c.name);
}

{
  let d = function() { return new.target; }
  class C {
    c = d;
  }

  let c = new C;
  assertEquals(undefined, c.c());
  assertEquals(new d, new c.c());
}

{
  class C {
    c = () => new.target;
  }

  let c = new C;
  assertEquals(undefined, c.c());
}

{
  let run = false;
  class C {
    c = () => {
      let b;
      class A {
        constructor() {
          b = new.target;
        }
      };
      new A;
      run = true;
      assertEquals(A, b);
    }
  }

  let c = new C;
  c.c();
  assertTrue(run);
}

{
  class C {
    c = new.target;
  }

  let c = new C;
  assertEquals(undefined, c.c);
}

{
  class B {
    c = 1;
  }

  class C extends B {}

  let c = new C;
  assertEquals(1, c.c);
}

{
  assertThrows(() => {
    class C {
      c = new C;
    }
    let c = new C;
  });
}

(function test() {
  function makeC() {
    var x = 1;

    return class {
      a = () => () => x;
    }
  }

  let C = makeC();
  let c = new C;
  let f = c.a();
  assertEquals(1, f());
})()

{
  let c1 = "c";
  class C {
    ["a"] = 1;
    ["b"];
    [c1];
  }

  let c = new C;
  assertEquals(1, c.a);
  assertEquals(undefined, c.b);
  assertEquals(undefined, c[c1]);
}

{
  let log = [];
  function run(i) {
    log.push(i);
    return i;
  }

  class C {
    [run(1)] = run(6);
    [run(2)] = run(7);
    [run(3)]() { run(9);}
    [run(4)] = run(8);
    [run(5)]() { throw new Error('should not execute');};
  }

  let c = new C;
  c[3]();
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], log);
}

function x() {
  // This tests lazy parsing.
  return function() {
    let log = [];
    function run(i) {
      log.push(i);
      return i;
    }

    class C {
      [run(1)] = run(6);
      [run(2)] = run(7);
      [run(3)]() { run(9);}
      [run(4)] = run(8);
      [run(5)]() { throw new Error('should not execute');};
    }

    let c = new C;
    c[3]();
    assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], log);
  }
}
x()();

{
  class C {}
  class D {
    [C];
  }

  let d = new D;
  assertThrows(() => { class X { [X] } let x = new X;});
  assertEquals(undefined, d[C]);
}

{
  class B {
    a = 1;
  }

  class C extends B {
    b = 2;
    constructor() {
      super();
    }
  }

  let c = new C;
  assertEquals(1, c.a);
  assertEquals(2, c.b);
}

{
  var log = [];
  function addToLog(item) { log.push(item); }

  class B {
    a = 1;
    constructor() {
      addToLog("base constructor");
    }
  }

  function initF() {
    addToLog("init f");
    return 1;
  }

  class C extends B {
    f = initF();

    constructor() {
      addToLog("derived constructor");
      var t = () => {
        addToLog("t");
        if (1==-1) {
          super();
        } else {
          super();
        }
      }
      (() => {
        addToLog("anon");
        t();
      })();
    }
  }

  let c = new  C;
  assertEquals(1, c.f);
  assertEquals(1, c.a);
  assertEquals(["derived constructor","anon","t","base constructor","init f"],
               log);
}

{
  class B {
    a = 1;
    returnA = () => this.a;
  }

  class C extends B {
    c = this.a;
    d = 2;
    returnC = () => this.c;
    returnD = () => this.d;
  }

  let c = new C;
  assertEquals(1, c.a);
  assertEquals(1, c.returnA());
  assertEquals(1, c.c);
  assertEquals(1, c.returnA());
  assertEquals(1, c.returnC());
  assertEquals(2, c.d);
  assertEquals(2, c.returnD());

  let c2 = new C;
  assertNotEquals(c2.returnA, c.returnA);
  assertNotEquals(c2.returnC, c.returnC);
  assertNotEquals(c2.returnD, c.returnD);
}

{
  let foo = undefined;
  class B {
    set d(x) {
      foo = x;
    }
  }

  class C extends B {
    d = 2;
  }

  let c = new C;
  assertEquals(undefined, foo);
  assertEquals(2, c.d);
}

{
  class B {}
  class C extends B {
    constructor() {
      super();
    }

    c = 1;
  }

  let c = new C;
  assertEquals(1, c.c);
}

{
  class B {}
  class C extends B {
    constructor() {
      let t = () => {
          super();
      }
      t();
    }

    c = 1;
  }

  let c = new C;
  assertEquals(1, c.c);
}

{
  let log = [];

  class B {}

  class C extends B {

    x = (log.push(1), 1);

    constructor() {
      let t = () => {
        class D extends B {

          x = (log.push(2), 2);

          constructor() {
            let p = () => {
              super();
            }

            p();
          }
        }

        let d = new D();
        assertEquals(2, d.x);
        super();
      }

      t();
    }
  }


  let c = new C;
  assertEquals(1, c.x);
  assertEquals([2, 1], log);
}

{
  let log = [];
  class C1 extends class {} {
    x = log.push(1);
    constructor() {
      var t = () => super();
      super();
      t();
    }
  }

  assertThrows(() => new C1, ReferenceError);
  assertEquals([1], log);

  log = [];
  class C2 extends class {} {
    x = log.push(1);
    constructor() {
      var t = () => super();
      t();
      super();
    }
  }

  assertThrows(() => new C2, ReferenceError);
  assertEquals([1], log);
}

{
  class C1 extends class {} {
    x = 1
    constructor() {
      eval("super()");
    }
  }

  let c = new C1;
  assertEquals(1, c.x);

  class C2 extends class {} {
    x = 1
    constructor() {
      var t = () => {
        eval("super()");
      }
      t();
    }
  }

  c = new C2;
  assertEquals(1, c.x);
}

{
  class C {
    ['x'] = 1;
    ['y'] = 2;
  }

  class C1 extends C {
    ['x'] = 3;
    ['z'] = 4;
  }

  let c = new C1;
  assertEquals(3, c.x);
  assertEquals(2, c.y);
  assertEquals(4, c.z);
}

{
  class X extends class {} {
    c = 1;

    constructor() {
      let t = () => {

        class P extends class {} {
          constructor() {
            let t = () => { super(); };
            t();
          }
        }

        let p = new P;
        assertEquals(undefined, p.c);
        super();
      }

      t();
    }
  }

  let x = new X;
  assertEquals(1, x.c);
}

{
  class A {
    a() { return 1; }
  }

  class C extends A {
    b = super.a();
    c = () => super.a;
    d = () => super.a();
    e = super.a;
    f = super.b;
  }

  let c = new C;
  assertEquals(1, c.a());
  assertEquals(1, c.b);
  assertEquals(1, c.c()());
  assertEquals(1, c.d());
  assertEquals(1, c.e());
  assertFalse(Object.hasOwnProperty(c, 'a'));
  assertEquals(c.a, c.e);
  assertEquals(undefined, c.f);
}

{
  function t() {
    return class {
      ['x'] = 1;
    }
  }

  let klass = t();
  let obj = new klass;
  assertEquals(1, obj.x);
}

{
  new class {
    t = 1;
    constructor(t = this.t) {
      assertEquals(1, t);
    }
  }

  new class extends class {} {
    t = 1;
    constructor(t = (super(), this.t)) {
      assertEquals(1, t);
    }
  }

  assertThrows(() => {
    new class extends class {} {
      t = 1;
      constructor(t = this.t) {
        super();
      }
    }
  }, ReferenceError);
}

{
  class X {
    p = function() { return arguments[0]; }
  }

  let x = new X;
  assertEquals(1, x.p(1));
}

{
  class X {
    t = () => {
      function p() { return arguments[0]; };
      return p;
    }
  }

  let x = new X;
  let p = x.t();
  assertEquals(1, p(1));
}

{
  class X {
    t = () => {
      function p() { return eval("arguments[0]"); };
      return p;
    }
  }

  let x = new X;
  let p = x.t();
  assertEquals(1, p(1));
}

{
  class X {
    p = eval("(function() { return arguments[0]; })(1)");
  }

  let x = new X;
  assertEquals(1, x.p);
}

{
  let thisInInitializer, thisInConstructor, thisFromArrowFn, arrowFn;
  let C = class extends class {} {
    field = (thisInInitializer = this, thisFromArrowFn = arrowFn());
    constructor() {
      arrowFn = () => this;
      super();
      thisInConstructor = this;
    }
  };

  let c = new C();

  assertSame(thisInInitializer, c);
  assertSame(thisFromArrowFn, c);
  assertSame(thisInConstructor, c);
}

