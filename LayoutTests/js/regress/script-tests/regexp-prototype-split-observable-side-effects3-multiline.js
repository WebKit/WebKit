//@ runDefault

function assert(testedValue, msg) {
    if (!testedValue)
        throw Error(msg);
}

// RegExp.prototype with overridden flags: Testing ES6 21.2.5.11: 5. Let flags be ? ToString(? Get(rx, "flags")).
(function () {
    let flag = "multiline";
    let flagValue = false;

    let accesses = [];
    let origDescriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, flag);

    Object.defineProperty(RegExp.prototype, flag, {
        get: function() {
            accesses.push(flag);
            return flagValue;
        }
    });
    let obj = /it/;

    assert(accesses == "", "unexpected call to overridden props");
    let result = RegExp.prototype[Symbol.split].call(obj, "splitme");
    assert(accesses == flag, "Property accesses do not match expectation");
    assert(result == "spl,me", "Unexpected result");
})();
