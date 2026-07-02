
function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: actual=" + JSON.stringify(actual) + " expected=" + JSON.stringify(expected));
}

for (var i = 0; i < 1e4; ++i) {
    shouldBe(/(?:(a+){2}|.)/.exec("ax"), ["a", undefined]);
    shouldBe(/(?:(ab){3}|.)/.exec("ababx"), ["a", undefined]);
    shouldBe(/(?:(a){2}x|.)/.exec("aab"), ["a", undefined]);
    shouldBe(/(?:(?<n>a+){2}|.)/.exec("ax"), ["a", undefined]);
    var m = /(?:(?<n>a+){2}|.)/.exec("ax");
    shouldBe(m.groups.n, undefined);
    shouldBe(/(?:(?:(a)b){2}|.)/.exec("abx"), ["a", undefined]);
}
