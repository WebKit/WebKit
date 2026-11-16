function shouldThrowRangeError(script) {
    let error;
    try {
        (0, eval)(script);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof RangeError))
        throw new Error("Expected RangeError, got " + error);
}

const depth = 12000;
const open = "[".repeat(depth);
const close = "]".repeat(depth);

shouldThrowRangeError("let " + open + "z" + close + " = [];");
shouldThrowRangeError("var " + open + "z" + close + " = [];");
shouldThrowRangeError("(function (" + open + "z" + close + ") {})([]);");
shouldThrowRangeError("try { throw []; } catch (" + open + "z" + close + ") {}");
shouldThrowRangeError("let " + "{a:".repeat(depth) + "z" + "}".repeat(depth) + " = {};");

let [[[a]]] = [[[42]]];
if (a !== 42)
    throw new Error("shallow array destructuring broken");
let { b: { c } } = { b: { c: 7 } };
if (c !== 7)
    throw new Error("shallow object destructuring broken");
