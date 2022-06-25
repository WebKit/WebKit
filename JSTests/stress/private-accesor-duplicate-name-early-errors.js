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
}

assert.throws(SyntaxError, function() {
    eval(`
        class C {
            set #m(v) { this._v = v; }
            set #m(u) {}
        }
    `);
});

assert.throws(SyntaxError, function() {
    eval(`
        class C {
            #m;
            set #m(v) { this._v = v; }
        }
    `);
});

assert.throws(SyntaxError, function() {
    eval(`
        class C {
            set #m(v) { this._v = v; }
            #m;
        }
    `);
});

assert.throws(SyntaxError, function() {
    eval(`
        class C {
            set #m(v) { this._v = v; }
            #m() {}
        }
    `);
});

assert.throws(SyntaxError, function() {
    eval(`
        class C {
            #m() {}
            get #m() {}
        }
    `);
});

assert.throws(SyntaxError, function() {
    eval(`
        class C {
            get #m() {}
            get #m() {}
        }
    `);
});

assert.throws(SyntaxError, function() {
    eval(`
        class C {
            get #m() {}
            set #m() {}
            #m;
        }
    `);
});

assert.throws(SyntaxError, function() {
    eval(`
        class C {
            get #m() {}
            set #m() {}
            #m() {}
        }
    `);
});

