function testTypeError(script, message) {
    var error = null;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Expected type error not thrown by `" + script + "`");

    if (String(error) !== message)
        throw new Error("Bad error: " + String(error));
}

function testOK(script) {
    var error = null;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }
    if (error)
        throw new Error("Bad error: " + String(error));
}

testTypeError(`({ } = null)`, "TypeError: Right side of assignment cannot be destructured");
testTypeError(`({ a } = null)`, "TypeError: Right side of assignment cannot be destructured");
testTypeError(`({ a: { b } = null } = { })`, "TypeError: Right side of assignment cannot be destructured");
testTypeError(`({ a: { b } } = { a: null })`, "TypeError: Right side of assignment cannot be destructured");
testTypeError(`({ } = undefined)`, "TypeError: Right side of assignment cannot be destructured");
testTypeError(`({ a } = undefined)`, "TypeError: Right side of assignment cannot be destructured");
testTypeError(`({ a: { b } = undefined } = { })`, "TypeError: Right side of assignment cannot be destructured");
testTypeError(`({ a: { b } } = { a: undefined })`, "TypeError: Right side of assignment cannot be destructured");

testOK(`({ } = 123)`);
testOK(`({ a } = 123)`);
testOK(`({ a: { b } = 123 } = { })`);
testOK(`({ a: { b } } = { a: 123 })`);

testOK(`({ } = 0.5)`);
testOK(`({ a } = 0.5)`);
testOK(`({ a: { b } = 0.5 } = { })`);
testOK(`({ a: { b } } = { a: 0.5 })`);

testOK(`({ } = NaN)`);
testOK(`({ a } = NaN)`);
testOK(`({ a: { b } = NaN } = { })`);
testOK(`({ a: { b } } = { a: NaN })`);

testOK(`({ } = true)`);
testOK(`({ a } = true)`);
testOK(`({ a: { b } = true } = { })`);
testOK(`({ a: { b } } = { a: true })`);

testOK(`({ } = {})`);
testOK(`({ a } = {})`);
testOK(`({ a: { b } = {} } = { })`);
testOK(`({ a: { b } } = { a: {} })`);

testOK(`({ } = [])`);
testOK(`({ a } = [])`);
testOK(`({ a: { b } = [] } = { })`);
testOK(`({ a: { b } } = { a: [] })`);

testOK(`({ } = /1/)`);
testOK(`({ a } = /1/)`);
testOK(`({ a: { b } = /1/ } = { })`);
testOK(`({ a: { b } } = { a: /1/ })`);
