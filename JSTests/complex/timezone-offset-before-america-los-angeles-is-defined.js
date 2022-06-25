function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// When it is 1111, timezone America/Los_Angeles is not effective yet.
// So, timezone offset is defined by America/Los_Angeles's longitude
// (118.2347W), thus, UTC-0752. Its timezone offset is 472.
shouldBe(new Date(1111, 1).getTimezoneOffset(), 472);
// After timezone America/Los_Angeles is effective, its timezone offset is 480.
shouldBe(new Date(2000, 1).getTimezoneOffset(), 480);

shouldBe(new Date(1111, 1).getTime(), -27104774822000);
shouldBe(new Date(2000, 1).getTime(), 949392000000);
