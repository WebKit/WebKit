// CSV-style row parsing where each subject string is built at runtime (not an atom),
// so String.prototype.split must not atomize the parts nor consult the split cache.

const words = ["alpha", "bravo", "charlie", "delta", "echo", "foxtrot", "golf", "hotel"];

function parseRow(row)
{
    return row.split(",");
}
noInline(parseRow);

let count = 0;
let seed = 1;
for (let i = 0; i < 3e5; ++i) {
    seed = (seed * 1103515245 + 12345) >>> 0;
    const row = words[seed & 7] + i + "," + words[(seed >>> 3) & 7] + (i + 1) + "," + words[(seed >>> 6) & 7] + (i + 2) + "," + words[(seed >>> 9) & 7] + (i + 3);
    const parts = parseRow(row);
    count += parts.length + parts[2].length;
}
if (count !== 4379333)
    throw new Error("bad count: " + count);
