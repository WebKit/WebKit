function test(i)
{
    return ("" + (1000000 + (i & 7))).length;
}
noInline(test);

let acc = 0;
for (let i = 0; i < 5e6; ++i)
    acc += test(i);
if (acc !== 35000000)
    throw new Error("bad result: " + acc);
