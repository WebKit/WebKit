//@ defaultNoEagerRun

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

load("./resources/harmony-support.js");

{
  class C {
    static #a;
    static getA() { return this.#a; }
  }

  assertEquals(undefined, C.a);
  assertEquals(undefined, C.getA());

  let c = new C;
  assertEquals(undefined, c.a);
}

{
  class C {
    static #a = 1;
    static getA() { return this.#a; }
  }

  assertEquals(undefined, C.a);
  assertEquals(1, C.getA());

  let c = new C;
  assertEquals(undefined, c.a);
}

{
  class C {
    static #a = 1;
    static #b = this.#a;
    static getB() { return this.#b; }
  }

  assertEquals(1, C.getB());

  let c = new C;
  assertEquals(undefined, c.getB);
}

{
  class C {
    static #a = 1;
    static getA() { return this.#a; }
    constructor() {
      assertThrows(() => this.#a, TypeError);
      C.#a = 2;
    }
  }

  assertEquals(1, C.getA());

  let c = new C;
  assertThrows(() => C.prototype.getA.call(c));
  assertEquals(2, C.getA());
}

{
  class C {
    static #a = this;
    static #b = () => this;
    static getA() { return this.#a; }
    static getB() { return this.#b; }
  }

  assertSame(C, C.getA());
  assertSame(C, C.getB()());
}

{
  class C {
    static #a = this;
    static #b = function() { return this; };
    static getA() { return this.#a; }
    static getB() { return this.#b; }
  }

  assertSame(C, C.getA());
  assertSame(C, C.getB().call(C));
  assertSame(undefined, C.getB()());
}


{
  class C {
    static #a = function() { return 1 };
    static getA() {return this.#a;}
  }

  assertEquals('#a', C.getA().name);
}

{
  let d = function() { return new.target; }
  class C {
    static #c = d;
    static getC() { return this.#c; }
  }

  assertEquals(undefined, C.getC()());
  assertSame(new d, new (C.getC()));
}

{
  class C {
    static #a = 1;
    static getA(instance) { return instance.#a; }
  }

  class B { }

  assertEquals(undefined, C.a);
  assertEquals(1, C.getA(C));
  assertThrows(() => C.getA(B), TypeError);
}

{
  class A {
    static #a = 1;
    static getA() { return this.#a; }
  }

  class B extends A {}
  assertThrows(() => B.getA(), TypeError);
}

{
  class A {
    static #a = 1;
    static getA() { return A.#a; }
  }

  class B extends A {}
  assertSame(1, B.getA());
}

{
  let prototypeLookup = false;
  class A {
    static set a(val) {
      prototypeLookup = true;
    }

    static get a() { return undefined; }
  }

  class C extends A {
    static #a = 1;
    static getA() { return this.#a; }
  }

  assertEquals(1, C.getA());
  assertEquals(false, prototypeLookup);
}

{
  class A {
    static a = 1;
  }

  class B extends A {
    static #b = this.a;
    static getB() { return this.#b; }
  }

  assertEquals(1, B.getB());
}

{
  class A {
    static #a = 1;
    static getA() { return this.#a; }
  }

  class B extends A {
    static getA() { return super.getA(); }
  }

  assertThrows(() => B.getA(), TypeError);
}

{
  class A {
    static #a = 1;
    static getA() { return this.#a;}
  }

  class B extends A {
    static #a = 2;
    static get_A() { return this.#a;}
  }

  assertEquals(1, A.getA());
  assertThrows(() => B.getA(), TypeError);
  assertEquals(2, B.get_A());
}

{
  let foo = undefined;
  class A {
    static #a = (function() { foo = 1; })();
  }

  assertEquals(1, foo);
}

{
  let foo = undefined;
  class A extends class {} {
    static #a = (function() { foo = 1; })();
  }

  assertEquals(1, foo);
}

{
  function makeClass() {
    return class {
      static #a;
      static setA(val) { this.#a = val; }
      static getA() { return this.#a; }
    }
  }

  let classA = makeClass();
  let classB = makeClass();

  assertEquals(undefined, classA.getA());
  assertEquals(undefined, classB.getA());

  classA.setA(3);
  assertEquals(3, classA.getA());
  assertEquals(undefined, classB.getA());

  classB.setA(5);
  assertEquals(3, classA.getA());
  assertEquals(5, classB.getA());

  assertThrows(() => classA.getA.call(classB), TypeError);
  assertThrows(() => classB.getA.call(classA), TypeError);
}

{
  let value = undefined;

  new class {
    static #a = 1;
    static getA() { return this.#a; }

    constructor() {
      new class C {
        static #a = 2;
        constructor() {
          value = C.#a;
        }
      }
    }
  }

  assertEquals(2, value);
}

{
  class A {
    static #a = 1;
    static b = class {
      static getA() { return this.#a; }
      static get_A(val) { return val.#a; }
    }
  }

  assertEquals(1, A.b.getA.call(A));
  assertEquals(1, A.b.get_A(A));
}

{
  assertThrows(() => class { static b = this.#a; static #a = 1 }, TypeError);
}

{
  let symbol = Symbol();

  class C {
    static #a = 1;
    static [symbol] = 1;
    static getA() { return this.#a; }
    static setA(val) { this.#a = val; }
  }

  var p = new Proxy(C, {
    get: function(target, name) {
      if (typeof(name) === 'symbol') {
        assertFalse($vm.isPrivateSymbol(name));
      }
      return target[name];
    }
  });

  assertThrows(() => p.getA(), TypeError);
  assertThrows(() => p.setA(1), TypeError);
  assertEquals(1, p[symbol]);
}

{
  class C {
    static #b = Object.freeze(this);
    static getA() { return this.#a; }
    static #a = 1;
  }

  assertEquals(1, C.getA());
}

{
  class C {
    static #a = 1;
    static getA() { return eval('this.#a'); }
  }

  assertEquals(1, C.getA());
}

{
  var C;
  eval('C = class { static #a = 1; static getA() { return eval(\'this.#a\'); }}');

  assertEquals(1, C.getA());
}

{
  class C {
    static #a = 1;
    static getA() { return this.#a; }
    static setA() { eval('this.#a = 4'); }
  }

  assertEquals(1, C.getA());
  C.setA();
  assertEquals(4, C.getA());
}

{
  class C {
    static getA() { return eval('this.#a'); }
  }

  assertThrows(() => C.getA(), SyntaxError);
}

// Additional tests by the WebKit project.
function shouldThrowSyntaxError(script) {
    let error;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }

    if (!(error instanceof SyntaxError))
        throw new Error('Expected SyntaxError!');
}

shouldThrowSyntaxError("class C { #foo = 0; static #foo = 1; }");
shouldThrowSyntaxError("class C { static #foo = 0; #foo = 1; }");

{
  class C {
    static #foo = 0;
    testAccess(X) { return X.#foo; }
  }

  class D {
    static #foo = 0;
  }

  let c = new C();
  assertDoesNotThrow(c.testAccess(C));
  assertThrows(() => c.testAccess(D), TypeError);
}
