disableRichSourceInfo();

expectedError = "ReferenceError: Cannot access uninitialized variable.";

function shouldThrow(func) {
    var actualError = false;
    try {
        func();
        throw new Error('func should have thrown');
    } catch (e) {
        actualError = e;
    }
    if (String(actualError) !== expectedError)
        throw new Error('\nActual Value:' + actualError + '\nExpected Value:' + expectedError);
}

function test1() {
    loadString("a = 42");
}

function test2() {
    loadString("a.f = 42");
}

function test3() {
    loadString("a[0] = 42;");
}

function test4() {
    loadString("a = 'root'; b = 5;")
}

function test4() {
    loadString("a({'foo':20})");
}

function test5() {
    loadString("a(0)");
}

function test6() {
    loadString("a.bar({'foo':20})");
}

function test7() {
    loadString("a.foo[0][0] = 42;");
}

function test8() {
    loadString("a[0][0][0] = 42;");
}

function test9() {
    loadString("c = 42");
}

function test10() {
    loadString("c.f = 42");
}

function test11() {
    loadString("c[0] = 42;");
}

function test12() {
    loadString("c = 'root'; d = 5;")
}

function test13() {
    loadString("c({'foo':20})");
}

function test14() {
    loadString("c(0)");
}

function test15() {
    loadString("c.bar({'foo':20})");
}

function test16() {
    loadString("c.foo[0][0] = 42;");
}

shouldThrow(test1);
shouldThrow(test2);
shouldThrow(test3);
shouldThrow(test4);
shouldThrow(test5);
shouldThrow(test6);
shouldThrow(test7);
shouldThrow(test8);

shouldThrow(test9);
shouldThrow(test10);
shouldThrow(test11);
shouldThrow(test12);
shouldThrow(test13);
shouldThrow(test14);
shouldThrow(test15);
shouldThrow(test16);

let a;
let b;
const c = 0;
const d = 0;