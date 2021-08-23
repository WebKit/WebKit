let assert = {
    sameValue: function (a, e) {
       if (a !== e) 
        throw new Error("Expected: " + e + " but got: " + a);
    },
    shouldThrow: function(exception, functor) {
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

let i = 0;

class Base {
    constructor() {
        if (i % 2)
            this.anotherField = i;
    }
}

class C extends Base {
    #method() {
        return 'test';
    }

    access() {
        return this.#method();
    }
}
noInline(C.prototype.access);

for (; i < 10000; i++) {
    count = i;
    let c = new C();
    assert.sameValue(c.access(), 'test');
}

assert.shouldThrow(TypeError, () => {
    c.access.call({});
});

