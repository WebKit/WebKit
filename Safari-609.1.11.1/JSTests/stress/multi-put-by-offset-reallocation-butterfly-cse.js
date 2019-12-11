var foos = [
    function(o) { o[0] = 5; o.ff = 42; o[0] = 6; },
    function(o) { o[0] = 5; o.ff = 42; o[0] = 6; },
    function(o) { o[0] = 5; o.ff = 42; o[0] = 6; },
    function(o) { o[0] = 5; o.ff = 42; o[0] = 6; },
    function(o) { o[0] = 5; o.ff = 42; o[0] = 6; },
    function(o) { o[0] = 5; o.ff = 42; o[0] = 6; },
    function(o) { o[0] = 5; o.ff = 42; o[0] = 6; },
    function(o) { o[0] = 5; o.ff = 42; o[0] = 6; }
];

if (foos.length != 8)
    throw "Error";

function bar(o, n) {
    if (n == 0)
        return;
    o.na = 1;
    if (n == 1)
        return;
    o.nb = 2;
    if (n == 2)
        return;
    o.nc = 3;
    if (n == 3)
        return;
    o.nd = 4;
    if (n == 4)
        return;
    o.ne = 5;
    if (n == 5)
        return;
    o.nf = 6;
    if (n == 6)
        return;
    o.ng = 7;
    if (n == 7)
        return;
    o.nh = 8;
}

function baz(o, n) {
    if (n == 0)
        return;
    if (o.na != 1)
        throw "Memory corruption; have o.na = " + o.na;
    if (n == 1)
        return;
    if (o.nb != 2)
        throw "Memory corruption";
    if (n == 2)
        return;
    if (o.nc != 3)
        throw "Memory corruption";
    if (n == 3)
        return;
    if (o.nd != 4)
        throw "Memory corruption";
    if (n == 4)
        return;
    if (o.ne != 5)
        throw "Memory corruption";
    if (n == 5)
        return;
    if (o.nf != 6)
        throw "Memory corruption";
    if (n == 6)
        return;
    if (o.ng != 7)
        throw "Memory corruption";
    if (n == 7)
        return;
    if (o.nh != 8)
        throw "Memory corruption";
}

for (var i = 0; i < 8; ++i)
    noInline(foos[i]);
noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var o = {};
    var p = {a:1, b:2, c:3, d:4, e:5, f:6};
    o[0] = 0;
    p[0] = 0;
    bar(o, i % 8);
    bar(p, i % 8);
    
    foos[i % 8](o);
    foos[i % 8](p);
    
    if (o.ff != 42)
        throw "Bad result in o: " + o.ff;
    if (p.ff != 42)
        throw "Bad result in o: " + p.ff;
    
    if (p.a != 1 || p.b != 2 || p.c != 3 || p.d != 4 || p.e != 5 || p.f != 6)
        throw "Memory corruption"
    baz(o, i % 8);
    baz(p, i % 8);
}

