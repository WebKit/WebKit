function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var symbol = Symbol("Cocoa");

shouldThrow(() => {
    // ToString => error.
    "" + symbol;
}, `TypeError: Cannot convert a symbol to a string`);

shouldThrow(() => {
    // ToNumber => error.
    +symbol;
}, `TypeError: Cannot convert a symbol to a number`);

shouldThrow(() => {
    Symbol.keyFor("Cappuccino");
}, `TypeError: Symbol.keyFor requires that the first argument be a symbol`);

shouldThrow(() => {
    Symbol.prototype.toString.call(null);
}, `TypeError: Symbol.prototype.toString requires that |this| be a symbol or a symbol object`);

shouldThrow(() => {
    Symbol.prototype.toString.call({});
}, `TypeError: Symbol.prototype.toString requires that |this| be a symbol or a symbol object`);

shouldThrow(() => {
    Symbol.prototype.valueOf.call(null);
}, `TypeError: Symbol.prototype.valueOf requires that |this| be a symbol or a symbol object`);

shouldThrow(() => {
    Symbol.prototype.valueOf.call({});
}, `TypeError: Symbol.prototype.valueOf requires that |this| be a symbol or a symbol object`);
