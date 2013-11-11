var array;

function bar(x) {
    array.push(x);
}

function foo(n) {
    var x = n + 1000;
    bar(x);
    x += 1000;
    bar(x);
    x += 1000;
    bar(x);
}

noInline(bar);
noInline(foo);

for (var i = 0; i < 10000; ++i) {
    array = [];
    foo(2147483647);
    if ("" + array != "2147484647,2147485647,2147486647")
        throw "Error: bad array: " + array;
}
