// This teset passes if it doesn't throw.

function runRegExp()
{
    let m = /ab(c)?d/.exec("abd");
    return m;
}

noInline(runRegExp);

let firstResult = undefined;
let firstResultKeys = undefined;

function assertSameAsFirstResult(testRun, o)
{
    if (firstResult.length != o.length)
        throw testRun + " results have different length than the first results";

    oKeys = Object.keys(o);

    if (firstResultKeys.length != oKeys.length)
        throw testRun + " results have different number of keys than the first result, first result keys: " + firstResultKeys + " this result keys: " + oKeys;

    for (let i = 0; i < firstResultKeys.length; i++)
    {
        if (firstResultKeys[i] != oKeys[i])
            throw testRun + " results mismatch, first result keys: " + firstResultKeys + " this result keys: " + oKeys;
    }
}

let count = 20000;

for (let i=0; i < count; i++) {
    let result = runRegExp();
    if (i == 0) {
        firstResult = result;
        firstResultKeys = Object.keys(firstResult);
        continue;
    }

    assertSameAsFirstResult(i, result);
}
