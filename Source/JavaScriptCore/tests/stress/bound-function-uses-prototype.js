// Test ES6 feature of using the bindee's prototype when binding a function.

bind = Function.prototype.bind;

function testChangeProto() {
    function foo() { }

    foo.__proto__ = Object.prototype;

    let bar = bind.call(foo);
    if (bar.__proto__ !== foo.__proto__)
        throw "incorrect prototype";

    foo.__proto__ = null;
    bar = bind.call(foo);
    if (bar.__proto__ !== foo.__proto__)
        throw "cached prototype incorrectly"
}
testChangeProto();

function testBuiltins() {
    let bar = bind.call(Array);
    if (bar.__proto__ !== Array.__proto__)
        throw "builtin prototype incorrect";

    Array.__proto__ = null;
    bar = bind.call(Array);
    if (bar.__proto__ !== Array.__proto__)
        throw "builtin prototype did not change correctly.";
}
testBuiltins();
