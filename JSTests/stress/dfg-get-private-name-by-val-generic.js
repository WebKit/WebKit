// Convoluted program which will compile turn GetPrivateName into JITGetByVal IC. GetByVal
// will never OSR here due to bad structures, but will instead repatch to the generic case.

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

class Base {
    constructor(i) {
        if (i & 1)
            this.megaCommonExtraStuff = true;
        if (i & 2) 
            this.commonExtraStuff = true;
        if (i & 4)
            this.moreExtraStuff = true;
        if (i & 8)
            this.soMuchExtraStuff = true;
    }
}

class C extends Base {
    #field = 'test';

    constructor(i) {
        super(i);
        if (i > 5000)
            this.extraStuff = true;
        if (i === 9999)
            this.moreStuff = true;
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

let c = new C(100);
assert.throws(TypeError, () => c.getField.call({}));
