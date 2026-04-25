let obj = {};

function foo(caller) {
    bar(caller);
}

function bar(caller) {
    baz(caller);
}

function baz(caller) {
    Error.captureStackTrace(obj, caller);
}

[1].forEach(() => { foo(foo); });
// =>, forEach, global
if (obj.stack.split("\n").length !== 3)
    throw new Error(obj.stack);

[1].forEach(() => { foo(Array.prototype.forEach); });
// global
if (obj.stack.split("\n").length !== 1)
    throw new Error(obj.stack);

bar(foo)
if (obj.stack !== "")
    throw new Error(obj.stack);

bar(baz)
// bar, global
if (obj.stack.split("\n").length !== 2)
    throw new Error(obj.stack);

// baz, bar, global
bar(null)
if (obj.stack.split("\n").length !== 3)
    throw new Error(obj.stack);

// baz, bar, foo, global
foo(obj)
if (obj.stack.split("\n").length !== 4)
    throw new Error(obj.stack);