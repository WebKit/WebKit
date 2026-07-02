// Tests encodeURIComponent on non-ASCII text where every character expands to
// multiple %XY escape sequences.

const values = [];
for (let j = 0; j < 16; j++)
    values.push('検索クエリ' + j + ': 東京都渋谷区の天気予報と交通情報を教えてください \u{1F600}\u{1F680}');

let expected = 0;
for (let j = 0; j < 16; j++)
    expected += encodeURIComponent(values[j]).length;

const iterations = 100000;
let sum = 0;
for (let i = 0; i < iterations; i++)
    sum += encodeURIComponent(values[i & 15]).length;

if (sum !== expected * (iterations / 16))
    throw new Error("bad result: " + sum);
