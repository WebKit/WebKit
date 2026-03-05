function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// In MatchOnly mode (.test()), captures inside FixedCount groups must be
// cleared between iterations. The bug: when an earlier alternative succeeds
// in iteration N+1, a later alternative's capture retains a stale value from
// iteration N instead of being undefined.
//
// /(?:(a)|(b)){2}\2/ on "ba":
//   iter1: (b) matches → capture[2]="b"
//   iter2: (a) matches, (b) not attempted → capture[2] should be undefined
//   \2 → undefined → empty match → true

var re = /(?:(a)|(b)){2}\2/;
for (var i = 0; i < 200; i++) {
    re.exec("aabb");
    re.test("aabb");
}
shouldBe(re.exec("ba")[2], undefined);
shouldBe(re.test("ba"), true);

var re2 = /(?:(a)|(b)){3}\2/;
for (var i = 0; i < 200; i++) {
    re2.exec("aabbaa");
    re2.test("aabbaa");
}
shouldBe(re2.exec("baa")[2], undefined);
shouldBe(re2.test("baa"), true);
