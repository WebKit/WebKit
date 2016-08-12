function performString(value) {
    return String(value);
}
noInline(performString);

for (var i = 0; i < 10000; ++i) {
    var result = performString(Symbol.iterator);
    if (result !== 'Symbol(Symbol.iterator)')
        throw new Error('bad value: ' + result);
}
