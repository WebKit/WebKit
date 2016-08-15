(function deleteSymbolFromJSSymbolTableObject(globalProxy) {
    var symbolProperty = Symbol("test");

    globalProxy[symbolProperty] = symbolProperty;
    if (globalProxy[symbolProperty] !== symbolProperty)
        throw new Error("bad value: " + String(globalProxy[symbolProperty]));

    delete globalProxy[symbolProperty];
    if (symbolProperty in globalProxy)
        throw new Error("bad value: " + String(globalProxy[symbolProperty]));
})(this);
