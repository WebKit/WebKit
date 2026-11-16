function test(id, str)
{
    return JSON.stringify({ id, excerpt: str.slice(0, 120), date: str.slice(130, 140) });
}
noInline(test);

const strs = [];
for (let k = 0; k < 16; k++)
    strs.push(("the quick brown fox jumps over the lazy dog " + k + " ").repeat(8));

let result;
for (let i = 0; i < 1000000; ++i)
    result = test(i & 15, strs[i & 15]);

if (test(1, "abcdefghij".repeat(20)) !== `{"id":1,"excerpt":"${"abcdefghij".repeat(12)}","date":"abcdefghij"}`)
    throw new Error("bad result: " + test(1, "abcdefghij".repeat(20)));
