function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

const setA = new Set(["a", "b", "c", "d", "e"]);

const values = ["f", "g", "h", "i", "j"];
const setB = {
    size: values.length,
    has() { return true },
    keys() {
        let index = 0;
        return {
            next() {
                const done = index >= values.length;
                if (!setA.has("f")) setA.add("f");
                return {
                    done,
                    value: values[index++]
                };
            }
        };
    }
};

const result = setA.symmetricDifference(setB);
shouldBe(JSON.stringify(Array.from(result)), JSON.stringify(["a", "b", "c", "d", "e", "g", "h", "i", "j"]));
