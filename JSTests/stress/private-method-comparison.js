let assert = {
    sameValue: function (lhs, rhs) {
        if (lhs !== rhs)
            throw new Error("Expected: " + lhs + " bug got: " + rhs);
    },

    throws: function (expectedError, op) {
        try {
          op();
        } catch(e) {
            if (!(e instanceof expectedError))
                throw new Error("Expected to throw: " + expectedError + " but threw: " + e);
        }
    }
};

class C {
    #m() { return 'test262'; }

    getPrivateMethod() {
        return this.#m;
    }
}

let c1 = new C();
let c2 = new C();

assert.sameValue(c1.getPrivateMethod(), c2.getPrivateMethod());

