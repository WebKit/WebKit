function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const setPrototypeHas = Set.prototype.has;
const setPrototypeKeys = Set.prototype.keys;

const set1 = new Set([1, 2, 3, 4, 5, 6, 7, 8]);
const set2 = new Set([3, 4, 5]);

let hasGet = 0;
let hasCall = 0;
Object.defineProperty(Set.prototype, "has", {
    get() {
        hasGet++;
        return function (...args) {
            hasCall++;
            return setPrototypeHas.call(set2, ...args);
        }
    }
});

let keysGet = 0;
let keysCall = 0;
Object.defineProperty(Set.prototype, "keys", {
    get() {
        keysGet++;
        return function (...args) {
            keysCall++;
            return setPrototypeKeys.call(set2, ...args);
        }
    }
});


const result = set1.intersection(set2);

shouldBe(result.size, 3);

shouldBe(result.has(3), true);
shouldBe(result.has(4), true);
shouldBe(result.has(5), true);

shouldBe(hasGet, 4);
shouldBe(hasCall, 3);

shouldBe(keysGet, 1);
shouldBe(keysCall, 1);
