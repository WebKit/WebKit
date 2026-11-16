// Simulates the common webpack/babel pattern:
// Object.defineProperty(exports, "__esModule", { value: !0 })

function bench() {
    var sum = 0;
    for (var i = 0; i < 1000000; i++) {
        var obj = {};
        Object.defineProperty(obj, "__esModule", { value: true });
        sum += obj.__esModule ? 1 : 0;
    }
    return sum;
}

var result = bench();
if (result !== 1000000)
    throw new Error("Bad result: " + result);
