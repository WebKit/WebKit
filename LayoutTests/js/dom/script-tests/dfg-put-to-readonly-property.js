function foo(a) {
    NaN=0
    return NaN|0;
}

function bar(a) {
    return foo(a) | eval("NaN=0");
}

noInline(bar);

var sum = 0;
var i = 0;
while (i < 10000) {
    sum += bar(i);
    i++;
}

sum += bar(i, true);
