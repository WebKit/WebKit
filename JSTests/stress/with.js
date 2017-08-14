for (var i = 0; i < 10000; ++i) {
    var x = 1;
    var y = 2;

    var z = {a: 42};
    with (z) {
        x = y;
    }
    if (x !== 2 || y !== 2 || z.a !== 42) {
        throw "Error: bad result, first case, for i = " + i;
    }

    z = {y: 42}
    with (z) {
        x = y;
    }
    if (x !== 42 || y !== 2 || z.y !== 42) {
        throw "Error: bad result, second case, for i = " + i;
    }

    z = {x: 13};
    with (z) {
        x = y;
    }
    if (x !== 42 || y !== 2 || z.x !== 2) {
        throw "Error: bad result, third case, for i = " + i;
    }

    z = {x:13, y:14};
    with (z) {
        x = y;
    }
    if (x !== 42 || y !== 2 || z.x !== 14 || z.y !== 14) {
        throw "Error: bad result, fourth case, for i = " + i;
    }
}
