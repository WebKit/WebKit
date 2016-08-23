function foo() {
    return a + b;
}

var array = Array(10007);

var string = foo.toString();

for (var i = 0; i < 700000; ++i) {
    array[i % array.length] = "foo " + string.substring(i % string.length, (i / string.length) % string.length) + " bar";
}

for (var i = 0; i < array.length; ++i) {
    var thing = array[i].substring(4, array[i].length - 4);
    if (string.indexOf(thing) < 0)
        throw "Error: bad substring: \"" + thing + "\"";
}
