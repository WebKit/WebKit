function test(str, count)
{
    return str.repeat(count);
}
noInline(test);

let a = "a".repeat(50000);
let b = "b".repeat(50000);
for (let i = 0; i < 1e4; ++i)
    test(a + b, 2);
