let assert = {
    sameValue: function (lhs, rhs) {
        if (lhs !== rhs)
            throw new Error("Expected: " + lhs + " bug got: " + rhs);
    }
};

class C {
    #m() { return this._v; }

    getPrivateMethod() {
        return this.#m;
    }
}

let c = new C();

let o1 = {_v: 'test262'};
let o2 = {_v: 'foo'};
assert.sameValue(c.getPrivateMethod().call(o1), 'test262');
assert.sameValue(c.getPrivateMethod().call(o2), 'foo');

