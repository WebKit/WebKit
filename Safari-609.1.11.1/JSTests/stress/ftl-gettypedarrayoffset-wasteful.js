// https://bugs.webkit.org/show_bug.cgi?id=198754
//@ skip if $architecture == "arm" and $hostOS == "linux"
function foo(x){
    return x.byteOffset
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var b = new Uint8Array(new ArrayBuffer(42), 0);
    if (foo(b) != 0) 
        throw new Error();
    b = new Uint8Array(new ArrayBuffer(42), 5);
    if (foo(b) !== 5)
        throw new Error();
    b = new Int32Array(new ArrayBuffer(100000 * 4), i * 4);
    if (foo(b) !== i * 4)
        throw new Error();
}

