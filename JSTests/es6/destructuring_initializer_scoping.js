var outer = [];
function test() {
    var a = {};
    var defaultObj = {
        name: "default",
        length: 3,
        0: "a",
        1: "b",
        2: "c",
        [Symbol.iterator]: Array.prototype[Symbol.iterator]
    };
    function tester({ name } = { name: a.name } = [outer[0], ...outer[1]] = defaultObj) { return name; }
    return tester() === "default" && a.name === "default" && (outer + "") === "a,b,c";
}

if (!test())
    throw new Error("Test failed");
