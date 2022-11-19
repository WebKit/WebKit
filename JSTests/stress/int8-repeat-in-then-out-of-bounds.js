//@ $skipModes << :lockdown
//@ if $jitTests then defaultNoEagerRun(*NO_CJIT_OPTIONS) else skip end

function foo(a, inBounds) {
    a[0] = 1;
    if (!inBounds) {
        a[1] = 2;
        a[2] = 3;
    }
}

noInline(foo);

var array = new Int8Array(1);

for (var i = 0; i < 100000; ++i)
    foo(array, true);

if (reoptimizationRetryCount(foo))
    throw "Error: unexpected retry count: " + reoptimizationRetryCount(foo);

for (var i = 0; i < 100000; ++i)
    foo(array, false);

if (reoptimizationRetryCount(foo) != 1)
    throw "Error: unexpected retry count: " + reoptimizationRetryCount(foo);
