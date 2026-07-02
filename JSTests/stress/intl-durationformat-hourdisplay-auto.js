function shouldBe(a, b) {
    if (a !== b) {
        throw new Error(`Expected ${b} but got ${a}`);
    }
}

const df = new Intl.DurationFormat('en-US', {
  style: "digital",
  hoursDisplay: "auto",
})


shouldBe(df.format({hours: 0, minutes: 1, seconds: 2}), "01:02");
shouldBe(df.format({hours: 0, minutes: 0, seconds: 2}), "00:02");
shouldBe(df.format({hours: 0, minutes: 0, seconds: 0}), "00:00");
shouldBe(df.format({hours: 1, minutes: 0, seconds: 0}), "1:00:00");
shouldBe(df.format({ days: 1, hours: 0, minutes: 1, seconds: 2 }), "1 day, 01:02");
shouldBe(df.format({years: 1, days: 3, hours: 1, minutes: 2, seconds: 3}), "1 yr, 3 days, 1:02:03");
shouldBe(df.format({years: 1, days: 3, hours: 0, minutes: 2, seconds: 3}), "1 yr, 3 days, 02:03");
shouldBe(df.format({years: 0, days: 3, hours: 1, minutes: 2, seconds: 3}), "3 days, 1:02:03");
shouldBe(df.format({years: 0, days: 0, hours: 0, minutes: 2, seconds: 3}), "02:03");
