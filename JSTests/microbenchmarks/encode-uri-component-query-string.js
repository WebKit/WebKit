// Tests encodeURIComponent on query-string payloads full of spaces, quotes,
// ampersands and percent signs, mimicking building fetch URLs from JSON values.

const values = [];
for (let j = 0; j < 16; j++)
    values.push('user query ' + j + ': {"name":"Alice & Bob","tags":["a b","c=d"],"note":"100% legit, really?","page":' + j + '}');

let expected = 0;
for (let j = 0; j < 16; j++)
    expected += encodeURIComponent(values[j]).length;

const iterations = 200000;
let sum = 0;
for (let i = 0; i < iterations; i++)
    sum += encodeURIComponent(values[i & 15]).length;

if (sum !== expected * (iterations / 16))
    throw new Error("bad result: " + sum);
