//@ requireOptions("--usePrivateClassFields=1")

// PrivateField "Create" access should throw if writing to a non-existent PrivateName.
let c, i = 0, threw = false;
class C {
    constructor() {
        return i < 29 ? this : c;
    }
}

class CC extends C {
    #x = i++;
    x() { return this.#x; }
}

c = new CC;
try {
    for (let j = 1; j < 50; ++j) {
        let x = new CC;
        if (x.x() !== j)
            throw new Error(`Expected x.x() to be ${j}, but found ${x.x()}`);
    }
} catch (e) {
    threw = true;
    if (i !== 30 || e.constructor !== TypeError) {
        throw e;
    }
}

if (!threw)
    throw new Error("Expected TypeError, but no exception was thrown");
