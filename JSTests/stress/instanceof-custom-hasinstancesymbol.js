function Constructor(x) {}

Object.defineProperty(Constructor, Symbol.hasInstance, {value: function() { return false; }});

x = new Constructor();

function instanceOf(a, b) {
    return a instanceof b;
}
noInline(instanceOf);

function body() {
    var result = 0;
    for (var i = 0; i < 100000; i++) {
        if (instanceOf(x, Constructor))
            result++;
    }

    return result;
}
noInline(body);

if (body())
    throw "result incorrect";
