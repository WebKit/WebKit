function assert(actual, expected) {
    if (actual !== expected)
        throw Error("Expected: " + expected + " Actual: " + actual);
}

function classExpr() {
    return class {
        #method() {
            return 'foo';
        }

        access() {
           return this.#method();
        }
    }
}

let C1 = classExpr();
let C2 = classExpr();

let arr = [C1, C2];
for (let i = 0; i < 10000; i++) {
    let c = new arr[i % arr.length]();
    assert(c.access(), 'foo');
}

