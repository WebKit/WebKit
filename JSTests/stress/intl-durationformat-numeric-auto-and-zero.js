function assertSameValue(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected "${expected}" but got "${actual}"`)
}

const durations = [
  [{hours: 0, minutes: 0, seconds: 0}, "0"],
  [{hours: 0, minutes: 0, seconds: 1}, "0:00:01"],
  [{hours: 0, minutes: 1, seconds: 0}, "0:01"],
  [{hours: 0, minutes: 1, seconds: 1}, "0:01:01"],
  [{hours: 1, minutes: 0, seconds: 0}, "1"],
  [{hours: 1, minutes: 0, seconds: 1}, "1:00:01"],
  [{hours: 1, minutes: 1, seconds: 0}, "1:01"],
  [{hours: 1, minutes: 1, seconds: 1}, "1:01:01"],
  [{hours: 1, minutes: 0, seconds: 0, milliseconds: 1}, "1:00:00.001"],
  [{hours: 1, minutes: 0, seconds: 0, microseconds: 1}, "1:00:00.000001"],
  [{hours: 1, minutes: 0, seconds: 0, nanoseconds: 1}, "1:00:00.000000001"],
];

const df = new Intl.DurationFormat("en", { hours: "numeric", minutesDisplay: "auto", secondsDisplay: "auto" });

for (const [duration, expected] of durations) {
  assertSameValue(df.format(duration), expected);
}
