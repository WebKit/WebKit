var array = new Int32Array(1);

function foo(value) {
    array[0] = value;
    return array[0];
}

noInline(foo);

function test(input, output) {
    var result = foo(input);
    if (result != output)
        throw "Error: " + input + " was supposed to result in " + output + " but instead got " + result;
}

for (var i = 0; i < 100000; ++i)
    test(i + 0.5, i);

test(0, 0);
test(100.5, 100);
test(-100.5, -100);
test(3000000000, -1294967296);
test(-3000000000, 1294967296);
test(-2147483648, -2147483648);
