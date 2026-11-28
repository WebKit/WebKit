function test(record) {
    return Number(record.id) + (+record.quantity) * Number(record.page);
}
noInline(test);

var source = [];
for (var i = 0; i < 16; ++i)
    source.push({ id: String(40000 + i * 137), quantity: String(10 + i), page: String(100 + i * 3) });
var records = JSON.parse(JSON.stringify(source));

var result = 0;
for (var i = 0; i < 1e6; ++i)
    result = (result + test(records[i & 15])) | 0;
if (result !== 285327040)
    throw new Error(`bad result ${result}`);
