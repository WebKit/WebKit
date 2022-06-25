var constant = {};

function foo(o) {
    var v = o.f;
    return v === constant;
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo({f:null});
    if (result !== false)
        throw "Error: bogus result in loop";
}

var result = foo({f:constant});
if (result !== true)
    throw "Error: bogus result at end";
