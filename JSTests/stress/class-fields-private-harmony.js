// Copyright 2018 the V8 project authors. All rights reserved.
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
    #a;
    getA() { return this.#a; }
  }

  assertEquals(undefined, C.a);

  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals(undefined, c.getA());
}

{
  class C {
    #a = 1;
    getA() { return this.#a; }
  }

  assertEquals(undefined, C.a);

  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals(1, c.getA());
}

{
  class C {
    #a = 1;
    #b = this.#a;
    getB() { return this.#b; }
  }

  let c = new C;
  assertEquals(1, c.getB());
}

{
  class C {
    #a = 1;
    getA() { return this.#a; }
    constructor() {
      assertEquals(1, this.#a);
      this.#a = 5;
    }
  }

  let c = new C;
  assertEquals(5, c.getA());
}

{
  class C {
    #a = this;
    #b = () => this;
    getA() { return this.#a; }
    getB() { return this.#b; }
  }

  let c1 = new C;
  assertSame(c1, c1.getA());
  assertSame(c1, c1.getB()());
  let c2 = new C;
  assertSame(c1, c1.getB().call(c2));
}

{
  class C {
    #a = this;
    #b = function() { return this; };
    getA() { return this.#a; }
    getB() { return this.#b; }
  }

  let c1 = new C;
  assertSame(c1, c1.getA());
  assertSame(c1, c1.getB().call(c1));
  let c2 = new C;
  assertSame(c2, c1.getB().call(c2));
}


{
  class C {
    #a = function() { return 1 };
    getA() {return this.#a;}
  }

  let c = new C;
  assertEquals('#a', c.getA().name);
}

{
  let d = function() { return new.target; }
  class C {
    #c = d;
    getC() { return this.#c; }
  }

  let c = new C;
  assertEquals(undefined, c.getC()());
  assertSame(new d, new (c.getC()));
}

{
  class C {
    #b = new.target;
    #c = () => new.target;
    getB() { return this.#b; }
    getC() { return this.#c; }
  }

  let c = new C;
  assertEquals(undefined, c.getB());
  assertEquals(undefined, c.getC()());
}

{
  class C {
    #a = 1;
    #b = () => this.#a;
    getB() { return this.#b; }
  }

  let c1 = new C;
  assertSame(1, c1.getB()());
}

{
  class C {
    #a = 1;
    getA(instance) { return instance.#a; }
  }

  class B { }
  let c = new C;
  assertEquals(undefined, c.a);
  assertEquals(1, c.getA(c));

  assertThrows(() => c.getA(new B), TypeError);
}

{
  class A {
    #a = 1;
    getA() { return this.#a; }
  }

  class B extends A {}
  let b = new B;
  assertEquals(1, b.getA());
}

{
  let prototypeLookup = false;
  class A {
    set a(val) {
      prototypeLookup = true;
    }

    get a() { return undefined; }
  }

  class C extends A {
    #a = 1;
    getA() { return this.#a; }
  }

  let c = new C;
  assertEquals(1, c.getA());
  assertEquals(false, prototypeLookup);
}

{
  class A {
    constructor() { this.a = 1; }
  }

  class B extends A {
    #b = this.a;
    getB() { return this.#b; }
  }

  let b = new B;
  assertEquals(1, b.getB());
}

{
  class A {
    #a = 1;
    getA() { return this.#a; }
  }

  class B extends A {
    #b = super.getA();
    getB() { return this.#b; }
  }

  let b = new B;
  assertEquals(1, b.getB());
}

{
  class A {
    #a = 1;
    getA() { return this.#a;}
  }

  class B extends A {
    #a = 2;
    get_A() { return this.#a;}
  }

  let a = new A;
  let b = new B;
  assertEquals(1, a.getA());
  assertEquals(1, b.getA());
  assertEquals(2, b.get_A());
}

{
  let foo = undefined;
  class A {
    #a = 1;
    constructor() {
      foo = this.#a;
    }
  }

  let a = new A;
  assertEquals(1, foo);
}

{
  let foo = undefined;
  class A extends class {} {
    #a = 1;
    constructor() {
      super();
      foo = this.#a;
    }
  }

  let a = new A;
  assertEquals(1, foo);
}

{
  function makeClass() {
    return class {
      #a;
      setA(val) { this.#a = val; }
      getA() { return this.#a; }
    }
  }

  let classA = makeClass();
  let a = new classA;
  let classB = makeClass();
  let b = new classB;

  assertEquals(undefined, a.getA());
  assertEquals(undefined, b.getA());

  a.setA(3);
  assertEquals(3, a.getA());
  assertEquals(undefined, b.getA());

  b.setA(5);
  assertEquals(3, a.getA());
  assertEquals(5, b.getA());

  assertThrows(() => a.getA.call(b), TypeError);
  assertThrows(() => b.getA.call(a), TypeError);
}

{
  let value = undefined;

  new class {
    #a = 1;
    getA() { return this.#a; }

    constructor() {
      new class {
        #a = 2;
        constructor() {
          value = this.#a;
        }
      }
    }
  }

  assertEquals(2, value);
}

{
  class A {
    #a = 1;
    b = class {
      getA() { return this.#a; }
      get_A(val) { return val.#a; }
    }
  }

  let a = new A();
  let b = new a.b;
  assertEquals(1, b.getA.call(a));
  assertEquals(1, b.get_A(a));
}

{
  class C {
    b = this.#a;
    #a = 1;
  }

  assertThrows(() => new C, TypeError);
}

{
  class C {
    #b = this.#a;
    #a = 1;
  }

  assertThrows(() => new C, TypeError);
}

{
  let symbol = Symbol();

  class C {
    #a = 1;
    [symbol] = 1;
    getA() { return this.#a; }
    setA(val) { this.#a = val; }
  }

  var p = new Proxy(new C, {
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
    #b = Object.freeze(this);
    #a = 1;
    getA() { return this.#a; }
  }

  let c = new C;
  assertEquals(1, c.getA());
}

{
  class C {
    #a = 1;
    setA(another, val) { another.#a = val; }
    getA(another) { return another.#a; }
  }

  let c = new C;
  assertThrows(() => c.setA({}, 2), TypeError);
  c.setA(c, 3);
  assertEquals(3, c.getA(c));
}

{
  class A {
    constructor(arg) {
      return arg;
    }
  }

  class C extends A {
    #x = 1;

    constructor(arg) {
      super(arg);
    }

    getX(arg) {
      return arg.#x;
    }
  }

  let leaker = new Proxy({}, {});
  let c = new C(leaker);
  assertEquals(1, C.prototype.getX(leaker));
  assertSame(c, leaker);

  c = new C();
  assertThrows(() => new C(c), TypeError);

  new C(1);
}

{
  class C {
    #a = 1;
    b;
    getA() { return this.b().#a; }
  }

  let c = new C();
  c.b = () => c;
  assertEquals(1, c.getA());
}

{
  class C {
    #a = 1;
    b;
    getA(arg) { return arg.b().#a; }
  }

  let c = new C();
  c.b = () => c;
  assertEquals(1, c.getA(c));
}

{
  class C {
    #a = 1;
    getA() { return eval('this.#a'); }
  }

  let c = new C;
  assertEquals(1, c.getA());
}

{
  var C;
  eval('C = class {#a = 1;getA() { return eval(\'this.#a\'); }}');

  let c = new C;
  assertEquals(1, c.getA());
}

{
  class C {
    #a = 1;
    getA() { return this.#a; }
    setA() { eval('this.#a = 4'); }
  }
  let c = new C;
  assertEquals(1, c.getA());
  c.setA();
  assertEquals(4, c.getA());
}

{
  class C {
    getA() { return eval('this.#a'); }
  }

  let c = new C;
  assertThrows(() => c.getA(), SyntaxError);
}

{
  assertThrows(() => {
    class A {
      [this.#a] = 1;
      #a = 2;
    }
  }, TypeError);
}
