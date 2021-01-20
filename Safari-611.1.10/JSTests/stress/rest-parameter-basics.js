function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);

function foo(a, ...b) {
    return b;    
}
noInline(foo);

function bar(a, ...b) {
    return a + b[0];
}
noInline(bar);

function baz(a, ...b) {
    function capture() { return b; }
    assert(b[0] === capture()[0]);
    return a + b[0];
}
noInline(baz);

function jaz(a, ...b) {
    function capture() { return a + b[0]; }
    assert(capture() === a + b[0]);
    return a + b[0];
}
noInline(jaz);

function kaz(a = 10, ...b) {
    return a + b[0]
}
noInline(kaz);

function raz(a = 10, ...b) {
    function capture() { return a + b[0]; }
    assert(capture() === a + b[0]);
    return a + b[0];
}
noInline(raz);

function restLength(a, ...b) {
    return b.length;
}
noInline(restLength);

function testArgumentsObject(...args) {
    assert(args.length === arguments.length);
    for (let i = 0; i < args.length; i++)
        assert(args[i] === arguments[i]);
}
noInline(testArgumentsObject);

function strictModeLikeArgumentsObject(a, ...args) {
    assert(arguments[0] === a);
    a = "a";
    assert(arguments[0] !== a);
    assert(arguments[0] === 20);
    assert(arguments.length === 2);
    assert(args.length === 1);
    assert(arguments[1] === args[0]);
    arguments[1] = "b";
    assert(args[0] !== "b");
}
noInline(strictModeLikeArgumentsObject);

for (let i = 0; i < 10000; i++) {
    let a1 = foo(10, 20);
    assert(a1 instanceof Array);
    assert(a1.length === 1);
    assert(a1[0] === 20);

    let a2 = foo(10);
    assert(a2 instanceof Array);
    assert(a2.length === 0);

    let a3 = bar(10, 20);
    assert(a3 === 30);

    let a4 = baz(10, 20);
    assert(a4 === 30);

    let a5 = jaz("hello", "world");
    assert(a5 === "helloworld");

    let a6 = kaz(undefined, 40);
    assert(a6 === 50);

    let a7 = kaz(undefined, 40);
    assert(a7 === 50);

    assert(restLength() === 0);
    assert(restLength(1) === 0);
    assert(restLength(1, 1) === 1);
    assert(restLength(1, 1, 1) === 2);
    assert(restLength(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1) === 20);

    let obj = {foo: 40};
    testArgumentsObject("hello", obj, 100, 12.34, "world", obj, [1, 2, 3]);

    strictModeLikeArgumentsObject(20, 30);
}
