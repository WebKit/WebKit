function test(b, f) {
    if (b)
        return f(b);
}
noInline(test);

function throwError(b) {
    if (b) {
        try {
            throw new Error;
        } catch(e) { }
    }
    return 2;
}
noInline(throwError);

function makeFoo() {
    return function foo(b) {
        throwError(b);
        OSRExit();
    }
}

let foos = [makeFoo(), makeFoo()];
for (let i = 0; i < 10000; ++i) {
    test(!!(i%2), foos[((Math.random() * 100) | 0) % foos.length]);
}
