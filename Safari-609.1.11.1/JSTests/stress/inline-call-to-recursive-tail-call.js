"use strict";

// factorial calls aux that tail-calls back into factorial.
// It is not OK to turn that tail call into a jump back to the top of the function, because the call to aux is not a tail call.
function factorial(n) {
    function aux(n) {
        if (n == 1)
            return 1;
        return factorial(n - 1);
    }

    return n * aux(n);
}

// Same, but this time with an irrelevant tail call in factorial.
// This exercises a different code path because the recursive tail call optimization aborts early if the op_enter block is not split, and it is split by the detection of a tail call.
function factorial2(n) {
    function aux2(n) {
        if (n == 1)
            return 1;
        return factorial2(n - 1);
    }
    function id(n) {
        return n;
    }

    return id(n * aux2(n));
}

// This time, aux is tail-calling itself: the optimization is valid but must jump to the start of aux3, not of factorial3.
function factorial3(n) {
    function aux3(n, acc) {
        if (n == 1)
            return acc;
        return aux3 (n-1, n*acc);
    }

    return n * aux3(n-1, 1);
}

// In this case, it is valid to jump all the way to the top of factorial4, because all calls are tail calls.
function factorial4(n, acc) {
    function aux4(n, acc) {
        if (n == 1)
            return acc;
        return factorial4(n-1, n*acc);
    }

    if (acc)
        return aux4(n, acc);
    return aux4(n, 1);
}

// This function is used to test that recursing with a different number of arguments doesn't mess up with the stack.
// The first two tail calls should be optimized, but not the last one (no place on the stack for the third argument).
function foo(a, b) {
    if (a == 0)
        return 42;
    if (a == 1)
        return foo(a - 1);
    if (a == 2)
        return foo(b - 1, a);
    return foo (b - 1, a, 43);
}

// Same deal as foo, just with an inlining thrown into the mix.
// Even the first tail call should not be optimized in this case, because some code in the compiler may constant-fold the number of arguments in that case.
function bar(x, y) {
    function auxBar(a, b) {
        if (a == 0)
            return 42;
        if (a == 1)
            return auxBar(a - 1);
        if (a == 2)
            return auxBar(b - 1, a);
        return auxBar(b - 1, a, 43);
    }

    return auxBar(x, y);
}

function test(result, expected, name) {
    if (result != expected)
        throw "Wrong result for " + name + ": " + result + " instead of " + expected;
}

for (var i = 0; i < 10000; ++i) {
    test(factorial(20), 2432902008176640000, "factorial");
    test(factorial2(20), 2432902008176640000, "factorial2");
    test(factorial3(20), 2432902008176640000, "factorial3");
    test(factorial4(20), 2432902008176640000, "factorial4");
    test(foo(10, 10), 42, "foo");
    test(bar(10, 10), 42, "bar");
}
