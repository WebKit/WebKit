var obj = {
    a: {
        b: {
            c: {
                get dGetter() { throw new Error("foo"); },
                dFunction() { throw new Error("foo"); },
            },
        },
    },
};

try {
obj
.a
.b
.c
    .dGetter // !! error
;
} catch (err) {
    assert(err.stack.endsWith(":17:5"));
}

try {
obj
.a
.b
.c
.dFunction
    ` // !! error
    `
;
} catch (err) {
    assert(err.stack.endsWith(":29:5"));
}

function A() {
    return function B() {
        return function C() {
            return function D() {
                throw new Error("foo");
            }
        }
    }
}

try {
new
    new // !! error
new // D
new // C
new // B
A
;
} catch (err) {
    assert(err.stack.endsWith(":48:5"));
}

try {
obj
["a"]
["b"]
["c"]
    ["dGetter"] // !! error
;
} catch (err) {
    assert(err.stack.endsWith(":63:5"));
}

function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}
