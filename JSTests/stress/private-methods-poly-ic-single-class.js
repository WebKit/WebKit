function assert(actual, expected) {
    if (actual !== expected)
        throw Error("Expected: " + expected + " Actual: " + actual);
}

class C {
    #method() {
        return 'foo';
    }

    access() {
       return this.#method();
    }
}

let c1 = new C();
let c2 = new C();
c2.bar = 'bar';
let arr = [c1, c2];
for (let i = 0; i < 10000; i++) {
    assert(arr[i % arr.length].access(), 'foo');
}

