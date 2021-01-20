//@ if $buildType == "release" then runDefault else skip end

function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);

let tests = [
    [true, true],
    [false, false],
    ["", false],
    ["" + "" + "", false],
    ["foo", true],
    ["foo" + "bar", true],
    [{}, true],
    [Symbol(), true],
    [undefined, false],
    [null, false],
    [0, false],
    [-0, false],
    [+0, false],
    [NaN, false],
    [10, true],
    [10.2012, true],
    [function() { }, true],
    [new String("foo"), true],
    [new String(""), true],
    [new String, true]
];

function test1(c) {
    return !!c;
}
noInline(test1);

function test2(c) {
    if (c)
        return true;
    return false;
}
noInline(test2);

function test3(c) {
    if (!c)
        return false;
    return true;
}
noInline(test3);

let testFunctions = [test1, test2, test3];

for (let testFunction of testFunctions) {
    for (let i = 0; i < 10000; i++) {
        let item = tests[i % tests.length];
        assert(testFunction(item[0]) === item[1]);
    }
}

let masquerader = makeMasquerader();
for (let testFunction of testFunctions) {
    for (let i = 0; i < 10000; i++) {
        assert(testFunction(masquerader) === false);
    }
}
