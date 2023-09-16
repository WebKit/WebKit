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

let testData = [
    // NonArrayWithInt32
    {
        target: { "3": -1 },
        source: { "0": 1, "1": 2, "2": 3, "3": 4, "4": 5, "5": 6, "6": 7, "7": 8, "8": 9, },
        sourceIndexingType: "NonArrayWithInt32",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "0": 1, },
        source: { "0": 2, },
        sourceIndexingType: "NonArrayWithInt32",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "-3": -1 },
        source: { "0": 1, "1": 2, "2": 3, "3": 4, "4": 5, "5": 6, "6": 7, "7": 8, "8": 9, },
        sourceIndexingType: "NonArrayWithInt32",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    {
        target: { p: 1 },
        source: { "0": 2 },
        sourceIndexingType: "NonArrayWithInt32",
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
        source: { "0": 2 },
        sourceIndexingType: "NonArrayWithInt32",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    // NonArrayWithContiguous
    {
        target: { "0": -1 },
        source: { "0": "a", "1": "a", "2": "a", "3": "a", "4": "a", "5": "a", "6": "a", "7": "a", "8": "a", },
        sourceIndexingType: "NonArrayWithContiguous",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "0": 1, },
        source: { "0": "a", },
        sourceIndexingType: "NonArrayWithContiguous",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "-3": -1 },
        source: { "0": "a", "1": "a", "2": "a", "3": "a", "4": "a", "5": "a", "6": "a", "7": "a", "8": "a", },
        sourceIndexingType: "NonArrayWithContiguous",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    {
        target: { p: 1 },
        source: { "0": "a" },
        sourceIndexingType: "NonArrayWithContiguous",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    {
        target: {},
        source: { "0": "a" },
        sourceIndexingType: "NonArrayWithContiguous",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    // NonArrayWithDouble
    {
        target: { "3": -1 },
        source: { "0": 10.12, "1": 20.12, "2": 30.12, "3": 40.12, "4": 50.12, "5": 60.12, "6": 70.12, "7": 80.12, "8": 90.12, },
        sourceIndexingType: "NonArrayWithDouble",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "0": 1, },
        source: { "0": 2.1, },
        sourceIndexingType: "NonArrayWithDouble",
        verifyWithTarget: false,
        verifyWithSource: true,
    },
    {
        target: { "-3": -1 },
        source: { "0": 10.12, "1": 20.12, "2": 30.12, "3": 40.12, "4": 50.12, "5": 60.12, "6": 70.12, "7": 80.12, "8": 90.12, },
        sourceIndexingType: "NonArrayWithDouble",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
    {
        target: { p: 1 },
        source: { "0": 2.1 },
        sourceIndexingType: "NonArrayWithDouble",
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
        source: { "0": 2.1 },
        sourceIndexingType: "NonArrayWithDouble",
        verifyWithTarget: true,
        verifyWithSource: true,
    },
];

for (let i = 0; i < 1e4; i++) {
    for (let data of testData)
        test(data);
}
