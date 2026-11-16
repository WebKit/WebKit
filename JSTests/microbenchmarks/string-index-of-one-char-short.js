function indexOfColon(string) {
    return string.indexOf(":");
}
noInline(indexOfColon);

function includesSlash(string) {
    return string.includes("/");
}
noInline(includesSlash);

var strings = [
    "content-type: application/json",
    "authorization",
    "src/utils/permissions/filesystem.ts",
    "node:fs",
    "x-request-id: 0123456789abcdef0123456789abcdef",
    "README.md",
    "https://example.com/path/to/resource",
    "a-long-identifier-without-any-separator-in-it",
];

var result = 0;
for (var i = 0; i < 1e6; ++i) {
    var string = strings[i & 7];
    result += indexOfColon(string);
    if (includesSlash(string))
        result++;
}
if (result !== 4000000)
    throw new Error("bad result: " + result);
