//@ requireOptions("--usePublicClassFields=1", "--usePrivateClassFields=1")

let assert = {
    throws: (expectedError, functor) => {
        try {
            functor();
        } catch(e) {
            if (!(e instanceof expectedError))
                throw new Error("Expected to throw: " + expectedError.name + " but throws: " + e.name);
        }
    }
}

assert.throws(TypeError, () => {
    class Base {
        [{}.#x] = 1; 
        #x = "foo";
    }
});

