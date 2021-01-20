//@ defaultRun; runNoCJITNoASO

function foo(a, b) {
    return a.f / b.f;
}

noInline(foo);

function test(a, b, e) {
    var result = foo({f:a}, {f:b});
    if (result != e)
        throw "Error: " + a + " / " + b + " should be " + e + " but was " + result;
}

for (var i = 1; i < 101; ++i)
    test(i * 2, i, 2);

test(9, 3, 3);
test(12, 4, 3);
test(-32, 8, -4);
test(-21, 7, -3);
test(7, 2, 3.5);
