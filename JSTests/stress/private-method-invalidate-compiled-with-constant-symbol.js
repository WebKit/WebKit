let assert = {
    sameValue: function (a, e) {
       if (a !== e) 
        throw new Error("Expected: " + e + " but got: " + a);
    },
    throws: function(exception, functor) {
        let threwException;
        try {
            functor();
            threwException = false;
        } catch(e) {
            threwException = true;
            if (!e instanceof exception)
                throw new Error("Expected to throw: " + exception.name + " but it throws: " + e.name);
        }

        if (!threwException)
            throw new Error("Expected to throw: " + exception.name + " but executed without exception");
    }
}

function factoryClass() {
    class C {
        #method() {
            return 'test';
        }
    
        access() {
            return this.#method();
        }
    }
    noInline(C.prototype.access);

    return C;
}

let C = factoryClass();

for (let i = 0; i < 10000; i++) {
    let c = new C();
    assert.sameValue(c.access(), 'test');
    assert.throws(TypeError, () => {
        c.access.call({});
    });
}

let C2 = factoryClass();

let c2 = new C2();
assert.sameValue(c2.access(), 'test');

let c = new C();

assert.throws(TypeError, () => {
    c.access.call(c2);
});

assert.throws(TypeError, () => {
    c2.access.call(c);
});
