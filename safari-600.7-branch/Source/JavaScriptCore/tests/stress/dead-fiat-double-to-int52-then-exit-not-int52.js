var array = new Float64Array(1);
array[0] = 42;

function foo() {
    fiatInt52(array[0]);
    fiatInt52(array[0]);
}

noInline(foo);

for (var i = 0; i < 1000000; ++i)
    foo();

array[0] = 5.5;
foo();
