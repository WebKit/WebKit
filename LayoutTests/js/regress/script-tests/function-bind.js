function foo(a,b) {
    return a ^ b + arguments.length;
}

var g0 = foo.bind({});
var g1 = foo.bind({}, 1);
var g2 = foo.bind({}, 1, 2);
var g3 = foo.bind({}, 1, 2, 3);

var result = 0;
for (var i = 0; i < 100000; ++i) {
    result *= 3;
    result += g0(i, result);
    result += g1();
    result += g1(i);
    result += g1(i, result);
    result += g2();
    result += g2(i);
    result += g2(i, result);
    result += g3();
    result += g3(i);
    result += g3(i, result);
    result |= 0;
}
if (result != 1596499010)
    throw "Bad result: " + result;
