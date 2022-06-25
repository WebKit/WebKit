var False = false;

function foo(p, array) {
    var result = 0;
    var i = 0;
    if (array.length) {
        if (p) {
        } else {
            return;
        }
        do {
            result += array[i++];
        } while (False);
    }
    return result;
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo(true, [42]);
    if (result != 42)
        throw "Error: bad result: " + result;
}
