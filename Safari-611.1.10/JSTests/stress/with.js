function foo (x, y, z, newX, checkZ, errorMessage) {
    with(z) {
        x = y;
    }
    if (x !== newX || !checkZ(z)) {
        throw errorMessage;
    }
}

for (var i = 0; i < 10000; ++i) {
    foo(1, 2, {a:42}, 2, z => z.a === 42, "Error: bad result for non-overlapping case, i = " + i);
    foo(1, 2, {x:42}, 1, z => z.x === 2, "Error: bad result for setter case, i = " + i);
    foo(1, 2, {y:42}, 42, z => z.y === 42, "Error: bad result for getter case, i = " + i);
    foo(1, 2, {x:42, y:13}, 1, z => z.x === 13 && z.y === 13, "Error: bad result for setter/getter case, i = " + i);
    foo(1, 2, "toto", 2, z => z === "toto", "Error: bad result for string case, i = " + i);
    try {
        foo(1, 2, null, 2, z =>
                {throw "Error: missing type error, i = " + i}, "Unreachable");
    } catch (e) {
        if (!(e instanceof TypeError)) {
            throw e;
        }
    }
}
