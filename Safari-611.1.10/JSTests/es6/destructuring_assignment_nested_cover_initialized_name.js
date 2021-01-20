function test1() {
    var c = "it didn't work";
    ({ a: { b: { c = "it worked" } } } = { a: { b: {} } });
    return c === "it worked";
}

function test2() {
    var c = "it didn't work";
    [{ a: { b: { c = "it worked" } } }] = [{ a: { b: {} } }];
    return c === "it worked";
}

if (!test1())
    throw new Error("Test1 failed");

if (!test2())
    throw new Error("Test2 failed");