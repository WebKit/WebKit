var otherObject = {
    valueOf: function() { return 5.1; }
};
// DFG.
var targetDFG = {
    value: 5.5,
    multiply: function(arg) {
        let returnValue = 1;
        if (typeof arg == "number") {
            returnValue = this.value * arg;
        }
        return returnValue + 1;
    }
};
noInline(targetDFG.multiply);

for (let i = 0; i < 400; ++i) {
    if (targetDFG.multiply(otherObject) !== 2)
        throw "Failed targetDFG.multiply(otherObject)";
    let result = targetDFG.multiply(Math.PI);
    if (result !== (5.5 * Math.PI + 1))
        throw "Failed targetDFG.multiply(Math.PI), expected " + (5.5 * Math.PI + 1) + " got " + result + " at iteration " + i;
}
for (let i = 0; i < 1e3; ++i) {
    let result = targetDFG.multiply(Math.PI);
    if (result !== (5.5 * Math.PI + 1))
        throw "Failed targetDFG.multiply(Math.PI), expected " + (5.5 * Math.PI + 1) + " got " + result + " at iteration " + i;
}

// FTL.
var targetFTL = {
    value: 5.5,
    multiply: function(arg) {
        let returnValue = 1;
        if (typeof arg == "number") {
            returnValue = this.value * arg;
        }
        return returnValue + 1;
    }
};
noInline(targetFTL.multiply);

// Warmup to baseline.
for (let i = 0; i < 400; ++i) {
    if (targetFTL.multiply(otherObject) !== 2)
        throw "Failed targetFTL.multiply(otherObject)";
    let result = targetFTL.multiply(Math.PI);
    if (result !== (5.5 * Math.PI + 1))
        throw "Failed targetFTL.multiply(Math.PI), expected " + (5.5 * Math.PI + 1) + " got " + result + " at iteration " + i;
}

// Step over DFG *WITHOUT* OSR Exit.
for (let i = 0; i < 1e6; ++i) {
    if (targetFTL.multiply(otherObject) !== 2)
        throw "Failed targetFTL.multiply(otherObject)";
}

// Now OSR Exit in FTL.
for (let i = 0; i < 1e2; ++i) {
    let result = targetFTL.multiply(Math.PI);
    if (result !== (5.5 * Math.PI + 1))
        throw "Failed targetFTL.multiply(Math.PI), expected " + (5.5 * Math.PI + 1) + " got " + result + " at iteration " + i;
}
