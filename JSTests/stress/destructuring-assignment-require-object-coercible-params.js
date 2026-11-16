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

testTypeError(`function fn({ }){} fn(null)`, "TypeError: Cannot destructure null or undefined value");
testTypeError(`function fn({ a }){} fn(null)`, "TypeError: Cannot destructure property 'a' from null or undefined value");
testTypeError(`function fn({ a: { b } = null }){} fn({ })`, "TypeError: Cannot destructure property 'b' from null or undefined value");
testTypeError(`function fn({ a: { b } }){} fn({ a: null })`, "TypeError: Cannot destructure property 'b' from null or undefined value");
testTypeError(`function fn({ ...a }){} fn(null)`, "TypeError: Cannot destructure null or undefined value");
testTypeError(`function fn({ }){} fn(undefined)`, "TypeError: Cannot destructure null or undefined value");
testTypeError(`function fn({ a }){} fn(undefined)`, "TypeError: Cannot destructure property 'a' from null or undefined value");
testTypeError(`function fn({ a: { b } = undefined }){} fn({ })`, "TypeError: Cannot destructure property 'b' from null or undefined value");
testTypeError(`function fn({ a: { b } }){} fn({ a: undefined })`, "TypeError: Cannot destructure property 'b' from null or undefined value");
testTypeError(`function fn({ ...a }){} fn(undefined)`, "TypeError: Cannot destructure null or undefined value");

