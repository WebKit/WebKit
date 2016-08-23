var x = 1;
var o = {
    get value() { 
        x ^= x * 3;
        x = x | 1;
        return x;
    }
}

function foo(o) {
    var result = 0;
    for (var i = 0; i < 128; i++) {
        result ^= o.value;
        result |= 1
    }
    return result;
}

noInline(foo);
var result = 0;
for (var i = 0; i < 40000; ++i) {
    result ^= foo(o);
    result = result | 1;
}
if (result != -2004318071)
    throw "Incorrect result: " + result + ". Should be -2004318071";
