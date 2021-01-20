//@ runFTLNoCJIT("--createPreHeaders=false")

function foo(array, y) {
    var x = 0;
    var j = 0;
    do {
        x = y * 3;
        var result = 0;
        var i = 0;
        if (!array.length)
            array = [1];
        do {
            result += array[i++];
        } while (i < array.length)
        j++;
    } while (j < 3);
    return result + x;
}

noInline(foo);

for (var i = 0; i < 10000; ++i)
    foo([1, 2, 3], 42);
