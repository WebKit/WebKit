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

shouldBe(format({ milliseconds: "numeric" }, { milliseconds: 500 }), "0.5 sec");
shouldBe(format({ milliseconds: "numeric" }, { microseconds: 500 }), "0.0005 sec");
shouldBe(format({ milliseconds: "numeric" }, { nanoseconds: 1 }), "0.000000001 sec");
shouldBe(format({ milliseconds: "numeric" }, { hours: 1, milliseconds: 500 }), "1 hr, 0.5 sec");
shouldBe(format({ milliseconds: "numeric" }, { hours: -1, milliseconds: -500 }), "-1 hr, 0.5 sec");
shouldBe(format({ style: "long", milliseconds: "numeric" }, { milliseconds: 500 }), "0.5 seconds");
shouldBe(format({ style: "long", microseconds: "numeric" }, { minutes: 3, microseconds: 250 }), "3 minutes, 0.25 milliseconds");
shouldBe(format({ style: "long", microseconds: "numeric" }, { seconds: 2, nanoseconds: 250 }), "2 seconds, 0.00025 milliseconds");
shouldBe(format({ style: "long", nanoseconds: "numeric" }, { nanoseconds: 500 }), "0.5 microseconds");
shouldBe(format({ style: "long", nanoseconds: "numeric" }, { milliseconds: 1, nanoseconds: 7 }), "1 millisecond, 0.007 microseconds");

shouldBe(format({ milliseconds: "numeric", fractionalDigits: 2 }, { milliseconds: 500 }), "0.50 sec");
shouldBe(format({ milliseconds: "numeric", fractionalDigits: 2 }, { milliseconds: 5 }), "0.00 sec");
shouldBe(format({ milliseconds: "numeric", fractionalDigits: 0 }, { milliseconds: 500 }), "0 sec");
shouldBe(format({ milliseconds: "numeric", fractionalDigits: 0 }, { hours: 1, milliseconds: 500 }), "1 hr, 0 sec");

shouldBe(parts({ milliseconds: "numeric" }, { milliseconds: 500 }), "integer:0|decimal:.|fraction:5|literal: |unit:sec");
shouldBe(parts({ milliseconds: "numeric" }, { hours: 1, milliseconds: 500 }), "integer:1|literal: |unit:hr|literal:, |integer:0|decimal:.|fraction:5|literal: |unit:sec");

shouldBe(format({ milliseconds: "numeric" }, { milliseconds: 0 }), "");
shouldBe(format({ milliseconds: "numeric" }, { hours: 1 }), "1 hr");
shouldBe(format({ milliseconds: "numeric" }, { hours: 1, milliseconds: 0 }), "1 hr");
shouldBe(format({ style: "long", microseconds: "numeric" }, { seconds: 2 }), "2 seconds");
shouldBe(format({ milliseconds: "numeric" }, { seconds: 1, milliseconds: 500 }), "1.5 sec");
shouldBe(format({ milliseconds: "numeric", secondsDisplay: "always" }, { milliseconds: 500 }), "0.5 sec");
shouldBe(format({ milliseconds: "numeric", secondsDisplay: "always" }, { hours: 1 }), "1 hr, 0 sec");
