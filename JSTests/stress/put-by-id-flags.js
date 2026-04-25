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

for (let i = 0; i < 1000; ++i) {
    optimizeNextInvocation(g);
    if (typeof g(x) !== 'number')
        throw 'failed warming up';
    if (numberOfDFGCompiles(g))
        break;
}

if (typeof g(y) !== 'string')
    throw 'failed after compilation';
