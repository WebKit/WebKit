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

(function () {
    class C {
        set #m(v) { this._v = v; }

        accessPrivateMember() {
            return this.#m;
        }
    }

    let c = new C();
    assert.throws(TypeError, function () {
        c.accessPrivateMember();
    });
})();

(function () {
    class C {
        get #m() { return 'test'; }

        accessPrivateMember(v) {
            this.#m = v;
        }
    }

    let c = new C();
    assert.throws(TypeError, function () {
        c.accessPrivateMember('test');
    });
})();

(function () {
    class C {
        #m() { return 'test'; }

        accessPrivateMember(v) {
            this.#m = v;
        }
    }

    let c = new C();
    assert.throws(TypeError, function () {
        c.accessPrivateMember('test');
    });
})();

