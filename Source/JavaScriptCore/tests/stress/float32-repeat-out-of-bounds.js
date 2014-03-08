//@ skip
// previously defaultNoEagerRun
// This test fails with reoptimizationRetryCount=1 with ftl-no-cjit.
// Reenable this test after fixing https://bugs.webkit.org/show_bug.cgi?id=129953.

function foo(a) {
    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
}

noInline(foo);

var array = new Float32Array(1);

for (var i = 0; i < 100000; ++i)
    foo(array);

if (reoptimizationRetryCount(foo))
    throw "Error: unexpected retry count: " + reoptimizationRetryCount(foo);
