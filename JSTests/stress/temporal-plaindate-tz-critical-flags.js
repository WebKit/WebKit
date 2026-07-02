//@ requireOptions("--useTemporal=1")

const cases = [
    ["2000-05-02T00+00:00[!UTC]", "2000-05-02T00+00:00[!UTC]"],
    ["2000-05-02T00+00:00[!UTC]", "2000-05-02T00+00:00"],
    ["2000-05-02T00+02:00[!UTC]", "2000-05-02T00+02:00"],
    ["2000-05-02T00+09:00[!UTC]", "2000-05-02T00+09:00[Asia/Tokyo]"],
];

for (const [str1, str2] of cases) {
    const plainDate = Temporal.PlainDate.from(str1);
    if (!plainDate.equals(str2))
        throw new Error(`"${str1}" and "${str2}" should result in the same Temporal.PlainDate.`)
}
