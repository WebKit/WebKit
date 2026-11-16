function shouldBe(actual, expected, message)
{
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

function quote(string)
{
    let result = '"';
    for (let i = 0; i < string.length; ++i) {
        let code = string.charCodeAt(i);
        if (code >= 0xD800 && code <= 0xDBFF && i + 1 < string.length) {
            let next = string.charCodeAt(i + 1);
            if (next >= 0xDC00 && next <= 0xDFFF) {
                result += string[i] + string[i + 1];
                ++i;
                continue;
            }
        }
        if (code >= 0xD800 && code <= 0xDFFF)
            result += "\\u" + code.toString(16);
        else if (code === 0x0A)
            result += "\\n";
        else
            result += string[i];
    }
    return result + '"';
}

function test(string)
{
    let expected = quote(string);
    shouldBe(JSON.stringify(string), expected, "string");
    shouldBe(JSON.stringify([string, string]), `[${expected},${expected}]`, "array");
    shouldBe(JSON.stringify({ key: string }), `{"key":${expected}}`, "object");
    shouldBe(JSON.stringify({ key: string }, null, 1), `{\n "key": ${expected}\n}`, "object with space");
}

let lead = "\uD83D";
let trail = "\uDE00";
let units = [
    lead + trail,
    lead + trail + lead + trail,
    lead,
    trail,
    trail + lead,
    lead + lead + trail,
    lead + trail + trail,
    lead + "\u3042" + trail,
    "\uD800\uDC00",
    "\uDBFF\uDFFF",
    "\uD7FF\uE000",
    lead + trail + "\n",
];

for (let unit of units) {
    for (let prefix = 0; prefix <= 34; ++prefix) {
        for (let suffix = 0; suffix <= 17; ++suffix)
            test("\u3042".repeat(prefix) + unit + "\u3044".repeat(suffix));
    }
}

for (let length of [100, 3000]) {
    test((lead + trail).repeat(length));
    test(("\u3042" + lead + trail).repeat(length));
    test((lead + trail).repeat(length) + lead);
    test(trail + (lead + trail).repeat(length));
    test((lead + trail).repeat(length) + lead + "\u3042".repeat(length));
}
