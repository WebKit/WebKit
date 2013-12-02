function foo() {
    var x = 0;
    ++x;
    for (var x in [1, 2, 3]) {
    }
    x = 1;
    x++;
    var y = x++;
    var z = y++;
    z = x++;
    x += 2;
    z -= 1;
    x *= 2;
    z <<= 1;
    x |= 5;
    return function() { return x++ + z; }
}

for (var i = 0; i < 100; ++i) {
    if (foo()() != 17)
        throw "Error";
}
