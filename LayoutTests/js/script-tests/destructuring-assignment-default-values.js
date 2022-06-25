description("Test default values in destructuring patterns: https://people.mozilla.org/~jorendorff/es6-draft.html#sec-destructuring-binding-patterns");
function assert(a, b) {
    if (b === a)
        testPassed(a + "," + b);
    else
        testFailed(a + "," + b);
}

function test1() {
    var {x = 40} = {};
    var {prop: y = 40} = {};
    var {prop: {z} = {z: 100}} = {};
    assert(x, 40);
    assert(y, 40);
    assert(z, 100);
}
test1();

function arr() { return [undefined, 20, undefined]; }

function test2() {
    var [x = "Hello", y = 40, z = 30] = [undefined, 20, undefined];
    assert(x, "Hello");
    assert(y, 20);
    assert(z, 30);
    var [x = "Hello", y = 40, z = 30] = arr();
    assert(x, "Hello");
    assert(y, 20);
    assert(z, 30);
}
test2();


function test3() {
    var z = "z";
    var [x = z] = [];
    assert(x, "z");
    z = "zz";
    var [x = eval("z")] = [];
    assert(x, "zz");

    var [{a} = {a: 40}] = [undefined];
    assert(a, 40);
    var [{a = 50}] = [{a: undefined}];
    assert(a, 50);
}
test3();


function test4() {
    var [{prop: {b = 40}}] = [{prop: {b: undefined}}];
    var [{prop: [c = 50]}] = [{prop: []}];
    var [{prop: [d = 60]} = {prop: [100]}] = [];
    assert(b, 40);
    assert(c, 50);
    assert(d, 100)
}
test4();


function test5() {
    var {x = undefined} = {x: null};
    assert(x, null);
    var {x = undefined} = {x: false};
    assert(x, false);
    var {x = undefined} = {x: 0};
    assert(x, 0);
}
test5();


function test6() {
    var [x = 50, y = x + 1] = [];
    assert(x, 50);
    assert(y, 51);

    var [x = y, y = x + 1] = [];
    assert(x, 51);
    assert(y, 52);

    // FIXME: make tests for TDZ failures when we land block scoping 'let' and 'const'.
    var [a = b, b = a] = [];
    assert(a, undefined);
    assert(b, undefined);
}
test6();


function test7(a, b) {
    var {c = a, d = c + b} = {};
    assert(c, 10);
    assert(d, 30);
}
test7(10, 20);


function test8(x) {
    // How much uglier can we make a factorial function?
    if (x <= 0) 
        return {p: 1};

    var {p = {p: x * test8(x - 1).p}} = {};
    return p;
}
assert(test8(5).p, 120);
assert(test8(10).p, 3628800);
assert(test8(0).p, 1);
