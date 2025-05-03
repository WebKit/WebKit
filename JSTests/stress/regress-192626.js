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

if (exception != "TypeError: String.prototype.toString requires that |this| be a string")
    throw "FAIL";
