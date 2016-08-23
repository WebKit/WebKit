function foo() {
    return a + b;
}

var array = Array(10007);

var string = foo.toString();

for (var i = 0; i < 500000; ++i) {
    array[i % array.length] = "foo " + string.substring(i % string.length, (i / string.length) % string.length) + " bar";
    array[i % array.length] = "this " + array[i % array.length] + " that";
}

for (var i = 0; i < array.length; ++i) {
    var thing = array[i].substring(9, array[i].length - 9);
    if (string.indexOf(thing) < 0)
        throw "Error: bad substring: " + thing;
}
