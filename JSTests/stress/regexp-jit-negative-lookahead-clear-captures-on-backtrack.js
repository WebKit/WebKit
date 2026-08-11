
function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: actual=" + JSON.stringify(actual) + " expected=" + JSON.stringify(expected));
}

for (var i = 0; i < 1e4; ++i) {
    shouldBe(/(?!(a){2})./.exec("ab"), ["a", undefined]);
    shouldBe(/(?!(ab){2})./.exec("abc"), ["a", undefined]);
    shouldBe(/(?!(?<n>a){2})./.exec("ab"), ["a", undefined]);
    var m = /(?!(?<n>a){2})./.exec("ab");
    shouldBe(m.groups.n, undefined);
    shouldBe(/(?!(a)(b))./.exec("abc"), ["b", undefined, undefined]);
}
