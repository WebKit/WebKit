b = true;
let x = "x";
for (var i = 0; i < 1000; i++) {
    assert(foo() === "x");
    assert(x === "x");
}

x = 20;
x = 40;
for (var i = 0; i < 1000; i++) {
    assert(foo() === 40);
    assert(x === 40);
}

