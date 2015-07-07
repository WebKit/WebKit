var array = new Float64Array(1);
array[0] = 42;

function foo() {
    var value = bar();
    fiatInt52(value);
    fiatInt52(value);
}

function bar() {
    return array[0];
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 1000000; ++i)
    foo();

