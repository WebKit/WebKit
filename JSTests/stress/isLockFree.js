//@ skip

function foo(bytes) {
    return Atomics.isLockFree(bytes);
}
noInline(foo);

function foo0(bytes) {
    return Atomics.isLockFree(0);
}
noInline(foo0);

function foo1(bytes) {
    return Atomics.isLockFree(1);
}
noInline(foo1);

function foo2(bytes) {
    return Atomics.isLockFree(2);
}
noInline(foo2);

function foo3(bytes) {
    return Atomics.isLockFree(3);
}
noInline(foo3);

function foo4(bytes) {
    return Atomics.isLockFree(4);
}
noInline(foo4);

function foo5(bytes) {
    return Atomics.isLockFree(5);
}
noInline(foo5);

function foo6(bytes) {
    return Atomics.isLockFree(6);
}
noInline(foo6);

function foo7(bytes) {
    return Atomics.isLockFree(7);
}
noInline(foo7);

function foo8(bytes) {
    return Atomics.isLockFree(8);
}
noInline(foo8);

function foo9(bytes) {
    return Atomics.isLockFree(9);
}
noInline(foo9);

for (var i = 0; i < 10000; ++i) {
    var result = foo(0);
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo(1);
    if (result !== true)
        throw new Error("Bad result: " + result);
    var result = foo(2);
    if (result !== true)
        throw new Error("Bad result: " + result);
    var result = foo(3);
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo(4);
    if (result !== true)
        throw new Error("Bad result: " + result);
    var result = foo(5);
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo(6);
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo(7);
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo(8);
    if (result !== true)
        throw new Error("Bad result: " + result);
    var result = foo(9);
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo0();
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo1();
    if (result !== true)
        throw new Error("Bad result: " + result);
    var result = foo2();
    if (result !== true)
        throw new Error("Bad result: " + result);
    var result = foo3();
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo4();
    if (result !== true)
        throw new Error("Bad result: " + result);
    var result = foo5();
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo6();
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo7();
    if (result !== false)
        throw new Error("Bad result: " + result);
    var result = foo8();
    if (result !== true)
        throw new Error("Bad result: " + result);
    var result = foo9();
    if (result !== false)
        throw new Error("Bad result: " + result);
}
