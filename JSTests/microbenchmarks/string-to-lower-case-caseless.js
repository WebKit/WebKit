function test(string) {
    return string.toLowerCase();
}
noInline(test);

const strings = [
    "\u65e5\u672c\u8a9e",
    "\u65e5\u672c\u8a9e\u306e\u6587\u5b57\u5217\u3092\u5c0f\u6587\u5b57\u306b\u5909\u63db\u3059\u308b",
    "JavaScript \u306e\u6587\u5b57\u5217\u3001String \u3092\u5c0f\u6587\u5b57\u306b\uff01",
    "\ud55c\uad6d\uc5b4 \ubb38\uc790\uc5f4",
    "it\u2019s already lowercase",
];

for (let i = 0; i < 1e6; ++i)
    test(strings[i % strings.length]);
