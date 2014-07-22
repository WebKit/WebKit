function foo() {
    return a + b;
}

var array = Array(10007);

var string = foo.toString();

for (var i = 0; i < 1000000; ++i) {
    array[i % array.length] = string.substring(i % string.length, (i / string.length) % string.length);
}

for (var i = 0; i < array.length; ++i) {
    if (string.indexOf(array[i]) < 0)
        throw "Error: bad substring: " + array[i];
}
