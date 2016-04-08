(function() {
    function allocate() {
        return {};
    };
    
    for (var i = 0; i < 300; ++i) {
        var o;
        var n = 100;
        for (var j = 0; j < n; ++j) {
            o = allocate();
            o[0] = i + 0;
            o[1] = i + 1;
            o[2] = i + 2;
            o[3] = i + 3;
            o[4] = i + 4;
            o[5] = i + 5;
            o[6] = i + 6;
            o[7] = i + 7;
            o[8] = i + 8;
            o[9] = i + 9;
            o[10] = i + 10;
            o[11] = i + 11;
            o.f = j + 0;
            o.g = j + 1;
            o.h = j + 2;
            o.i = j + 3;
            o.j = j + 4;
            o.k = j + 5;
            o.l = j + 6;
            o.m = j + 7;
            o.n = j + 8;
            o.o = j + 9;
            o.p = j + 10;
            o.q = j + 11;
            o.r = j + 12;
            o.s = j + 13;
            o.t = j + 14;
            o.u = j + 15;
            o.v = j + 16;
            o.w = j + 17;
        }
        
        for (var j = 0; j < 11; ++j) {
            if (o[j] != i + j)
                throw "Error: bad value at o[" + j + "]: " + o[j];
        }
        if (o.f != n - 1 + 0)
            throw "Error: bad value at o.f: " + o.f;
        if (o.g != n - 1 + 1)
            throw "Error: bad value at o.f: " + o.g;
        if (o.h != n - 1 + 2)
            throw "Error: bad value at o.f: " + o.h;
        if (o.i != n - 1 + 3)
            throw "Error: bad value at o.f: " + o.i;
        if (o.j != n - 1 + 4)
            throw "Error: bad value at o.f: " + o.j;
        if (o.k != n - 1 + 5)
            throw "Error: bad value at o.f: " + o.k;
        if (o.l != n - 1 + 6)
            throw "Error: bad value at o.f: " + o.l;
        if (o.m != n - 1 + 7)
            throw "Error: bad value at o.f: " + o.m;
        if (o.n != n - 1 + 8)
            throw "Error: bad value at o.f: " + o.n;
        if (o.o != n - 1 + 9)
            throw "Error: bad value at o.f: " + o.o;
        if (o.p != n - 1 + 10)
            throw "Error: bad value at o.f: " + o.p;
        if (o.q != n - 1 + 11)
            throw "Error: bad value at o.f: " + o.q;
        if (o.r != n - 1 + 12)
            throw "Error: bad value at o.f: " + o.r;
        if (o.s != n - 1 + 13)
            throw "Error: bad value at o.f: " + o.s;
        if (o.t != n - 1 + 14)
            throw "Error: bad value at o.f: " + o.t;
        if (o.u != n - 1 + 15)
            throw "Error: bad value at o.f: " + o.u;
        if (o.v != n - 1 + 16)
            throw "Error: bad value at o.f: " + o.v;
        if (o.w != n - 1 + 17)
            throw "Error: bad value at o.f: " + o.w;
    }
})();
