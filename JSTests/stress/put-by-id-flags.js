function f(x, y) {
    x.y = y;
};

function g(x) {
    return x.y + 42;
}
noInline(f);
noInline(g);

var x = {};
var y = {};
f(x, 42);
f(y, {});

while (!numberOfDFGCompiles(g)) {
    optimizeNextInvocation(g);
    if (typeof g(x) !== 'number')
        throw 'failed warming up';
}

if (typeof g(y) !== 'string')
    throw 'failed after compilation';
