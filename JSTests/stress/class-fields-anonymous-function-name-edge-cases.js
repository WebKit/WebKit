function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// The inferred class name is visible while static members evaluate, matching
// NamedEvaluation order: SetFunctionName happens before static fields run.
{
    class O { h = class { static x = this.name; }; }
    shouldBe(new O().h.x, 'h');
    shouldBe(new O().h.name, 'h');
}

// A static "name" field overrides the inferred name, but static fields that
// run before it still observe the inferred name.
{
    class O { n = class { static x = this.name; static name = 42; }; }
    let n = new O().n;
    shouldBe(n.x, 'n');
    shouldBe(n.name, 42);
}

// Many class-valued fields in one class: each constructor must get its own
// field name (stresses the lifetime of identifiers captured by the parser).
{
    class O {
        alpha = class {};
        beta = class {};
        gamma = class {};
        delta = class {};
        epsilon = class {};
    }
    let o = new O();
    shouldBe(o.alpha.name, 'alpha');
    shouldBe(o.beta.name, 'beta');
    shouldBe(o.gamma.name, 'gamma');
    shouldBe(o.delta.name, 'delta');
    shouldBe(o.epsilon.name, 'epsilon');
}

// "__proto__" as a field name is a regular field, and the function gets it as
// a name.
{
    class O { "__proto__" = () => {}; }
    let desc = Object.getOwnPropertyDescriptor(new O(), '__proto__');
    shouldBe(desc.value.name, '__proto__');
}

// Parenthesized anonymous functions are still anonymous function definitions;
// comma, ternary, and eval results are not.
{
    class O {
        p = (() => {});
        c = (0, () => {});
        t = true ? () => {} : 0;
        e = eval('() => {}');
    }
    let o = new O();
    shouldBe(o.p.name, 'p');
    shouldBe(o.c.name, '');
    shouldBe(o.t.name, '');
    shouldBe(o.e.name, '');
}

// All anonymous function flavors are inferred.
{
    class O {
        a = async () => {};
        g = function* () {};
        ag = async function* () {};
        af = async function () {};
    }
    let o = new O();
    shouldBe(o.a.name, 'a');
    shouldBe(o.g.name, 'g');
    shouldBe(o.ag.name, 'ag');
    shouldBe(o.af.name, 'af');
}

// Nested classes with their own function-valued fields.
{
    class O {
        inner = class {
            leaf = () => 1;
            deep = class {};
        };
    }
    let innerInstance = new (new O().inner)();
    shouldBe(new O().inner.name, 'inner');
    shouldBe(innerInstance.leaf.name, 'leaf');
    shouldBe(innerInstance.deep.name, 'deep');
}

// Static and computed fields mix in the same class; computed names keep the
// runtime SetFunctionName path.
{
    function make(key) {
        class D {
            fixed = () => 1;
            [key] = () => 2;
            other = class {};
        }
        return new D();
    }
    let d1 = make('k1');
    let d2 = make('k2');
    shouldBe(d1.fixed.name, 'fixed');
    shouldBe(d1.k1.name, 'k1');
    shouldBe(d1.other.name, 'other');
    shouldBe(d2.k2.name, 'k2');
}

// Names stay correct as the field initializer tiers up.
{
    class O { f = () => 1; k = class {}; }
    for (let i = 0; i < 1e5; ++i) {
        let o = new O();
        if (o.f.name !== 'f' || o.k.name !== 'k')
            throw new Error('bad name at iteration ' + i);
    }
}

// Repeatedly evaluating the class definition itself.
{
    function makeClass() {
        return class { f = () => 1; k = class {}; };
    }
    for (let i = 0; i < 1e3; ++i) {
        let K = makeClass();
        let o = new K();
        shouldBe(o.f.name, 'f');
        shouldBe(o.k.name, 'k');
    }
}

// toString still returns the original source text.
{
    class O { f = () => 1; }
    shouldBe(new O().f.toString(), '() => 1');
}

// A Proxy observing the field definition sees the already-named function.
{
    let observedName;
    class Base {
        constructor() {
            return new Proxy({}, {
                defineProperty(target, key, desc) {
                    observedName = desc.value.name;
                    return Reflect.defineProperty(target, key, desc);
                }
            });
        }
    }
    class O extends Base { f = () => 1; }
    new O();
    shouldBe(observedName, 'f');
}
