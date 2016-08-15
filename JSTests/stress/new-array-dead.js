// Tests when we have a dead NewArray that we end up killing and there are other things in the basic block.

function foo(a, b, c) {
    var d = a + b;
    var e = b + c;
    var f = c + a;
    var g = a - b;
    var h = b - c;
    var i = c - a;
    var j = [a + 1, b + 2, c + 3, d + 4, e + 5, f + 6, g + 7, h + 8, i + 9];
    var d = a * b;
    var e = b * c;
    var f = c * a;
    var g = a / b;
    var h = b / c;
    var i = c / a;
    var j = [a + 10, b + 11, c + 12, d + 13, e + 14, f + 15, g + 16, h + 17, i + 18];
    var d = a % b;
    var e = b % c;
    var f = c % a;
    var g = b - a;
    var h = c - b;
    var i = a - c;
    var j = [a + 19, b + 20, c + 21, d + 22, e + 23, f + 24, g + 25, h + 26, i + 27];
    var d = b / a;
    var e = c / b;
    var f = a + c;
    var g = b % a;
    var h = c % b;
    var i = a % c;
    var j = [a + 28, b + 29, c + 30, d + 31, e + 32, f + 33, g + 34, h + 35, i + 36];
    return 42;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1.5, 2.5, 3.5);
    if (result != 42)
        throw "Error: bad result: " + result;
}

