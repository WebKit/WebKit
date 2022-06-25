// Convoluted program which will compile turn GetPrivateName into GetByOffset, and then OSRExit
// from getField() due to a CheckStructure failure

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

class C {
    #field = 'test';

    constructor(i) {
        if (i > 5000)
            this.extraStuff = true; // Invalidate loop due to structure change
        if (i === 9999)
            this.moreStuff = true; // Trigger a BadCache OSR on get_private_name
    }

    setField(v) {
        this.#field = v;
    }

    getField() {
        return this.#field;
    }
}
noInline(C.prototype.constructor);
noDFG(C.prototype.constructor);
noFTL(C.prototype.constructor);
noInline(C.prototype.setField);
noInline(C.prototype.getField);
noDFG(C.prototype.setField);
noFTL(C.prototype.setField);

for (let i = 0; i < 10000; ++i) {
    let c = new C(i);
    assert.equals(c.getField(), 'test');
    c.setField('foo' + i);
    assert.equals(c.getField(), 'foo' + i);
}

// Check semantic failure still functions
let c = new C(100);
assert.throws(TypeError, () => c.getField.call({}));
