function foo() {
    var value = bar(DFGTrue());
    fiatInt52(value);
    fiatInt52(value);
}

function bar(p) {
    return p ? 42 : 5.5;
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 1000000; ++i)
    foo();

