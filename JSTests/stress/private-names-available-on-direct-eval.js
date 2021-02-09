//@ requireOptions("--usePrivateMethods=true")

let assert = {
    sameValue: function (lhs, rhs) {
        if (lhs !== rhs)
            throw new Error("Expected: " + rhs + " bug got: " + lhs);
    }
};

(function () {
    class C {
        #m() { return 'test'; }

        callMethodFromEval() {
            let self = this;
            return eval('self.#m()');
        }
    }

    let c = new C();
    assert.sameValue(c.callMethodFromEval(), 'test');
})();

