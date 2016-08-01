// This file tests subclassing various misc constructors.

class A extends ArrayBuffer { }
class B extends Boolean { }
class D extends Date { }
class E extends Error { }
class N extends Number { }
class M extends Map { }
class R extends RegExp { }
class S extends Set { }
class WM extends WeakMap { }
class WS extends WeakSet { }

function test() {

    a = new A(10);
    if (!(a instanceof ArrayBuffer && a instanceof A))
        throw "a has incorrect prototype chain";

    b = new B(true);
    if (!(b instanceof Boolean && b instanceof B))
        throw "b has incorrect prototype chain";

    d = new D();
    if (!(d instanceof Date && d instanceof D))
        throw "d has incorrect prototype chain";

    e = new E();
    if (!(e instanceof Error && e instanceof E))
        throw "e has incorrect prototype chain";

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

    wm = new WM();
    if (!(wm instanceof WeakMap && wm instanceof WM))
        throw "wm has incorrect prototype chain";

    ws = new WS();
    if (!(ws instanceof WeakSet && ws instanceof WS))
        throw "ws has incorrect prototype chain";
}
noInline(test);

for(i = 0; i < 10000; i++)
    test();
