function foo(x){
    return x.byteOffset
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var b = new Uint8Array(42, 0);
    if (foo(b) != 0) 
        throw "error"
}

