//@ requireOptions("--useTemporal=1")

const cases = [
    ["1976-11-18T15:23+00:00[!UTC]", "1976-11-18T15:23+00:00[!UTC]"],
    ["1976-11-18T15:23+00:00[!UTC]", "1976-11-18T15:23+00:00"],
    ["1976-11-18T15:23+02:00[!UTC]", "1976-11-18T15:23+02:00"],
    ["1976-11-18T15:23+09:00[!UTC]", "1976-11-18T15:23+09:00[Asia/Tokyo]"],
];

for (const [str1, str2] of cases) {
    const plainDateTime = Temporal.PlainDateTime.from(str1);
    if (!plainDateTime.equals(str2))
        throw new Error(`"${str1}" and "${str2}" should result in the same Temporal.PlainDateTime.`)
}
