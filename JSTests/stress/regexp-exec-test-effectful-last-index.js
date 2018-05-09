function assert(b) {
    if (!b)
        throw new Error;
}

let outer = 42;

function foo(r, s) {
    let y = outer;
    r.test(s);
    return y + outer;
}
noInline(foo);

for (let i = 0; i < 10000; ++i) {
    let r = /foo/g;
    regexLastIndex = {};
    regexLastIndex.toString = function() {
        outer = 1;
        return "1";
    };

    r.lastIndex = regexLastIndex;
    let result = foo(r, "bar");
    assert(result === 43);

    outer = 42;
}

function bar(r, s) {
    let y = outer;
    r.exec(s);
    return y + outer;
}
noInline(bar);

for (let i = 0; i < 10000; ++i) {
    let r = /foo/g;
    regexLastIndex = {};
    regexLastIndex.toString = function() {
        outer = 1;
        return "1";
    };

    r.lastIndex = regexLastIndex;
    let result = bar(r, "bar");
    assert(result === 43);

    outer = 42;
}
