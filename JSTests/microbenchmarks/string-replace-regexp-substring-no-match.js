function test(str, re, replacer)
{
    return str.slice(0, 120).replace(re, replacer);
}
noInline(test);

const strs = [];
for (let k = 0; k < 16; k++)
    strs.push(("the quick brown fox jumps over the lazy dog " + k + " ").repeat(8));

const re = /[&<>"']/g;
const replacer = (character) => `&#${character.charCodeAt(0)};`;

let result;
for (let i = 0; i < 1000000; ++i)
    result = test(strs[i & 15], re, replacer);

if (test("a < b ".repeat(30), re, replacer) !== "a &#60; b ".repeat(20))
    throw new Error("bad result: " + test("a < b ".repeat(30), re, replacer));
