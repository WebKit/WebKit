function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

// Should format seconds, milliseconds or microseconds with `roundingMode` is `trunc`
const durations = [
  {
    fractionalDigits: 0,
    expected: "1",
    duration: {
      seconds: 1,
      milliseconds: 500,
    },
  },
  {
    fractionalDigits: 3,
    expected: "0.001",
    duration: {
      milliseconds: 1,
      microseconds: 500,
    }
  },
  {
    fractionalDigits: 6,
    expected: "0.000001",
    duration: {
      microseconds: 1,
      nanoseconds: 500
    }
  }
];

for (const { fractionalDigits, expected, duration } of durations) {
    const df = new Intl.DurationFormat("en", { seconds: "numeric", fractionalDigits });
    const result = df.format(duration);
    sameValue(result, expected);
}
