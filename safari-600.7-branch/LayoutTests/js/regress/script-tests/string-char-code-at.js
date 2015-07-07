function foo(s) {
    var result = 0;
    for (var i = 0; i < s.length; ++i)
        result += s.charCodeAt(i);
    return result;
}

for (var i = 0; i < 1000000; ++i) {
    var result = foo("hello");
    if (result != 532)
        throw "Error: bad result: " + result;
}
