function testMax(x, y) {
    return Math.max(x, y);
}

function testMin(x, y) {
    return Math.min(x, y);
}

for (var i = 0; i < 1000; ++i) {
    x = 100/testMax(0.0, -0.0);
    if (x < 0)
        throw "FAILED";
    x = 100/testMax(-0.0, 0.0);
    if (x < 0)
        throw "FAILED";

    x = 100/testMin(0.0, -0.0);
    if (x > 0)
        throw "FAILED";
    x = 100/testMin(-0.0, 0.0);
    if (x > 0)
        throw "FAILED";
}
