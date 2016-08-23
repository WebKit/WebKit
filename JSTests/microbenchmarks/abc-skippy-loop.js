function foo(array) {
    var result = 0;
    for (var i = 0; i < array.length; i += 2) {
        var element = array[i];
        if (element === void 0)
            break;
        result += array[i];
    }
    return result;
}

noInline(foo);

var array = [];
for (var i = 0; i < 1000; ++i)
    array.push(i);
for (var i = 0; i < 50000; ++i)
    foo(array);
