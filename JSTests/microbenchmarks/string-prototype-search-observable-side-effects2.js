//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ runNoFTL

function assert(testedValue, msg) {
    if (!testedValue)
        throw Error(msg);
}

// String prototype with overridden @@search.
(function () {
    let accesses = [];
    var obj = String("");
    Object.defineProperty(String.prototype, Symbol.search, {
        value: function (str) {
            accesses.push("Symbol(Symbol.search)");
            return /rch/[Symbol.search](str);
        },
        writable: true,
        configurable: true,
    });

    assert(accesses == "", "unexpected call to overridden props");
    let result = "searchme".search(obj);
    assert(accesses == "Symbol(Symbol.search)", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");

    Object.defineProperty(String.prototype, Symbol.search, { value: undefined, writable: false, configurable: true });
})();
