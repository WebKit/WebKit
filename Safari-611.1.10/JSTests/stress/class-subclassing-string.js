A = class extends String { }
B = class extends A { get 4() { return 1; } }
C = class extends B { }

A.prototype[3] = 1;

function test() {
    let a = new A("foo");
    let b = new B("baz");
    let c = new C("bar");

    // String objects have a non-writable length property
    a.length = 1;
    b.length = 1;
    c.length = 1;

    if (a.length !== 3 || b.length !== 3 || c.length !== 3)
        throw "not string objects";

    if (!(a instanceof A && a instanceof String))
        throw "a has incorrect prototype chain";

    if (!(b instanceof B && b instanceof A && b instanceof String))
        throw "b has incorrect prototype chain";

    if (!(c instanceof C && c instanceof B && c instanceof A && c instanceof String))
        throw "c has incorrect prototype chain";

    if (a[4] !== undefined || b[4] !== 1 || c[4] !== 1)
        throw "bad indexing type with accessors on chain";

    if (a[3] !== 1 || b[3] !== 1 || c[3] !== 1)
        throw "bad indexing type with values on chain";
}
noInline(test);

for (i = 0; i < 10000; i++)
    test();
