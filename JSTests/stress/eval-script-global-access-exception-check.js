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

const o = {};
o.__proto__ = RegExp();
shouldThrow(() => {
    $.evalScript.call(o);
}, `TypeError: The RegExp.prototype.global getter can only be called on a RegExp object`);
