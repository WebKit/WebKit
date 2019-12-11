function Constructor(x) {}

Object.defineProperty(Constructor, Symbol.hasInstance, {value: function() { return false; }});

x = new Constructor();

function body() {
    var result = 0;
    for (var i = 0; i < 100000; i++) {
        if (x instanceof Constructor)
            result++;
    }

    return result;
}
noInline(body);

if (body())
    throw "result incorrect";
