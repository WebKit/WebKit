function blah(args) {
    var array = [];
    for (var i = 0; i < args.length; ++i)
        array.push(args[i]);
    return array;
}

function foo() {
    // Force creation of arguments by doing out-of-bounds access.
    var tmp = arguments[42];
    
    // Use the created arguments object.
    return blah(arguments);
}

function bar(array) {
    return foo.apply(this, array);
}

noInline(blah);
noInline(bar);

function checkEqual(a, b) {
    if (a.length != b.length)
        throw "Error: length mismatch: " + a + " versus " + b;
    for (var i = a.length; i--;) {
        if (a[i] != b[i])
            throw "Error: mismatch at i = " + i + ": " + a + " versus " + b;
    }
}

function test(array) {
    var actual = bar(array);
    checkEqual(actual, array);
}

for (var i = 0; i < 10000; ++i) {
    var array = [];
    for (var j = 0; j < i % 6; ++j)
        array.push(j);
    test(array);
}
