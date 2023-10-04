//@ runBytecodeCacheNoAssertion

function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}


try {
    $262.evalScript(`try {} catch ({foo2}) { function foo2() {} }`);
    throw new Error("evalScript() didn't throw()!");
} catch (err) {
    assert(err.toString() === "SyntaxError: Cannot declare a function that shadows a let/const/class/function variable 'foo2'.");
}
assert(!globalThis.hasOwnProperty("foo2"));


let foo9 = 9;
$262.evalScript(`{ function foo9() {} }`);
assert(foo9 === 9);
assert(!globalThis.hasOwnProperty("foo9"));


try {
    $262.evalScript(`try {} catch (foo19) { function foo19() {} }`);
    throw new Error("evalScript() didn't throw()!");
} catch (err) {
    assert(err.toString() === "SyntaxError: Cannot declare a function that shadows a let/const/class/function variable 'foo19'.");
}
assert(!globalThis.hasOwnProperty("foo19"));


try {
    $262.evalScript(`"use strict"; { function foo20() {} function foo20() {} }`);
    throw new Error("evalScript() didn't throw()!");
} catch (err) {
    assert(err.toString() === "SyntaxError: Cannot declare a function that shadows a let/const/class/function variable 'foo20'.");
}
assert(!globalThis.hasOwnProperty("foo19"));
