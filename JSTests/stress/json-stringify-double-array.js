function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function doubles(count) {
    let result = [];
    for (let i = 0; i < count; ++i)
        result.push(i / 7);
    return result;
}

for (let i = 0; i < 100; ++i) {
    let array = [1.5, -0, 2.25, Infinity, -Infinity, 1e21, 1e-7, 5e-324, 0.1];
    shouldBe(JSON.stringify(array), "[1.5,0,2.25,null,null,1e+21,1e-7,5e-324,0.1]");
    shouldBe(JSON.stringify(array, null, 1), "[\n 1.5,\n 0,\n 2.25,\n null,\n null,\n 1e+21,\n 1e-7,\n 5e-324,\n 0.1\n]");
    shouldBe(JSON.stringify({ x: [0.5, 1.5] }), '{"x":[0.5,1.5]}');
    shouldBe(JSON.stringify([[0.5], [1.5, 2.5]], null, "\t"), "[\n\t[\n\t\t0.5\n\t],\n\t[\n\t\t1.5,\n\t\t2.5\n\t]\n]");

    let holes = [1.5, 2.5];
    holes[4] = 3.5;
    shouldBe(JSON.stringify(holes), "[1.5,2.5,null,null,3.5]");

    let nan = [1.5, 2.5];
    nan.push(NaN);
    shouldBe(JSON.stringify(nan), "[1.5,2.5,null]");

    let emptied = [1.5];
    emptied.length = 0;
    shouldBe(JSON.stringify(emptied), "[]");

    shouldBe(JSON.stringify([1.5, "\u3042", 2.5]), '[1.5,"\u3042",2.5]');
    shouldBe(JSON.stringify(["\u3042", [1.5, 2.5]]), '["\u3042",[1.5,2.5]]');
}

let large = doubles(20000);
let expected = "[" + large.map(String).join(",") + "]";
shouldBe(JSON.stringify(large), expected);
shouldBe(JSON.stringify([large, large]), "[" + expected + "," + expected + "]");
