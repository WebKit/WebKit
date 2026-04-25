//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

let before = Date.now();
let instant = Temporal.Now.instant();
let after = Date.now();
shouldBe(instant instanceof Temporal.Instant, true);
shouldBe(instant.epochMilliseconds >= before, true);
shouldBe(instant.epochMilliseconds <= after, true);
