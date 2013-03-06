description(
"Tests that the DFG doesn't get confused about an edge being a double edge after we perform CFG simplification."
);

function foo(a, p) {
    var p2;
    var x;
    var y;
    if (p)
        p2 = true;
    else
        p2 = true;
    if (p2)
        x = a;
    else
        x = 0.5;
    if (p2)
        y = a;
    else
        y = 0.7;
    var result = x + y;
    return [result, [x, y], [x, y], [x, y]];
}

for (var i = 0; i < 1000; ++i)
    shouldBe("foo(42, true)[0]", "84");
