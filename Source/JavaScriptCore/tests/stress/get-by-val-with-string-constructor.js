var symbol = "@@species";
function Hello() {
}

Object.defineProperty(Hello, symbol, {
    get: function () {
        return this;
    }
});

Hello.prototype.generate = function () {
    return new this.constructor[symbol]();
};

function ok() {
    var object = new Hello();
    if (!(object.generate() instanceof Hello))
        throw new Error("bad instance");
}
noInline(ok);

for (var i = 0; i < 10000; ++i)
    ok();
