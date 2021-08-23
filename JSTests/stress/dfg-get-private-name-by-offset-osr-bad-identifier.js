// Convoluted program which will compile turn GetPrivateName into GetByOffset, and then OSRExit
// from getField() due to a CheckIsConstant failure when a different identifier is used.

let assert = {
    equals: function (a, e) {
       if (a !== e) 
        throw new Error(`Expected: ${e} but got: ${a}`);
    },
    throws: function(exception, functor) {
        let threwException;
        try {
            functor();
            threwException = false;
        } catch(e) {
            threwException = true;
            if (!e instanceof exception)
                throw new Error(`Expected to throw a ${exception.name} but it threw: ${e}`);
        }

        if (!threwException)
            throw new Error(`Expected to throw a ${exception.name} but did not throw`);
    }
};

function factoryClass() {
    class C {
        #field = 'test';
    
        setField(v) {
            this.#field = v;
        }
    
        getField() {
            return this.#field;
        }
    }
    noInline(C.prototype.setField);
    noInline(C.prototype.getField);
    noDFG(C.prototype.setField);
    noFTL(C.prototype.setField);

    return C;
}

let C = factoryClass();

for (let i = 0; i < 10000; i++) {
    let c = new C();
    assert.equals(c.getField(), 'test');
    c.setField('foo' + i);
    assert.equals(c.getField(), 'foo' + i);
}

let C2 = factoryClass(); // invalidate first version of setField due to Scope invalidation point

// Triggers new version without folding get_from_scope
for (let i = 0; i < 10000; i++) {
    let c = new C();
    assert.equals(c.getField(), 'test');
    c.setField('foo' + i);
    assert.equals(c.getField(), 'foo' + i);
}

let c2 = new C2();
assert.throws(TypeError, function () {
    c2.getField.call(new C()); // trigger OSR exit due to bad constant value
});
