//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function Foo() {
    var o = {f:{f:{f:{f:42}}}};
    this.f = 42;
}

noInline(Foo);

for (var i = 0; i < 3000000; ++i) {
    var result = new Foo();
    if (result.f != 42)
        throw "Error: bad result: " + result.f;
}

