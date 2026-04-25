function test(data) {
    Object.assign(data.target, data.source1, data.source2);
}

let o1 = { "0": "a", 1: "b", p1: 1, p2: 2, p3: 1, p4: 2, p5: 1, p6: 2, p7: 1, p8: 2, "1000": 1000 };
let o2 = { "4": 2, p10: 1, "2000": 2000 };
let o = { a: 10 };
Object.defineProperties(o, {
    "0": {
        get: function () { return this.a; },
        set: function (x) { this.a = x; },
    },
});
o1.__proto__ = o;
o2.__proto__ = o;

let testData = [
    // NonArrayWithInt32
    {
        target: { "3": -1 },
        source1: { "0": 1, "1": 2, "2": 3, "3": 4, "4": 5, "5": 6, "6": 7, "7": 8, "8": 9, },
        source2: { "9": 2, "10": 3, "11": 4, },
    },
    // NonArrayWithContiguous
    {
        target: { "0": -1 },
        source1: { "0": "a", "1": "a", "2": "a", "3": "a", "4": "a", "5": "a", "6": "a", "7": "a", "8": "a", },
        source2: { "9": "a", "10": "a", "11": "a", },
    },
    // NonArrayWithDouble
    {
        target: { "3": -1 },
        source1: { "0": 10.12, "1": 20.12, "2": 30.12, "3": 40.12, "4": 50.12, "5": 60.12, "6": 70.12, "7": 80.12, "8": 90.12, },
        source2: { "9": 10.12, "10": 20.12, "11": 30.12, },
    },

    // NonArrayWithArrayStorage
    {
        target: { "3": -1 },
        source1: { "0": "a", 1: "b", p1: 1, p2: 2, p3: 1, p4: 2, p5: 1, p6: 2, p7: 1, p8: 2, "1000": 1000 },
        source2: { "3": "a", 4: "b", p9: 1, p10: 2, "2000": 2000 },
    },
    // NonArrayWithSlowPutArrayStorage
    {
        target: { "3": -1 },
        source1: o1,
        source2: o2,
    },
];

for (let i = 0; i < 1e4; i++) {
    for (let data of testData)
        test(data);
}
