var rows = [];
for (var i = 0; i < 16; ++i) {
    var row = {};
    for (var j = 0; j < 12; ++j)
        row["field" + j] = i * 100 + j;
    rows.push(row);
}

function test(row)
{
    return Object.values(row);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(rows[i & 15]);
