function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

function joinDash(arr) {
    return arr.join("-");
}
noInline(joinDash);

function joinDefault(arr) {
    return arr.join();
}
noInline(joinDefault);

var selfArray = [1, 2];
selfArray.push(selfArray);

var mutualA = [1];
var mutualB = [2];
mutualA.push(mutualB);
mutualB.push(mutualA);

var reenterArray = [1, 2, {
    toString() {
        return reenterArray.join("*");
    }
}];

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(joinDash(selfArray), "1-2-");
    shouldBe(joinDefault(selfArray), "1,2,");
    shouldBe(joinDash(mutualA), "1-2,");
    shouldBe(joinDefault(mutualB), "2,1,");
    shouldBe(joinDash(reenterArray), "1-2-");
}
