function foo(a, b) {
    return a.f ^ b.f;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:5.5}, {f:6.5});
    if (result != 3)
        throw "Error: bad result: " + result;
}

var result = foo({f:"5.5"}, {f:6.5});
if (result != 3)
    throw "Error: bad result: " + result;

var result = foo({f:5.5}, {f:"6.5"});
if (result != 3)
    throw "Error: bad result: " + result;

