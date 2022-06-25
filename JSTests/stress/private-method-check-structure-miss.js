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

class C {
    constructor() {
        if (i >= 5000)
            this.property = i;
    }

    #method(v) {
        this.field = v;
    }

    access(v) {
        this.#method(v);
    }
}
noInline(C.prototype.access);

let c = new C();
for (; i < 10000; i++) {
    c.access('foo' + i);
    assert.sameValue(c.field, 'foo' + i);
}

assert.shouldThrow(TypeError, function() {
    c.access.call({}, 'test');
});

