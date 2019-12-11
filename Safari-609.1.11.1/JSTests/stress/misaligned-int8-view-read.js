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

function foo(o, i) {
    return o[i];
}

noInline(foo);

var o = make();
for (var i = 0; i < 10000; ++i) {
    var index = i % o.length;
    var result = foo(o, index);
    if (result != index + 1)
        throw "Read test error: bad result for index = " + index + ": " + result + "; expected " + (index + 1);
}
