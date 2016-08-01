function makeBaseArguments() {
    return arguments;
}

noInline(makeBaseArguments);

function makeArray(length) {
    var array = new Array(length);
    for (var i = 0; i < length; ++i)
        array[i] = 99999;
    return array;
}

noInline(makeArray);

function cons(f) {
    var result = makeBaseArguments();
    result.f = f;
    return result;
}

var array = [];
for (var i = 0; i < 100000; ++i)
    array.push(cons(i));

for (var i = 0; i < 1000000; ++i) {
    var j = (i * 3) % array.length;
    array[j] = cons(j);
    
    makeArray(i % 7);
}

for (var i = 0; i < array.length; ++i) {
    if (array[i].f != i)
        throw "Error: bad value of f at " + i + ": " + array[i].f;
}
