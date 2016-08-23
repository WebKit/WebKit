function foo(o) {
    switch (o.f) {
    case 1: return 5;
    case 2: return 2;
    case 3: return 7;
    case 4: return 9;
    case 5: return o.f + 1;
    case 6: return 0;
    case 7: return 89;
    case 8: return 23;
    case 9: return 12;
    case 10: return 54;
    case 11: return 53;
    default: return 42;
    }
}

function bar() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += foo({f:5});
    return result;
}

var result = bar();
if (result != 6000000)
    throw "Error: bad result: " + result;
