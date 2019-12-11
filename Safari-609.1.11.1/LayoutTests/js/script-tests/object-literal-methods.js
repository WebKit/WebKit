description("basic tests for object literal methods");

shouldNotThrow("o = { foo() { return 10; } };");
shouldBe("o.foo()", "10");
shouldBe("typeof o.foo", "'function'");
shouldBe("o.foo.length", "0");
shouldBe("o.foo.name", "'foo'");
shouldBe("o.foo.toString()", "'function foo() { return 10; }'");
shouldBe("Object.getOwnPropertyDescriptor(o, 'foo').value", "o.foo");
shouldBeTrue("Object.getOwnPropertyDescriptor(o, 'foo').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(o, 'foo').configurable");
shouldBeTrue("Object.getOwnPropertyDescriptor(o, 'foo').writable");

shouldNotThrow("o = { add(x, y) { return x + y; } };");
shouldBe("o.add(42, -10)", "32");
shouldBe("typeof o.add", "'function'");
shouldBe("o.add.length", "2");
shouldBe("o.add.name", "'add'");
shouldBe("o.add.toString()", "'function add(x, y) { return x + y; }'");

shouldNotThrow("o = { 'add'(a, b, c) { return a + b + c; } };");
shouldBe("o.add(1, 2, 3)", "6");
shouldNotThrow("o = { 'a(d)d'(a, b, c) { return a + b + c; } };");
shouldBe("o['a(d)d'](1, 2, 3)", "6");

shouldNotThrow("o = { 100() { return 100; } };");
shouldBe("o[100]()", "100");
shouldBe("o['100']()", "100");
shouldNotThrow("o = { 100.100() { return 100.100; } };");
shouldBe("o[100.1]()", "100.1");
shouldBe("o['100.1']()", "100.1");
shouldNotThrow("o = { 1e3() { return 1e3; } };");
shouldBe("o[1e3]()", "1000");
shouldBe("o['1000']()", "1000");
shouldNotThrow("o = { 0x11() { return 0x11; } };");
shouldBe("o[0x11]()", "17");
shouldBe("o['17']()", "17");

shouldNotThrow("o = { add([x, y]) { return x + y; } };");
shouldBe("o.add([142, -100])", "42");

a = 1; b = 2; c = 3;
shouldNotThrow("o = { foo() { return 10; }, };");
shouldNotThrow("o = { foo (  ) { return 10; } };");
shouldNotThrow("o = {true(){return true;}};");
shouldNotThrow("o = {NaN(){return NaN;}};");
shouldNotThrow("o = {eval(){return eval;}};");
shouldNotThrow("o = { a:1, foo() { return 10; }, bar() { return 20; }, b: 2 };");
shouldNotThrow("o = { a:1, foo() { return 10; }, bar() { return 20; }, b };");
shouldNotThrow("o = { a:1, foo() { return 10; }, b: b, bar() { return 20; }, c: 2 };");
shouldNotThrow("o = { a:1, foo() { return 10; }, b, bar() { return 20; }, c };");

shouldNotThrow("o = {inner:{method(){ return 100; }}};");
shouldBe("o.inner.method()", "100");

// Duplicate methods are okay.
shouldNotThrow("o = { foo() { return 10; }, foo() { return 20; } };");
shouldBe("o.foo()", "20");

// Method named "get" or "set".
shouldNotThrow("o = { get(x, y) { return x + y; } };");
shouldBe("o.get('hello', 'world')", "'helloworld'");
shouldNotThrow("o = { set(x, y) { return x + y; } };");
shouldBe("o.set('hello', 'world')", "'helloworld'");

// Getter/Setter syntax still works.
shouldBeTrue("({get x(){ return true; }}).x");
shouldBeTrue("({get 'x'(){ return true; }}).x");
shouldBeTrue("({get 42(){ return true; }})['42']");
shouldBeTrue("!!Object.getOwnPropertyDescriptor({set x(value){}}, 'x').set");
shouldBeTrue("!!Object.getOwnPropertyDescriptor({set 'x'(value){}}, 'x').set");
shouldBeTrue("!!Object.getOwnPropertyDescriptor({set 42(value){}}, '42').set");

// Function parse errors.
shouldThrow("({ (){} })");
shouldThrow("({ foo(,,,){} })");
shouldThrow("({ foo(a{}, bar(){} })");
shouldThrow("({ foo(a, b), bar(){} })");
shouldThrow("({ foo(a, b) { if }, bar(){} })");

// __proto__ method should be not modify the prototype.
shouldBeTrue("({__proto__: function(){}}) instanceof Function");
shouldBeFalse("({__proto__(){}}) instanceof Function");
shouldBeTrue("({__proto__(){}}).__proto__ instanceof Function");

shouldThrow("{ f() { return super.f(); } }.f()");
shouldThrow("new ({ f() { return super(); }.f)");
shouldThrow("o = { f() { } }; new ({ __proto__: o, f() { return super(); } }).f");
shouldThrow("({ f() { return (() => super.f())(); } }).f()");
shouldBeTrue("o = { f() { return true; } }; ({ __proto__: o, f() { return super.f(); } }).f()");
shouldBeTrue("o = { get p() { return true; } }; ({ __proto__: o, get p() { return super.p; } }).p");
shouldBeTrue("o = { set p(p2) { } }; ({ __proto__: o, set p(p2) { super.p = p2; } }).p = true");
shouldBeTrue("o = { f() { return true; } }; ({ __proto__: o, f() { return (() => super.f())(); } }).f()");
