function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let largeSet1 = new Set();
let largeSet2 = new Set();

for (let i = 0; i < 1000; i++) {
    largeSet1.add(i);
}

for (let i = 500; i < 1500; i++) {
    largeSet2.add(i);
}

let result = largeSet1.symmetricDifference(largeSet2);

shouldBe(result.size, 1000);

shouldBe(result.has(0), true);
shouldBe(result.has(499), true);
shouldBe(result.has(500), false);
shouldBe(result.has(999), false);
shouldBe(result.has(1000), true);
shouldBe(result.has(1499), true);

let perfSetLike = {
    size: 500,
    has: function(key) {
        return key >= 250 && key <= 749;
    },
    keys: function() {
        let values = [];
        for (let i = 250; i <= 749; i++) {
            values.push(i);
        }
        let index = 0;
        return {
            next: function() {
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

let subset = new Set();
for (let i = 0; i < 500; i++) {
    subset.add(i);
}

result = subset.symmetricDifference(perfSetLike);

shouldBe(result.size, 500);

shouldBe(result.has(0), true);
shouldBe(result.has(249), true);
shouldBe(result.has(250), false);
shouldBe(result.has(499), false);
shouldBe(result.has(500), true);
shouldBe(result.has(749), true);

let disjointSet1 = new Set();
let disjointSet2 = new Set();

for (let i = 0; i < 500; i++) {
    disjointSet1.add(i);
}

for (let i = 1000; i < 1500; i++) {
    disjointSet2.add(i);
}

result = disjointSet1.symmetricDifference(disjointSet2);

shouldBe(result.size, 1000);

for (let i = 0; i < 500; i++) {
    shouldBe(result.has(i), true);
}

for (let i = 1000; i < 1500; i++) {
    shouldBe(result.has(i), true);
}

for (let i = 500; i < 1000; i++) {
    shouldBe(result.has(i), false);
}
