function foo(o) {
    return o.f;
}

function bar(o) {
    return o.g;
}

function baz(o, p, q) {
    var result = 0;
    if (isFinalTier()) {
        p = o;
        q = o;
        result += 10000;
    }
    result += foo(p);
    result += bar(q);
    return result;
}

noInline(baz);

for (var i = 0; i < 100000; ++i) {
    var o, p, q;
    var expected1;
    var expected2;
    o = {f:100, g:101};
    expected2 = 10000 + 100 + 101;
    if (i & 1) {
        p = {e:1, f:2, g:3};
        q = {e:4, f:5, g:6};
        expected1 = 2 + 6;
    } else {
        p = {f:7, g:8};
        q = {g:9, f:10};
        expected1 = 7 + 9;
    }
    var result = baz(o, p, q);
    if (result != expected1 && result != expected2)
        throw "Error: bad result: " + result + " (expected " + expected1 + " or " + expected2 + ")";
}

