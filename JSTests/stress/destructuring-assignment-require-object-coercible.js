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

testTypeError(`({ } = null)`, "TypeError: Cannot destructure null or undefined value");
testTypeError(`({ a } = null)`, "TypeError: Cannot destructure property 'a' from null or undefined value");
testTypeError(`({ a: { b } = null } = { })`, "TypeError: Cannot destructure property 'b' from null or undefined value");
testTypeError(`({ a: { b } } = { a: null })`, "TypeError: Cannot destructure property 'b' from null or undefined value");
testTypeError(`({ ...a } = null)`, "TypeError: Cannot destructure null or undefined value");
testTypeError(`({ } = undefined)`, "TypeError: Cannot destructure null or undefined value");
testTypeError(`({ a } = undefined)`, "TypeError: Cannot destructure property 'a' from null or undefined value");
testTypeError(`({ a: { b } = undefined } = { })`, "TypeError: Cannot destructure property 'b' from null or undefined value");
testTypeError(`({ a: { b } } = { a: undefined })`, "TypeError: Cannot destructure property 'b' from null or undefined value");
testTypeError(`({ ...a } = undefined)`, "TypeError: Cannot destructure null or undefined value");

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

testOK(`({ } = makeMasquerader())`);
testOK(`({ a } = makeMasquerader())`);
testOK(`({ a: { b } = makeMasquerader() } = { })`);
testOK(`({ a: { b } } = { a: makeMasquerader() })`);
