function shouldThrow(test, func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error(`test ${test} not thrown`);
    if (String(error) !== errorMessage)
        throw new Error(`test ${test} bad error: ${String(error)}`);
}

function shouldNotThrow(func) {
    func();
}

shouldThrow(1, () => {
    "use strict";
    a = this.a = 0;
}, "ReferenceError: Can't find variable: a");

shouldThrow(2, () => {
    "use strict";
    this.b = 0;
    b = 0;
}, "ReferenceError: Can't find variable: b");

shouldThrow(3, () => {
    "use strict";
    eval(`
        let c = 0;
    `);
    c = 0;
}, "ReferenceError: Can't find variable: c");

shouldNotThrow(() => {
    eval(`
        let d = 0;
    `);
    d = 0;
});

shouldNotThrow(() => {
    "use strict";
    let e = 0;
    e = 0;
});
