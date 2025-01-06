//@ requireOptions("--useTemporal=1")

const cases = [
    ["1970-01-01T00:00:00Z[!UTC]", "1970-01-01T00:00:00Z[!UTC]"],
    ["1970-01-01T00:00:00Z[!UTC]", "1970-01-01T00:00:00Z"],
    ["1970-01-01T00:00:00+02:00[!UTC]", "1970-01-01T00:00:00+02:00"],
    ["1970-01-01T00:00:00+09:00[!UTC]", "1970-01-01T00:00:00+09:00[Asia/Tokyo]"]
];

for (const [str1, str2] of cases) {
    const instant = Temporal.Instant.from(str1);
    if (!instant.equals(str2))
        throw new Error(`"${str1}" and "${str2}" should result in the same Temporal.Instant.`)
}
