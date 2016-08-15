//@ defaultNoEagerRun

function foo(a) {
    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
}

noInline(foo);

var array = new Int8Array(1);

for (var i = 0; i < 100000; ++i)
    foo(array);

if (reoptimizationRetryCount(foo))
    throw "Error: unexpected retry count: " + reoptimizationRetryCount(foo);
