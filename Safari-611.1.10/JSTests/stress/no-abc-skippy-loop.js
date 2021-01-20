function foo(array) {
    var result = 0;
    for (var i = 0; i != array.length; i += 2) {
        var element = array[i];
        if (element === void 0)
            break;
        result += array[i];
    }
    return result;
}

noInline(foo);

var array = [1, 2, 3, 4];
for (var i = 0; i < 10000; ++i) {
    var result = foo(array);
    if (result != 4)
        throw "Error: bad result in loop: " + result;
}

var array = [1, 2, 3];
var result = foo(array);
if (result != 4)
    throw "Error: bad result at end: " + result;
