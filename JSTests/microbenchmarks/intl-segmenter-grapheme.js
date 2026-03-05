function test() {
    const segmenter = new Intl.Segmenter("en", { granularity: "grapheme" });
    const testStrings = [
        "Hello, World!",
        "こんにちは世界",
        "👨‍👩‍👧‍👦🏳️‍🌈",
        "The quick brown fox jumps over the lazy dog.",
        "日本語テキストのセグメンテーション",
        "Mixed: 日本語 and English テキスト",
        "Emoji: 😀😃😄😁😆😅🤣😂🙂🙃",
    ];

    let count = 0;
    for (let i = 0; i < 1e4; i++) {
        for (const str of testStrings) {
            const segments = segmenter.segment(str);
            for (const segment of segments) {
                count++;
            }
        }
    }
    return count;
}

const result = test();
if (result !== 1270000)
    throw new Error("Bad result: " + result);
