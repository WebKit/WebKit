function foo(o) {
    var result = 0;
    for (var i = 0; i != 100; ++i) // ++i still has an overflow check in the emitted code
        result += o.f;
    return result;
}

noInline(foo);

var p = {f:42};
var o = Object.create(p);

for (var i = 0; i < 500000; ++i) {
    p.f = i;
    foo(o);
}

