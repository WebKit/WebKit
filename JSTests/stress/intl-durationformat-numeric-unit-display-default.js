function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function displays(options) {
    const resolved = new Intl.DurationFormat("en", options).resolvedOptions();
    return [resolved.hoursDisplay, resolved.minutesDisplay, resolved.secondsDisplay, resolved.millisecondsDisplay].join("/");
}

function format(options, duration) {
    return new Intl.DurationFormat("en", options).format(duration);
}

// A numeric or 2-digit unit makes the following minutes and seconds default to "always".
shouldBe(format({ hours: "numeric" }, { hours: 1 }), "1:00:00");
shouldBe(format({ hours: "numeric" }, { hours: 1, minutes: 2, seconds: 3 }), "1:02:03");
shouldBe(format({ hours: "numeric" }, { hours: 0 }), "0:00:00");
shouldBe(format({ hours: "2-digit" }, { hours: 1 }), "01:00:00");
shouldBe(displays({ hours: "numeric" }), "always/always/always/auto");
shouldBe(displays({ hours: "2-digit" }), "always/always/always/auto");

shouldBe(format({ minutes: "numeric" }, { minutes: 5 }), "5:00");
shouldBe(format({ minutes: "2-digit" }, { minutes: 5 }), "05:00");
shouldBe(displays({ minutes: "numeric" }), "auto/always/always/auto");
shouldBe(displays({ minutes: "2-digit" }), "auto/always/always/auto");

shouldBe(format({ hours: "numeric", minutes: "numeric" }, { hours: 1 }), "1:00:00");
shouldBe(displays({ hours: "numeric", minutes: "numeric" }), "always/always/always/auto");

shouldBe(format({ style: "long", hours: "numeric" }, { hours: 1 }), "1:00:00");
shouldBe(format({ style: "narrow", hours: "numeric" }, { hours: 1 }), "1:00:00");
shouldBe(displays({ style: "long", hours: "numeric" }), "always/always/always/auto");
shouldBe(displays({ style: "narrow", hours: "numeric" }), "always/always/always/auto");

// The per-unit spelling must agree with the digital style.
shouldBe(format({ hours: "numeric" }, { hours: 1 }), format({ style: "digital" }, { hours: 1 }));
shouldBe(displays({ hours: "numeric" }), displays({ style: "digital" }));

// An explicit display option still wins over the default.
shouldBe(format({ hours: "numeric", secondsDisplay: "auto" }, { hours: 1 }), "1:00");
shouldBe(displays({ hours: "numeric", secondsDisplay: "auto" }), "always/always/auto/auto");
shouldBe(format({ hours: "numeric", minutesDisplay: "auto", secondsDisplay: "auto" }, { hours: 1 }), "1");
shouldBe(displays({ hours: "numeric", minutesDisplay: "auto", secondsDisplay: "auto" }), "always/auto/auto/auto");

// Units that do not follow a numeric unit keep "auto".
shouldBe(displays({}), "auto/auto/auto/auto");
shouldBe(displays({ seconds: "numeric" }), "auto/auto/always/auto");
shouldBe(format({ seconds: "numeric" }, { seconds: 5 }), "5");
shouldBe(displays({ hours: "long" }), "always/auto/auto/auto");

// A non-numeric unit after a numeric one is still rejected.
let error;
try {
    new Intl.DurationFormat("en", { hours: "numeric", minutes: "long" });
} catch (e) {
    error = e;
}
shouldBe(error instanceof RangeError, true);
