function test() {
    const df = new Intl.DurationFormat("en");
    let length = 0;
    for (let i = 0; i < 1e4; ++i)
        length += df.formatToParts({ hours: 1, minutes: 30, seconds: i % 60 + 1 }).length;
    return length;
}

const result = test();
if (result !== 110000)
    throw new Error("Bad result: " + result);
