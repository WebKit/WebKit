function foo(input) {
    return Atomics.load(input, 0);
}
noInline(foo);


var a = new Uint8Array(10);
var b = new Uint32Array(10);
for (let i=0; i<1e4; i++) {
    foo(a);
    foo(b);
}
