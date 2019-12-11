// return object
let foo = { }
foo[Symbol.toPrimitive] = function() { return {} };

for (i = 0; i < 100000; i++) {
    let failed = true;
    try {
        foo >= 1;
    } catch (e) {
        if (e instanceof TypeError)
            failed = false;
    }

    if (failed)
        throw "should have thrown on return of object";
}

// The general use of Symbol.toPrimitive is covered in the ES6 tests.
