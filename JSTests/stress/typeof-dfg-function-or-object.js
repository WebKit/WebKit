function assert(b) {
    if (!b) {
        throw new Error("Bad")
    }
}

function foo(arg) {
    let o;
    if (arg) {
        o = {};
    } else {
        o = function() { }
    }
    return typeof o;
}
noInline(foo);

for (let i = 0; i < testLoopCount; i++) {
    let bool = !!(i % 2);
    let result = foo(bool);
    if (bool)
        assert(result === "object");
    else
        assert(result === "function");
}
