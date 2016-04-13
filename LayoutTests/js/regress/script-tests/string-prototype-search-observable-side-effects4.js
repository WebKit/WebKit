//@ runDefault

function assert(testedValue, msg) {
    if (!testedValue)
        throw Error(msg);
}

// RegExp.prototype with overridden exec.
(function () {
    let accesses = [];
    let origExec = RegExp.prototype.exec;

    let obj = /rch/;
    Object.defineProperty(RegExp.prototype, "exec", {
        value: function(str) {
            accesses.push("exec");
            return origExec.call(this, str);
        }
    });

    assert(accesses == "", "unexpected call to overridden props");
    let result = "searchme".search(obj);
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();
