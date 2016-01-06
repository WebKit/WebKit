// This file tests subclassing various misc constructors.

class B extends Boolean { }
class N extends Number { }
class M extends Map { }
class R extends RegExp { }
class S extends Set { }

function test() {

    b = new B(true);
    if (!(b instanceof Boolean && b instanceof B))
        throw "b has incorrect prototype chain";
    
    n = new N(10);
    if (!(n instanceof Number && n instanceof N))
        throw "n has incorrect prototype chain";

    m = new M();
    if (!(m instanceof Map && m instanceof M))
        throw "m has incorrect prototype chain";

    r = new R("foo");
    if (!(r instanceof RegExp && r instanceof R))
        throw "r has incorrect prototype chain";

    s = new S();
    if (!(s instanceof Set && s instanceof S))
        throw "s has incorrect prototype chain";
}
noInline(test);

for(i = 0; i < 10000; i++)
    test();
