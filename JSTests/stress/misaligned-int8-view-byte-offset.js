function make(offset) {
    // Default offset is 1.
    if (offset === void 0)
        offset = 1;
    
    var int8Array = new Int8Array(100);
    for (var i = 0; i < int8Array.length; ++i)
        int8Array[i] = i;

    return new Int8Array(int8Array.buffer, offset, int8Array.length - offset);
}
noInline(make);

function foo(o) {
    return o.byteOffset;
}

noInline(foo);

var o = make();
for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result != 1)
        throw "Error: bad result: " + result;
}
