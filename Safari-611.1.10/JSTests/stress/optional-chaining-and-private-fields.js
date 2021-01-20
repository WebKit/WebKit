//@ requireOptions("--usePrivateClassFields=true")

let assert = {
    sameValue: (a, e) => {
        if (a !== e) 
            throw new Error("Expected: " + e + " but got: " + a);
    },

    throws: (expectedException, functor, m) => {
        try {
            functor();
        } catch(e) {
            if (!(e instanceof expectedException))
                throw new Error(m);
        }
    }
};

(() => {
    class C {
        #field = 'test';
    
        static access(o) {
            return o?.#field;
        }
    }

    let c = new C;
    for (let i = 0; i < 100000; i++) {
        assert.sameValue(C.access(c), 'test');
        assert.sameValue(C.access(undefined), undefined);
        assert.sameValue(C.access(null), undefined);
    }

    assert.throws(TypeError, () => {
        C.access({});
    }, "Object without private field should throw");
})();

// Chained case
(() => {
    class C {
        #field = 'test';
    
        static chainedAccess(o) {
            return o.private?.#field;
        }
    }

    let c = new C;
    let o = {private: c};
    for (let i = 0; i < 100000; i++) {
        assert.sameValue(C.chainedAccess(o), 'test');
        assert.sameValue(C.chainedAccess({}), undefined);
        assert.sameValue(C.chainedAccess({private: null}), undefined);
    }

    assert.throws(TypeError, () => {
        o.private = {};
        C.chainedAccess(o);
    }, "Object without private field should throw");
})();

(() => {
    class C {
        #field;

        setField(v) {
            this.#field = v;
        }
    
        static access(o) {
            return o.#field?.property;
        }
    }

    let c = new C;
    for (let i = 0; i < 100000; i++) {
        c.setField({property: 'test'});
        assert.sameValue(C.access(c), 'test');

        c.setField(undefined);
        assert.sameValue(C.access(c), undefined);

        c.setField(null);
        assert.sameValue(C.access(c), undefined);
    }

    assert.throws(TypeError, () => {
        C.access({});
    }, "Object without private field should throw");
})();

