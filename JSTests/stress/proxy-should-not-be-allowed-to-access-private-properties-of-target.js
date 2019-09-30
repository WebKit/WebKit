var foo = (function* bar() {
    try {
        yield* x;
    } finally {
        try {
            y;
        } finally {
            return;
        }
    }
}) ();

var x = new Proxy(foo, {});
try {
    x.next();
} catch (e) {
    exception = e;
}

if (exception != 'TypeError: |this| should be a generator')
    throw "FAILED";
