function foo(a) {
    switch (a) {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return 2;
    default:
        return 3;
    }
}

function bar(a, runFoo) {
    if (runFoo)
        return foo(a);
    return 0;
}

noInline(bar);

var sum = 0;

for (var i = 0; i < 5; i++)
    sum += bar(i, true);

var i = 0;
while (!dfgCompiled({f:bar})) {
    sum += bar(i, false);
    i++;
}

sum += bar(i, true);
