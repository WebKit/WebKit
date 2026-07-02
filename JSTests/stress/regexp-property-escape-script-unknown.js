function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const unknownChar = "\uE000";
const knownChar = "A";

shouldBe(/\p{Script=Unknown}/u.test(unknownChar), true);
shouldBe(/\p{Script=Unknown}/u.test(knownChar), false);

shouldBe(/\p{Script=Zzzz}/u.test(unknownChar), true);
shouldBe(/\p{Script=Zzzz}/u.test(knownChar), false);

shouldBe(/\p{Script_Extensions=Unknown}/u.test(unknownChar), true);
shouldBe(/\p{Script_Extensions=Unknown}/u.test(knownChar), false);

shouldBe(/\p{Script_Extensions=Zzzz}/u.test(unknownChar), true);
shouldBe(/\p{Script_Extensions=Zzzz}/u.test(knownChar), false);
