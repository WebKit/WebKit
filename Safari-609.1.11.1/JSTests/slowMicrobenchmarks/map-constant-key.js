function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);
function test(map, key, value) {
    let loadValue = eval(`${Math.random()}; let k = key; (function getValue() { return map.get(k); });`);
    noInline(loadValue);
    for (let i = 0; i < 1000000; i++) {
        assert(loadValue() === value);
    }
}

let reallyLongString = "";
for (let i = 0; i < 60000; i++) {
    reallyLongString += "i";
}
reallyLongString.toString();

let keys = [
    "foo",
    "fdashfdsahfdashfdsh",
    "rope" + "string",
    reallyLongString,
    259243,
    1238231.2138321,
    -92138.328,
    {foo: 25},
    Symbol("Hello world"),
    true,
    false,
    undefined,
    null,
    NaN,
    -0,
    function foo() {}
];

let start = Date.now();
let map = new Map;
let i = 0;
for (let key of keys) {
    let value = {i: i++};
    map.set(key, value);
    test(map, key, value);
}
const verbose = false;
if (verbose)
    print(Date.now() - start);
