// Tests that toInt32 conversion on a boolean does not trigger us to take
// an unconditional OSR exit.

function foo(a,b) {
    return (a<b) | 0;
}

var result = 0;
for (var i = 0; i < 1000000; ++i) {
    result *= 2;
    result += foo(i, i + ((i%2) * 2 - 1));
    result |= 0;
}

if (result != 1431655765) {
    print("Bad result: " + result);
    throw "Error";
}


