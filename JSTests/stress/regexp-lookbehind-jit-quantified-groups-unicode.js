//@ skip if not $jitTests
//@ runDefault
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected " + expected);
}

function stringify(match) {
    if (match === null)
        return "null";
    return "[" + Array.from(match, (x) => x === undefined ? "undefined" : JSON.stringify(x)).join(",") + "]";
}

function stringifyIndices(match) {
    if (match === null)
        return "null";
    return JSON.stringify(Array.from(match.indices, (x) => x === undefined ? null : x));
}

shouldBe(stringify(/(?<=(?:\u{1F600}b)+)c/u.exec("\ud83d\ude00b\ud83d\ude00bc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:\u{1F600}b)+)c/u.exec("bc")), "null");
shouldBe(stringify(/(?<=(?:\u{1F600}b)+)c/v.exec("\ud83d\ude00b\ud83d\ude00bc")), "[\"c\"]");
shouldBe(stringify(/(?<=(\u{1F600}b)+)c/u.exec("\ud83d\ude00b\ud83d\ude00bc")), "[\"c\",\"\ud83d\ude00b\"]");
shouldBe(stringify(/(?<=(\u{1F600})+)b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(\u{1F600})*)b/u.exec("b")), "[\"b\",undefined]");
shouldBe(stringify(/(?<=^(\u{1F600})+)b/u.exec("\ud83d\ude00\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600})+)b/u.exec("a\ud83d\ude00\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=^(\u{1F600})+?)b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(?:\u{1F600})+?)b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:\u{1F600})*?)b/u.exec("\ud83d\ude00b")), "[\"b\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F601})+)b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F601})+?)b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F601}\u{1F601})+)b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude01b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F601}\u{1F601})+)b/u.exec("\ud83d\ude00\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F600}\u{1F601})+)b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\ud83d\ude01\"]");
shouldBe(stringify(/(?<=^(\u{1F600}\u{1F601}|\u{1F600})+)b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\ud83d\ude01\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F600}\u{1F601})+)b/v.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00\ud83d\ude00\ud83d\ude01b")), "[\"b\",\"\ud83d\ude00\ud83d\ude01\"]");
shouldBe(stringify(/(?<=(?:\u{1F600}b){2})c/u.exec("\ud83d\ude00b\ud83d\ude00bc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:\u{1F600}b){2})c/u.exec("\ud83d\ude00bc")), "null");
shouldBe(stringify(/(?<=(\u{1F600}){3})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(\u{1F600}){3})b/u.exec("\ud83d\ude00\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=^(\u{1F600}){3})b/u.exec("a\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(\u{1F600}|\u{1F601}){2})b/u.exec("\ud83d\ude00\ud83d\ude01b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(\u{1F600}|\u{1F601}){2})b/u.exec("\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude01\"]");
shouldBe(stringify(/(?<=(\u{1F600}|\u{1F601}){2})b/u.exec("\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F600}\u{1F601}){2})b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\ud83d\ude01\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F600}\u{1F601}){2})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600}\u{1F601}|\u{1F600}){2})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F600}\u{1F601}){2,3})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F600}\u{1F601}){2,3}?)b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F600}\u{1F601}){1,2})b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\ud83d\ude01\"]");
shouldBe(stringify(/(?<=^(\u{1F600}|\u{1F600}\u{1F601}){1,2})b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=^(?:\u{1F600}+){2})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "[\"b\"]");
shouldBe(stringify(/(?<=^(\u{1F600}+?){2})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "[\"b\",\"\ud83d\ude00\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(\u{1F600}*){2})b/u.exec("\ud83d\ude00\ud83d\ude00b")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(?:[\u{1F600}-\u{1F64F}]b)+)c/u.exec("\ud83d\ude00b\ud83d\ude01bc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:[\u{1F600}-\u{1F64F}]b){2})c/u.exec("\ud83d\ude00b\ud83d\ude01bc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:[\u{1F600}-\u{1F64F}]b){2})c/u.exec("\ud83d\ude00bc")), "null");
shouldBe(stringify(/(?<=^(?:\p{Lu}){2,3})b/u.exec("\ud801\udc00A\ud801\udc00b")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:\p{Lu}){2,3})b/u.exec("A\ud801\udc00\ud801\udc28b")), "null");
shouldBe(stringify(/(?<=^(\p{Lu}|\u{10428}){2,3})b/u.exec("A\ud801\udc00\ud801\udc28b")), "[\"b\",\"A\"]");
shouldBe(stringify(/(?<=^(\p{Lu}|\u{10428}){2,3}?)b/u.exec("A\ud801\udc00\ud801\udc28b")), "[\"b\",\"A\"]");
shouldBe(stringify(/(?<=^(?:[^a]){2})b/u.exec("\ud83d\ude00\ud83d\ude00b")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:[^a]){2})b/u.exec("\ud83d\ude00ab")), "null");
shouldBe(stringify(/(?<=^(?:.){2})b/u.exec("\ud83d\ude00\ud83d\ude00b")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:.){2})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=^(?:.){2,3})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:.){2,3}?)b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "[\"b\"]");
shouldBe(stringify(/(?<=(?:\P{L}){2})b/v.exec("\ud83d\ude00\ud83d\ude01b")), "[\"b\"]");
shouldBe(stringify(/(?<=[\p{L}--[a-z]](?:\u{1F601}){0,2})b/v.exec("A\ud83d\ude01\ud83d\ude01b")), "[\"b\"]");
shouldBe(stringify(/(?<=[\p{L}--[a-z]](?:\u{1F601}){0,2})b/v.exec("a\ud83d\ude01\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(?:\u{10400}b)+)c/iu.exec("\ud801\udc28B\ud801\udc00bc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:\u{10400}b){2})c/iu.exec("\ud801\udc28B\ud801\udc00bc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:\u{10400}b){2})c/iu.exec("\ud801\udc28Bc")), "null");
shouldBe(stringify(/(?<=^(\u{10400}|k)+)c/iu.exec("\ud801\udc28K\u212a\ud801\udc00c")), "[\"c\",\"\ud801\udc28\"]");
shouldBe(stringify(/(?<=\b(?:\u{1F600}b)+)c/u.exec("x \ud83d\ude00b\ud83d\ude00bc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:^|,)(\u{1F600}|\u{1F601}){2})c/u.exec("x,\ud83d\ude00\ud83d\ude01c")), "[\"c\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(?:^|,)(\u{1F600}|\u{1F601}){2})c/u.exec("x\ud83d\ude00\ud83d\ude01c")), "null");
shouldBe(stringify(/(?<=^(\u{1F600})\1+)b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=^(?:(\u{1F600}|\u{1F601})\1)+)b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01\ud83d\ude01b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(?:(\u{1F600}|\u{1F601})\1)+)b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01b")), "[\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=^(?:(\u{1F600}|\u{1F601})\1){2})b/u.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(\u{1F600}|\u{1F601})+)b\1/u.exec("\ud83d\ude00\ud83d\ude01b\ud83d\ude01")), "null");
shouldBe(stringify(/(?<=(\u{1F600}|\u{1F601})+)b\1/u.exec("\ud83d\ude00\ud83d\ude01b\ud83d\ude00")), "[\"b\ud83d\ude00\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=y(y\u{1F600}|\u{1F600}){1,2}?c)d/u.exec("xy\ud83d\ude00cd")), "[\"d\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=y(y\u{1F600}|\u{1F600}){1,2}c)d/u.exec("xy\ud83d\ude00cd")), "[\"d\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=\u{1F601}(\u{1F601}\u{1F600}|\u{1F600}){1,2}?c)d/u.exec("x\ud83d\ude01\ud83d\ude00cd")), "[\"d\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<!(?:\u{1F600}b)+)c/u.exec("\ud83d\ude00b\ud83d\ude00bc")), "null");
shouldBe(stringify(/(?<!(?:\u{1F600}b)+)c/u.exec("xc")), "[\"c\"]");
shouldBe(stringify(/(?<!(\u{1F600}|\u{1F601}){2})c/u.exec("\ud83d\ude00\ud83d\ude01c")), "null");
shouldBe(stringify(/(?<!(\u{1F600}|\u{1F601}){2})c/u.exec("a\ud83d\ude01c")), "[\"c\",undefined]");

{
    let re = /(?<=(?:\u{1F600}b)+)c/gu;
    re.lastIndex = 0;
    let match = re.exec("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc");
    shouldBe(match === null ? "null" : match.index, 6);
    shouldBe(re.lastIndex, 7);
}
{
    let re = /(?<=(?:\u{1F600}b)+)c/gu;
    re.lastIndex = 1;
    let match = re.exec("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc");
    shouldBe(match === null ? "null" : match.index, 6);
    shouldBe(re.lastIndex, 7);
}
{
    let re = /(?<=(?:\u{1F600}b)+)c/gu;
    re.lastIndex = 3;
    let match = re.exec("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc");
    shouldBe(match === null ? "null" : match.index, 6);
    shouldBe(re.lastIndex, 7);
}
{
    let re = /(?<=(?:\u{1F600}b)+)c/gu;
    re.lastIndex = 6;
    let match = re.exec("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc");
    shouldBe(match === null ? "null" : match.index, 6);
    shouldBe(re.lastIndex, 7);
}
{
    let re = /(?<=(?:\u{1F600}b)+)c/yu;
    re.lastIndex = 0;
    let match = re.exec("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}
{
    let re = /(?<=(?:\u{1F600}b)+)c/yu;
    re.lastIndex = 6;
    let match = re.exec("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc");
    shouldBe(match === null ? "null" : match.index, 6);
    shouldBe(re.lastIndex, 7);
}
{
    let re = /(?<=(?:\u{1F600}b)+)c/yu;
    re.lastIndex = 7;
    let match = re.exec("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}
{
    let re = /(?<=(?:\u{1F600}b)+)c/yu;
    re.lastIndex = 10;
    let match = re.exec("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc");
    shouldBe(match === null ? "null" : match.index, 10);
    shouldBe(re.lastIndex, 11);
}
{
    let re = /(?<=(\u{1F600}|\u{1F600}\u{1F601}){2})c/gu;
    re.lastIndex = 0;
    let match = re.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01c\ud83d\ude00\ud83d\ude00c");
    shouldBe(match === null ? "null" : match.index, 6);
    shouldBe(re.lastIndex, 7);
}
{
    let re = /(?<=(\u{1F600}|\u{1F600}\u{1F601}){2})c/gu;
    re.lastIndex = 1;
    let match = re.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01c\ud83d\ude00\ud83d\ude00c");
    shouldBe(match === null ? "null" : match.index, 6);
    shouldBe(re.lastIndex, 7);
}
{
    let re = /(?<=(\u{1F600}|\u{1F600}\u{1F601}){2})c/gu;
    re.lastIndex = 8;
    let match = re.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01c\ud83d\ude00\ud83d\ude00c");
    shouldBe(match === null ? "null" : match.index, 11);
    shouldBe(re.lastIndex, 12);
}
{
    let re = /(?<=(\u{1F600}|\u{1F600}\u{1F601}){1,2}?)c/gu;
    re.lastIndex = 0;
    let match = re.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01c\ud83d\ude00\ud83d\ude00c");
    shouldBe(match === null ? "null" : match.index, 6);
    shouldBe(re.lastIndex, 7);
}
{
    let re = /(?<=(\u{1F600}|\u{1F600}\u{1F601}){1,2}?)c/gu;
    re.lastIndex = 7;
    let match = re.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01c\ud83d\ude00\ud83d\ude00c");
    shouldBe(match === null ? "null" : match.index, 11);
    shouldBe(re.lastIndex, 12);
}

shouldBe("\ud83d\ude00b\ud83d\ude00bc\ud83d\ude00bcc".replace(/(?<=(?:\u{1F600}b)+)c/gu, "-"), "\ud83d\ude00b\ud83d\ude00b-\ud83d\ude00b-c");
shouldBe("\ud83d\ude00\ud83d\ude01c\ud83d\ude01c".replace(/(?<=(\u{1F600}|\u{1F601})+)c/gu, "[$1]"), "\ud83d\ude00\ud83d\ude01[\ud83d\ude00]\ud83d\ude01[\ud83d\ude01]");
shouldBe("\ud83d\ude00\ud83d\ude00\ud83d\ude01c\ud83d\ude00\ud83d\ude00c".replace(/(?<=(\u{1F600}|\u{1F600}\u{1F601}){2})c/gu, "[$1]"), "\ud83d\ude00\ud83d\ude00\ud83d\ude01[\ud83d\ude00]\ud83d\ude00\ud83d\ude00[\ud83d\ude00]");

shouldBe(stringifyIndices(/(?<=(\u{1F600}b)+)c/du.exec("\ud83d\ude00b\ud83d\ude00bc")), "[[6,7],[0,3]]");
shouldBe(stringifyIndices(/(?<=(\u{1F600}|\u{1F601})+)b/du.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00b")), "[[6,7],[0,2]]");
shouldBe(stringifyIndices(/(?<=(\u{1F600}|\u{1F600}\u{1F601}){2})b/du.exec("\ud83d\ude00\ud83d\ude00\ud83d\ude01b")), "[[6,7],[0,2]]");
shouldBe(stringifyIndices(/(?<=y(y\u{1F600}|\u{1F600}){1,2}?c)d/du.exec("xy\ud83d\ude00cd")), "[[5,6],[2,4]]");
shouldBe(stringifyIndices(/(?<=^(\u{1F600}*){2})b/du.exec("\ud83d\ude00\ud83d\ude00b")), "[[4,5],[0,0]]");
