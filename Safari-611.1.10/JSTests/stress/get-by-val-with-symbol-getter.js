
var object = {
    get hello() {
        return 42;
    }
};

var symbol = Symbol();

Object.defineProperty(object, symbol, {
    get: function () {
        return 42;
    }
});

function ok() {
    if (object[symbol] + 20 !== 62)
        throw new Error();
}
noInline(ok);

for (var i = 0; i < 10000; ++i)
    ok();
