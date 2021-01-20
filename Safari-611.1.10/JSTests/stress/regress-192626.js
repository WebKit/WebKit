var a = {};

function foo() {
    return Array.prototype.splice.apply([], a);
}
noInline(foo);

function bar(b) {
    with({});
    a = arguments;
    a.__defineGetter__("length", String.prototype.valueOf);
    foo();
}

var exception;
try {
    bar();
} catch (e) {
    exception = e;
}

if (exception != "TypeError: Type error")
    throw "FAIL";
