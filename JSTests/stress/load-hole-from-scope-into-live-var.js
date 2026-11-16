//@ runDefault
var result = eval(`
try {
    switch (0) {
    case 1:
        let x = eval();
    default:
        x;
    }
} catch (e) {
}
`);
if (result !== void 0)
    throw "Bad result: " + result;
