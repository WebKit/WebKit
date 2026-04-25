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
    function classFactory() {
        return class {
            static #method() {
                return 'test';
            }
    
            static access(o) {
                return o.#method();
            }
        }
    };
    
    let C1 = classFactory();
    let C2 = classFactory();
    
    assert.sameValue(C1.access(C1), 'test');
    assert.sameValue(C2.access(C2), 'test');
    
    assert.throws(TypeError, function () {
        C1.access(C2);
    });
    
    assert.throws(TypeError, function () {
        C2.access(C1);
    });
})();

(function () {
    function classFactory() {
        return class {
            static get #m() {
                return 'test';
            }
    
            static access(o) {
                return o.#m;
            }
        }
    };
    
    let C1 = classFactory();
    let C2 = classFactory();
    
    assert.sameValue(C1.access(C1), 'test');
    assert.sameValue(C2.access(C2), 'test');
    
    assert.throws(TypeError, function () {
        C1.access(C2);
    });
    
    assert.throws(TypeError, function () {
        C2.access(C1);
    });
})();

(function () {
    function classFactory() {
        return class {
            static set #m(v) {
                this._v = v;
            }
    
            static access(o) {
                o.#m = 'test';
            }
        }
    };
    
    let C1 = classFactory();
    let C2 = classFactory();
    
    C1.access(C1);
    assert.sameValue(C1._v, 'test');

    C2.access(C2);
    assert.sameValue(C2._v, 'test');
    
    assert.throws(TypeError, function () {
        C1.access(C2);
    });
    
    assert.throws(TypeError, function () {
        C2.access(C1);
    });
})();

