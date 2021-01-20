// This file tests subclassing arrays.

class A extends Array { }
class B extends A { get 1() { return 1; } }
class C extends B { }

function test() {

    a = new A();
    b = new B();
    c = new C();

    if (!Array.isArray(a) || !Array.isArray(b) || !Array.isArray(c))
        throw "subclasses are not arrays";

    if (!(a instanceof Array && a instanceof A))
        throw "b has incorrect prototype chain";

    if (!(b instanceof Array && b instanceof A && b instanceof B))
        throw "b has incorrect prototype chain";

    if (!(c instanceof Array && c instanceof A && c instanceof B && c instanceof C))
        throw "c has incorrect prototype chain";

    a[1] = 2;
    b[1] = 2;
    c[1] = 2;

    if (a[1] !== 2 || b[1] !== 1 || c[1] !== 1)
        throw "bad indexing type";
}
noInline(test);

for(i = 0; i < 10000; i++)
    test();
