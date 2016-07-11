function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class DerivedPromise extends Promise {
    static get [Symbol.species]() {
        return Promise;
    }
}

shouldBe(DerivedPromise.all([ 1, 2, 3]) instanceof DerivedPromise, true);
shouldBe(DerivedPromise.race([ 1, 2, 3]) instanceof DerivedPromise, true);
