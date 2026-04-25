function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const durationFormat = new Intl.DurationFormat("en", { fractionalDigits: undefined });
shouldBe(durationFormat.resolvedOptions().fractionalDigits, undefined);
