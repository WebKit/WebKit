disableRichSourceInfo();


function shouldThrow(func, variableName) {
    const expectedError = `ReferenceError: Cannot access '${variableName}' before initialization.`;
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

shouldThrow(test1, 'a');
shouldThrow(test2, 'a');
shouldThrow(test3, 'a');
shouldThrow(test4, 'a');
shouldThrow(test5, 'a');
shouldThrow(test6, 'a');
shouldThrow(test7, 'a');
shouldThrow(test8, 'a');

shouldThrow(test9, 'c');
shouldThrow(test10, 'c');
shouldThrow(test11, 'c');
shouldThrow(test12, 'c');
shouldThrow(test13, 'c');
shouldThrow(test14, 'c');
shouldThrow(test15, 'c');
shouldThrow(test16, 'c');

let a;
let b;
const c = 0;
const d = 0;
