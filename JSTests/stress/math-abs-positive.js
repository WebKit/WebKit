var minus_three_quarters = -0.75;

function foo() {
    return Math.abs(minus_three_quarters);
}

for (var i = 0; i < 10000; i++) {
    var result = foo();
    if (result < 0)
        throw "Error: Math.abs returned a negative value.";
}
