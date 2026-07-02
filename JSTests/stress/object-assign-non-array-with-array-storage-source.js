//@ $skipModes << :lockdown
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function test(data) {
    shouldBe($vm.indexingMode(data.source), data.sourceIndexingType);
    let result = Object.assign(data.target, data.source);
    if (data.verifyWithTarget)
        for (const [key, value] of Object.entries(data.target))
            shouldBe(result[key], value);
    if (data.verifyWithSource)
        for (const [key, value] of Object.entries(data.source))
            shouldBe(result[key], value);
}

let o1 = { "0": "a", 1: "b", p1: 1, p2: 2, p3: 1, p4: 2, p5: 1, p6: 2, p7: 1, p8: 2, "1000": 1000 };
let o2 = { "0": 2, p1: 1, "1000": 1000 };
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
    // NonArrayWithArrayStorage
    {
        target: { "3": -1 },
        source: { "0": "a", 1: "b", p1: 1, p2: 2, p3: 1, p4: 2, p5: 1, p6: 2, p7: 1, p8: 2, "1000": 1000 },
        sourceIndexingType: "NonArrayWithArrayStorage",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "0": 1, },
        source: { "0": 2, p1: 1, "1000": 1000 },
        sourceIndexingType: "NonArrayWithArrayStorage",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "-3": -1 },
        source: { "0": "a", 1: "b", p1: 1, p2: 2, p3: 1, p4: 2, p5: 1, p6: 2, p7: 1, p8: 2, "1000": 1000 },
        sourceIndexingType: "NonArrayWithArrayStorage",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    {
        target: { p: 1 },
        source: { "0": 2, p1: 1, "1000": 1000 },
        sourceIndexingType: "NonArrayWithArrayStorage",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    {
        target: { "0": 2 },
        source: {},
        sourceIndexingType: "NonArray",
        verifyWithTarget: true,
        verifyWithSource: false,
    },
    {
        target: {},
        source: { "0": 2, p1: 1, "1000": 1000 },
        sourceIndexingType: "NonArrayWithArrayStorage",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    // NonArrayWithSlowPutArrayStorage
    {
        target: { "3": -1 },
        source: o1,
        sourceIndexingType: "NonArrayWithSlowPutArrayStorage",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "0": 1, },
        source: o2,
        sourceIndexingType: "NonArrayWithSlowPutArrayStorage",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "-3": -1 },
        source: o1,
        sourceIndexingType: "NonArrayWithSlowPutArrayStorage",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    {
        target: { p: 1 },
        source: o2,
        sourceIndexingType: "NonArrayWithSlowPutArrayStorage",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    {
        target: { "0": 2 },
        source: {},
        sourceIndexingType: "NonArray",
        verifyWithTarget: true,
        verifyWithSource: false,
    },
    {
        target: {},
        source: o2,
        sourceIndexingType: "NonArrayWithSlowPutArrayStorage",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
];

for (let i = 0; i < 1e4; i++) {
    for (let data of testData)
        test(data);
}
