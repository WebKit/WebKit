function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const set1 = new Set([1, 2, 3, 4, 5]);
const set2 = new Set([3, 4, 5, 6, 7, 8]);

let hasGet = 0;
let hasCall = 0;
Object.defineProperty(set2, "has", {
    get() {
        hasGet++;
        return function (...args) {
            hasCall++;
            return Set.prototype.has.call(set2, ...args);
        }
    }
});

let keysGet = 0;
let keysCall = 0;
Object.defineProperty(set2, "keys", {
    get() {
        keysGet++;
        return function (...args) {
            keysCall++;
            return Set.prototype.keys.call(set2, ...args);
        }
    }
});


const result = set1.intersection(set2);

shouldBe(result.size, 3);

shouldBe(result.has(3), true);
shouldBe(result.has(4), true);
shouldBe(result.has(5), true);

shouldBe(hasGet, 1);
shouldBe(hasCall, 5);

shouldBe(keysGet, 1);
shouldBe(keysCall, 0);
