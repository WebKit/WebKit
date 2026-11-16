function test(string, re)
{
    return string.replace(re, "$2:$1");
}
noInline(test);

const parts = [];
for (let i = 0; i < 200; ++i)
    parts.push("name" + i + "=value" + i);
const text = parts.join("&");
const re = /(\w+)=(\w+)/g;

let result;
for (let i = 0; i < 20000; ++i)
    result = test(text, re);

if (!result.startsWith("value0:name0&value1:name1"))
    throw new Error("bad result: " + result.substring(0, 30));
