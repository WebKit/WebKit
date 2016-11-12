function bar(a, b, c, d, e, f) { }
noInline(bar);
function foo(a, b, ...args) {
    return bar(a, b, ...args);
}
noInline(foo);

let start = Date.now();
for (let i = 0; i < 500000; i++) {
    foo(i, i+1, i+2, i+3, i+4, i+5);
}
const verbose = false;
if (verbose)
    print(Date.now() - start);
