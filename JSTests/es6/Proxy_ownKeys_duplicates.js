function test() {

var symbol = Symbol("test");
var proxy = new Proxy({}, {
    ownKeys: function (t) {
        return ["A", "A", "0", "0", symbol, symbol];
    }
});
var keys = Object.keys(proxy);
var names = Object.getOwnPropertyNames(proxy);
var symbols = Object.getOwnPropertySymbols(proxy);

if (keys.length === 4 && keys[0] === keys[1] && keys[2] === keys[3] &&
    keys[0] === "A" && keys[2] === "0" &&
    names.length === 4 && names[0] === names[1] && names[2] === names[3] &&
    names[0] === "A" && names[2] === "0" &&
    symbols.length === 2 && symbols[0] === symbols[1] && symbols[0] === symbol)
    return true;
return false;

}

if (!test())
    throw new Error("Test failed");

