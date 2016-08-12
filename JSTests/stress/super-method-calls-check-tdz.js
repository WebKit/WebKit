class Base {}

class Derived extends Base {
    constructor() {
        super.method();
        super();
    }
};

function test() {
    let failed = false
    try {
        new Derived();
        failed = true;
    } catch (e) {
        if (!(e instanceof ReferenceError))
            failed = true;
    }
    if (failed)
        throw "did not throw reference error";
}

for (i = 0; i < 10000; i++) {
    test();
}
