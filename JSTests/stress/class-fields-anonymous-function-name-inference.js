function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// Anonymous functions and classes in non-computed class field initializers
// get their names inferred statically (no SetFunctionName at runtime).
class C {
    f = () => 1;
    g = function () { return 2; };
    h = class {};
    #p = () => 3;
    static s = () => 4;
    "str x" = () => 5;
    0 = () => 6;
    n = class { static name() { return 7; } };
    getP() { return this.#p; }
}

for (var i = 0; i < 1e4; ++i) {
    var c = new C();
    shouldBe(c.f.name, 'f');
    shouldBe(c.g.name, 'g');
    shouldBe(c.h.name, 'h');
    shouldBe(c.getP().name, '#p');
    shouldBe(c['str x'].name, 'str x');
    shouldBe(c[0].name, '0');
}
shouldBe(C.s.name, 's');

// A static "name" method shadows the inferred class name.
shouldBe(typeof new C().n.name, 'function');
shouldBe(new C().n.name(), 7);

// Property descriptor must match runtime SetFunctionName semantics.
{
    let c = new C();
    let desc = Object.getOwnPropertyDescriptor(c.f, 'name');
    shouldBe(desc.value, 'f');
    shouldBe(desc.writable, false);
    shouldBe(desc.enumerable, false);
    shouldBe(desc.configurable, true);
}

// Own property names and their order.
{
    let c = new C();
    shouldBe(Object.getOwnPropertyNames(c.f).join(','), 'length,name');
    shouldBe(Object.keys(c.f).length, 0);
    shouldBe(c.f.hasOwnProperty('name'), true);
}

// delete fn.name falls through to Function.prototype.name.
{
    let c = new C();
    shouldBe(delete c.f.name, true);
    shouldBe(c.f.name, '');
    shouldBe(c.f.hasOwnProperty('name'), false);
}

// Writing is not allowed (ReadOnly), but defineProperty works (configurable).
{
    let c = new C();
    let threw = false;
    try {
        (function () { 'use strict'; c.f.name = 'changed'; })();
    } catch (e) {
        threw = e instanceof TypeError;
    }
    shouldBe(threw, true);
    shouldBe(c.f.name, 'f');
    Object.defineProperty(c.f, 'name', { value: 'redefined' });
    shouldBe(c.f.name, 'redefined');
}

// Instances are independent: reifying one must not affect another.
{
    let a = new C();
    shouldBe(a.f.name, 'f');
    let b = new C();
    Object.defineProperty(a.f, 'name', { value: 'mutated' });
    shouldBe(b.f.name, 'f');
    shouldBe(a.f.name, 'mutated');
}

// length is still derived from the function itself, not the field.
{
    let c = new C();
    shouldBe(c.g.length, 0);
    shouldBe(c.f.length, 0);
}

// bind reads the inferred name.
{
    let c = new C();
    shouldBe(c.f.bind(null).name, 'bound f');
}

// Error.stack shows the inferred name.
{
    class S { e = () => new Error(); }
    let stack = new S().e().stack;
    shouldBe(stack.split('\n')[0].split('@')[0], 'e');
}

// Computed field names still go through runtime SetFunctionName.
{
    function make(key) {
        class D { [key] = () => 1; }
        return new D();
    }
    shouldBe(make('alpha').alpha.name, 'alpha');
    shouldBe(make('beta').beta.name, 'beta');
    let sym = Symbol('x');
    shouldBe(make(sym)[sym].name, '[x]');
}

// Named function expressions in field initializers keep their own name.
{
    class E { m = function inner() {}; }
    shouldBe(new E().m.name, 'inner');
}

// Fields whose initializer is not a function are unaffected.
{
    class F { v = 42; w = 'hi'; }
    let f = new F();
    shouldBe(f.v, 42);
    shouldBe(f.w, 'hi');
}
