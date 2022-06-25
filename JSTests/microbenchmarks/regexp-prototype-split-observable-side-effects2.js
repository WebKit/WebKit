//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ runNoFTL

function assert(testedValue, msg) {
    if (!testedValue)
        throw Error(msg);
}

// RegExp with overridden [@@species]: Testing ES6 21.2.5.11: 4. Let C be ? SpeciesConstructor(rx, %RegExp%).
(function () {
    let accesses = [];
    let origDescriptor = Object.getOwnPropertyDescriptor(RegExp, Symbol.species);
    Object.defineProperty(RegExp, Symbol.species, {
        value: function() {
            accesses.push(Symbol.species.toString());
            return /it/y;
        }
    });
    let obj = new RegExp;
    let errorStr;

    assert(accesses == "", "unexpected call to overridden props");
    let result = RegExp.prototype[Symbol.split].call(obj, "splitme");
    assert(accesses == "Symbol(Symbol.species)", "Property accesses do not match expectation");
    assert(result == "spl,me", "Unexpected result");
})();
