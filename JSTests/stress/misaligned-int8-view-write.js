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

function foo(o, i, v) {
    o[i] = v;
}

noInline(foo);

var o = make();
var real = new Int8Array(o.buffer);
for (var i = 0; i < 10000; ++i) {
    var index = i % o.length;
    var value = i % 7;
    foo(o, index, value);
    var result = real[index + 1];
    if (result != value)
        throw "Write test error: bad result for index = " + index + ": " + result + "; expected " + value;
}

