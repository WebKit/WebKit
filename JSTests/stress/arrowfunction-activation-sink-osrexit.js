var n = 100000;

function bar() { }

function foo(b) {
    var result = 0;
    var set = (x) => { result = x; }
    if (b) {
        OSRExit();
        if (b) {
            bar(set);
        }
        return 0;
    }
    return result;
}

noInline(bar);
noInline(foo);

for (var i = 0; i < n; i++) {
    var result = foo(!(i % 100));
    if (result != 0)
        throw "Error: bad result: " + result;
}
