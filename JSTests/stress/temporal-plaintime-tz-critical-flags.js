//@ requireOptions("--useTemporal=1")

const cases = [
    ["12:34:56.987654321+00:00[!UTC]", "12:34:56.987654321+00:00[!UTC]"],
    ["12:34:56.987654321+00:00[!UTC]", "12:34:56.987654321+00:00"],
    ["12:34:56.987654321+02:00[!UTC]", "12:34:56.987654321+02:00"],
    ["12:34:56.987654321+09:00[!UTC]", "12:34:56.987654321+09:00[Asia/Tokyo]"],
];

for (const [str1, str2] of cases) {
    const plainTime = Temporal.PlainTime.from(str1);
    if (!plainTime.equals(str2))
        throw new Error(`"${str1}" and "${str2}" should result in the same Temporal.PlainTime.`)
}
