function sameValue(a, b) {
    if (a !== b && !(Number.isNaN(a) && Number.isNaN(b)))
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(setMethod) {
    var dt = new Date(NaN);
    var valueOfCalled = 0;
    var value = {
      valueOf() {
        valueOfCalled++;
        dt.setTime(0);
        return 1;
      }
    };
    var result = setMethod.call(dt, value);
    sameValue(valueOfCalled, 1);
    sameValue(result, NaN);
    sameValue(dt.getTime(), 0);
}

var setMethods = [
    Date.prototype.setHours,
    Date.prototype.setMilliseconds,
    Date.prototype.setMinutes,
    Date.prototype.setSeconds,
    Date.prototype.setUTCHours,
    Date.prototype.setUTCMilliseconds,
    Date.prototype.setUTCMinutes,
    Date.prototype.setUTCSeconds,
];

for (var setMethod of setMethods) {
    test(setMethod);
}
