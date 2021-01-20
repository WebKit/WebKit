function upsilonReferencingItsPhi(index, input)
{
    // All uses of "outputDouble" are converted to float.
    // Inside the loop, the Upsilon is referencing its own Phi. This should
    // not prevent the conversion.
    let outputDouble = input;
    while (index) {
        if (index & 0x4)
            outputDouble = Math.fround(outputDouble) + Math.PI;
        index = index >>> 1;
    }
    return Math.fround(outputDouble);
}
noInline(upsilonReferencingItsPhi);

let expectedNotTaken = Math.fround(Math.LN2);
let expectedTaken = Math.fround(Math.fround(Math.LN2) + Math.PI);
for (let i = 0; i < 1e6; ++i) {
    let branchNotTakenResult = upsilonReferencingItsPhi(3, Math.LN2);
    if (branchNotTakenResult !== expectedNotTaken)
        throw "Failed upsilonReferencingItsPhi(3, Math.LN2) at i = " + i + " result = " + branchNotTakenResult;

    let branchTakenResult = upsilonReferencingItsPhi(7, Math.LN2);
    if (branchTakenResult !== expectedTaken)
        throw "Failed upsilonReferencingItsPhi(7, Math.LN2) at i = " + i + " result = " + branchTakenResult;
}

// Same as above, but this time it is always better to convert the outside Phi-Upsilon.
function upsilonReferencingItsPhiAllFloat(index, input)
{
    let outputDouble = Math.fround(input);
    while (index) {
        if (index & 0x4)
            outputDouble = Math.fround(outputDouble) + Math.PI;
        index = index >>> 1;
    }
    return Math.fround(outputDouble);
}
noInline(upsilonReferencingItsPhiAllFloat);

for (let i = 0; i < 1e6; ++i) {
    let branchNotTakenResult = upsilonReferencingItsPhiAllFloat(3, Math.LN2);
    if (branchNotTakenResult !== expectedNotTaken)
        throw "Failed upsilonReferencingItsPhiAllFloat(3, Math.LN2) at i = " + i + " result = " + branchNotTakenResult;

    let branchTakenResult = upsilonReferencingItsPhiAllFloat(7, Math.LN2);
    if (branchTakenResult !== expectedTaken)
        throw "Failed upsilonReferencingItsPhiAllFloat(7, Math.LN2) at i = " + i + " result = " + branchTakenResult;
}

// This time, converting to float would be a mistake because one of the Phi
// is not converted.
function upsilonReferencingItsPhiWithoutConversion(index, input)
{
    let outputDouble = input;
    while (index) {
        if (index & 0x4)
            outputDouble = Math.fround(outputDouble) + Math.PI;
        index = index >>> 1;
    }
    return outputDouble;
}
noInline(upsilonReferencingItsPhiWithoutConversion);

let expectedNotTakenWithoutConversion = Math.LN2;
let expectedTakenWithoutConversion = Math.fround(Math.LN2) + Math.PI;
for (let i = 0; i < 1e6; ++i) {
    let branchNotTakenResult = upsilonReferencingItsPhiWithoutConversion(3, Math.LN2);
    if (branchNotTakenResult !== expectedNotTakenWithoutConversion)
        throw "Failed upsilonReferencingItsPhiWithoutConversion(3, Math.LN2) at i = " + i + " result = " + branchNotTakenResult;

    let branchTakenResult = upsilonReferencingItsPhiWithoutConversion(7, Math.LN2);
    if (branchTakenResult !== expectedTakenWithoutConversion)
        throw "Failed upsilonReferencingItsPhiWithoutConversion(7, Math.LN2) at i = " + i + " result = " + branchTakenResult;
}

function conversionPropagages(flags, a, b)
{
    let result = 0.5;
    if (flags & 0x1) {
        if (flags & 0x2) {
            if (flags & 0x4) {
                if (flags & 0x8) {
                    result = Math.fround(a) + Math.fround(b);
                } else {
                    result = 6.5;
                }
            } else {
                result = 4.5;
            }
        } else {
            result = 2.5;
        }
    } else {
        result = 1.5;
    }
    return Math.fround(result);
}
noInline(conversionPropagages);

let conversionPropagageExpectedResult = Math.fround(Math.fround(Math.LN2) + Math.fround(Math.PI));
for (let i = 0; i < 1e6; ++i) {
    let result = conversionPropagages(0xf, Math.LN2, Math.PI);
    if (result !== conversionPropagageExpectedResult)
        throw "Failed conversionPropagages(0xf, Math.LN2, Math.PI)";
}


function chainedUpsilonBothConvert(condition1, condition2, a, b)
{
    let firstPhi;
    if (condition1)
        firstPhi = Math.fround(a);
    else
        firstPhi = Math.fround(b);

    let secondPhi;
    if (condition2)
        secondPhi = firstPhi + 2;
    else
        secondPhi = firstPhi + 1;
    return Math.fround(secondPhi);
}
noInline(chainedUpsilonBothConvert);

let expectedChainedUpsilonBothConvert = Math.fround(Math.fround(Math.PI) + Math.fround(1));
for (let i = 0; i < 1e6; ++i) {
    if (chainedUpsilonBothConvert(1, 0, Math.PI, Math.LN2) !== expectedChainedUpsilonBothConvert)
        throw "Failed chainedUpsilonBothConvert(1, 0, Math.PI, Math.LN2)";
}

function chainedUpsilonFirstConvert(condition1, condition2, a, b)
{
    // This first phi is trivially simplified by the fround()
    // of the second if-else.
    let firstPhi;
    if (condition1)
        firstPhi = Math.fround(a);
    else
        firstPhi = Math.fround(b);

    // This second one cannot ever be converted because the
    // result is not rounded to float.
    let secondPhi;
    if (condition2)
        secondPhi = Math.fround(firstPhi) + Math.fround(1/3);
    else
        secondPhi = Math.fround(firstPhi) - Math.fround(1/3);
    return secondPhi;
}
noInline(chainedUpsilonFirstConvert);

let expectedChainedUpsilonFirstConvert = Math.fround(Math.PI) - Math.fround(1/3);
for (let i = 0; i < 1e6; ++i) {
    if (chainedUpsilonFirstConvert(1, 0, Math.PI, Math.LN2) !== expectedChainedUpsilonFirstConvert)
        throw "Failed chainedUpsilonFirstConvert(1, 0, Math.PI, Math.LN2)";
}
