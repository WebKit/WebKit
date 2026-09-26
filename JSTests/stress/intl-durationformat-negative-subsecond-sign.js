function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function format(options, duration) {
    return new Intl.DurationFormat("en", options).format(duration);
}

function parts(options, duration) {
    return new Intl.DurationFormat("en", options).formatToParts(duration).map((part) => `${part.type}:${part.value}`).join("|");
}

shouldBe(format({ seconds: "numeric" }, { milliseconds: -500 }), "-0.5");
shouldBe(format({ seconds: "numeric" }, { microseconds: -500 }), "-0.0005");
shouldBe(format({ seconds: "numeric" }, { nanoseconds: -1 }), "-0.000000001");
shouldBe(format({ seconds: "numeric" }, { milliseconds: -999, microseconds: -999, nanoseconds: -999 }), "-0.999999999");
shouldBe(format({ seconds: "2-digit" }, { milliseconds: -500 }), "-00.5");
shouldBe(format({ style: "digital", hoursDisplay: "auto", minutesDisplay: "auto" }, { milliseconds: -500 }), "-00.5");

shouldBe(format({ seconds: "numeric", fractionalDigits: 0 }, { milliseconds: -500 }), "-0");
shouldBe(format({ seconds: "numeric", fractionalDigits: 3 }, { nanoseconds: -1 }), "-0.000");
shouldBe(format({ seconds: "numeric", fractionalDigits: 3 }, { milliseconds: -500 }), "-0.500");

shouldBe(format({ style: "long", milliseconds: "numeric", secondsDisplay: "always" }, { milliseconds: -500 }), "-0.5 seconds");
shouldBe(format({ style: "long", microseconds: "numeric", millisecondsDisplay: "always" }, { microseconds: -500 }), "-0.5 milliseconds");
shouldBe(format({ style: "long", nanoseconds: "numeric", microsecondsDisplay: "always" }, { nanoseconds: -500 }), "-0.5 microseconds");

shouldBe(parts({ seconds: "numeric" }, { milliseconds: -500 }), "minusSign:-|integer:0|decimal:.|fraction:5");
shouldBe(parts({ style: "long", milliseconds: "numeric", secondsDisplay: "always" }, { milliseconds: -500 }), "minusSign:-|integer:0|decimal:.|fraction:5|literal: |unit:seconds");

shouldBe(format({ seconds: "numeric" }, { milliseconds: 500 }), "0.5");
shouldBe(format({ seconds: "numeric" }, { milliseconds: -1500 }), "-1.5");
shouldBe(format({ seconds: "numeric" }, { seconds: -1 }), "-1");
shouldBe(format({ seconds: "numeric", secondsDisplay: "always" }, { seconds: 0 }), "0");
shouldBe(format({ style: "digital" }, { milliseconds: -500 }), "-0:00:00.5");
shouldBe(format({ style: "digital" }, { hours: -1, milliseconds: -500 }), "-1:00:00.5");
shouldBe(format({ style: "digital", hoursDisplay: "auto" }, { minutes: -1, milliseconds: -500 }), "-01:00.5");
shouldBe(format({ style: "long", milliseconds: "numeric", secondsDisplay: "always" }, { minutes: -1, milliseconds: -500 }), "-1 minute, 0.5 seconds");
