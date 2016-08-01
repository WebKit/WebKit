function foo(o) {
    o.a = 0
    o.b = 1
    o.c = 2
    o.d = 3
    o.e = 4
    o.f = 5
    o.g = 6
    o.h = 7
    o.i = 8
    o.j = 9
    o.k = 10
    o.l = 11
    o.m = 12
    o.n = 13
    o.o = 14
    o.p = 15
    o.q = 16
    o.r = 17
    o.s = 18
    o.t = 19
    o.u = 20
    o.v = 21
    o.w = 22
    o.x = 23
    o.y = 24
    o.z = 25
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var o = {};
    foo(o);
    if (o.a != 0)
        throw "Error: bad value for a: " + o.a;
    if (o.b != 1)
        throw "Error: bad value for b: " + o.b;
    if (o.c != 2)
        throw "Error: bad value for c: " + o.c;
    if (o.d != 3)
        throw "Error: bad value for d: " + o.d;
    if (o.e != 4)
        throw "Error: bad value for e: " + o.e;
    if (o.f != 5)
        throw "Error: bad value for f: " + o.f;
    if (o.g != 6)
        throw "Error: bad value for g: " + o.g;
    if (o.h != 7)
        throw "Error: bad value for h: " + o.h;
    if (o.i != 8)
        throw "Error: bad value for i: " + o.i;
    if (o.j != 9)
        throw "Error: bad value for j: " + o.j;
    if (o.k != 10)
        throw "Error: bad value for k: " + o.k;
    if (o.l != 11)
        throw "Error: bad value for l: " + o.l;
    if (o.m != 12)
        throw "Error: bad value for m: " + o.m;
    if (o.n != 13)
        throw "Error: bad value for n: " + o.n;
    if (o.o != 14)
        throw "Error: bad value for o: " + o.o;
    if (o.p != 15)
        throw "Error: bad value for p: " + o.p;
    if (o.q != 16)
        throw "Error: bad value for q: " + o.q;
    if (o.r != 17)
        throw "Error: bad value for r: " + o.r;
    if (o.s != 18)
        throw "Error: bad value for s: " + o.s;
    if (o.t != 19)
        throw "Error: bad value for t: " + o.t;
    if (o.u != 20)
        throw "Error: bad value for u: " + o.u;
    if (o.v != 21)
        throw "Error: bad value for v: " + o.v;
    if (o.w != 22)
        throw "Error: bad value for w: " + o.w;
    if (o.x != 23)
        throw "Error: bad value for x: " + o.x;
    if (o.y != 24)
        throw "Error: bad value for y: " + o.y;
    if (o.z != 25)
        throw "Error: bad value for z: " + o.z;
}
