//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(o) {
    o.f = 1;
}

function fu(o) {
    o.e = 2;
}

function bar(f, o) {
    f(o);
}

for (var i = 0; i < 100; ++i) {
    foo({e:2});
    foo({e:1, f:2});
    fu({f:1, e:2});
    fu({e:1, f:2});
    fu({d:1, e:2, f:3});
}
    
for (var i = 0; i < 100; ++i) {
    bar(foo, {e:1});
    bar(function() { }, null);
    bar(function() { return 42 }, null);
}
    
(function(f, o) {
    var result = 0;
    var n = 1000000;
    for (var i = 0; i < n; ++i) {
        fu(o);
        bar(f, o);
    }
    if (o.e != 2)
        throw "Error: bad value in o.e: " + o.e;
    if (o.f != 1)
        throw "Error: bad value in o.f: " + o.f;
})(foo, {e:42, f:23});

