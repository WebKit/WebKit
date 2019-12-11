function bar() {
    return {f:42};
}

noInline(bar);

function foo0(b) {
    var o = {f:42};
    if (b) {
        var p = bar();
        p.g = o;
        return p;
    }
}

function foo1(b) {
    var o = {f:42};
    if (b) {
        var p = bar();
        p.f1 = 1;
        p.g = o;
        return p;
    }
}

function foo2(b) {
    var o = {f:42};
    if (b) {
        var p = bar();
        p.f1 = 1;
        p.f2 = 2;
        p.g = o;
        return p;
    }
}

function foo3(b) {
    var o = {f:42};
    if (b) {
        var p = bar();
        p.f1 = 1;
        p.f2 = 2;
        p.f3 = 3;
        p.g = o;
        return p;
    }
}

function foo4(b) {
    var o = {f:42};
    if (b) {
        var p = bar();
        p.f1 = 1;
        p.f2 = 2;
        p.f3 = 3;
        p.f4 = 4;
        p.g = o;
        return p;
    }
}

noInline(foo0);
noInline(foo1);
noInline(foo2);
noInline(foo3);
noInline(foo4);

var array = new Array(1000);
for (var i = 0; i < 400000; ++i) {
    var o = foo0(true);
    array[i % array.length] = o;
}
for (var i = 0; i < 400000; ++i) {
    var o = foo1(true);
    array[i % array.length] = o;
}
for (var i = 0; i < 400000; ++i) {
    var o = foo2(true);
    array[i % array.length] = o;
}
for (var i = 0; i < 400000; ++i) {
    var o = foo3(true);
    array[i % array.length] = o;
}
for (var i = 0; i < 400000; ++i) {
    var o = foo4(true);
    array[i % array.length] = o;
}

