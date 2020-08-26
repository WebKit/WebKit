//@ skip if $architecture == "mips" # Due to ICU version. MIPS maintainer can look into the results and update.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldNotThrow(func) {
    func();
}

{
    shouldBe(Intl.Segmenter instanceof Function, true);
    shouldBe(Intl.Segmenter.length, 0);
    shouldBe(Object.getOwnPropertyDescriptor(Intl.Segmenter, 'prototype').writable, false);
    shouldBe(Object.getOwnPropertyDescriptor(Intl.Segmenter, 'prototype').enumerable, false);
    shouldBe(Object.getOwnPropertyDescriptor(Intl.Segmenter, 'prototype').configurable, false);
    let segmenter = new Intl.Segmenter("fr");
    shouldBe(JSON.stringify(segmenter.resolvedOptions()), `{"locale":"fr","granularity":"grapheme"}`);
    shouldBe(segmenter.toString(), `[object Intl.Segmenter]`);
    shouldNotThrow(() => new Intl.Segmenter());
}

{
    let segmenter = new Intl.Segmenter("fr", {granularity: "word"});

    let input = "Moi?  N'est-ce pas.";
    let segments = segmenter.segment(input);

    let results = [
        [ 0, 3, "Moi", true ],
        [ 3, 4, "?", false ],
        [ 4, 6, "  ", false ],
        [ 6, 11, "N'est", true ],
        [ 11, 12, "-", false ],
        [ 12, 14, "ce", true ],
        [ 14, 15, " ", false ],
        [ 15, 18, "pas", true ],
        [ 18, 19, ".", false ],
    ];
    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let result = results[cursor++];
        shouldBe(result[0], index);
        shouldBe(result[1], index + segment.length);
        shouldBe(result[2], segment);
        shouldBe(result[3], isWordLike);
    }

    shouldBe(JSON.stringify(Intl.Segmenter.supportedLocalesOf('fr')), `["fr"]`);
    shouldBe(JSON.stringify(segmenter.resolvedOptions()), `{"locale":"fr","granularity":"word"}`);
}

{
    let segmenter = new Intl.Segmenter("fr", {granularity: "grapheme"});

    let input = "Moi?  N'est-ce pas.";
    let segments = segmenter.segment(input);

    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let current = cursor++;
        shouldBe(segment, input[current]);
        shouldBe(index, current);
        shouldBe(isWordLike, undefined);
    }

    shouldBe(JSON.stringify(segmenter.resolvedOptions()), `{"locale":"fr","granularity":"grapheme"}`);
}

{
    let segmenter = new Intl.Segmenter("en", {granularity: "sentence"});

    let input = "Performance is a top priority for WebKit. We adhere to a simple directive for all work we do on WebKit: The way to make a program faster is to never let it get slower.";
    let segments = segmenter.segment(input);

    let results = [
        [ 0, 42, "Performance is a top priority for WebKit. ", undefined ],
        [ 42, 167, "We adhere to a simple directive for all work we do on WebKit: The way to make a program faster is to never let it get slower.", undefined ],
    ];
    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let result = results[cursor++];
        shouldBe(result[0], index);
        shouldBe(result[1], index + segment.length);
        shouldBe(result[2], segment);
        shouldBe(result[3], isWordLike);
    }

    shouldBe(JSON.stringify(segmenter.resolvedOptions()), `{"locale":"en","granularity":"sentence"}`);
}

// languages without spaces.
{
    let segmenter = new Intl.Segmenter("ja", {granularity: "word"});

    // https://en.wikipedia.org/wiki/I_Am_a_Cat
    let input = "吾輩は猫である。名前はまだ無い。どこで生れたかとんと見当がつかぬ。";
    let segments = segmenter.segment(input);

    let results = [
        [ 0, 2, "吾輩", true ],
        [ 2, 3, "は", true ],
        [ 3, 4, "猫", true ],
        [ 4, 5, "で", true ],
        [ 5, 7, "ある", true ],
        [ 7, 8, "。", false ],
        [ 8, 10, "名前", true ],
        [ 10, 11, "は", true ],
        [ 11, 13, "まだ", true ],
        [ 13, 15, "無い", true ],
        [ 15, 16, "。", false ],
        [ 16, 18, "どこ", true ],
        [ 18, 19, "で", true ],
        [ 19, 21, "生れ", true ],
        [ 21, 23, "たか", true ],
        [ 23, 26, "とんと", true ],
        [ 26, 28, "見当", true ],
        [ 28, 29, "が", true ],
        [ 29, 30, "つ", true ],
        [ 30, 31, "か", true ],
        [ 31, 32, "ぬ", true ],
        [ 32, 33, "。", false ],
    ];
    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let result = results[cursor++];
        shouldBe(result[0], index);
        shouldBe(result[1], index + segment.length);
        shouldBe(result[2], segment);
        shouldBe(result[3], isWordLike);
    }
}

{
    let segmenter = new Intl.Segmenter("ja", {granularity: "grapheme"});

    // https://en.wikipedia.org/wiki/I_Am_a_Cat
    let input = "吾輩は猫である。名前はまだ無い。どこで生れたかとんと見当がつかぬ。";
    let segments = segmenter.segment(input);

    let results = [
        [ 0, 1, "吾", undefined ],
        [ 1, 2, "輩", undefined ],
        [ 2, 3, "は", undefined ],
        [ 3, 4, "猫", undefined ],
        [ 4, 5, "で", undefined ],
        [ 5, 6, "あ", undefined ],
        [ 6, 7, "る", undefined ],
        [ 7, 8, "。", undefined ],
        [ 8, 9, "名", undefined ],
        [ 9, 10, "前", undefined ],
        [ 10, 11, "は", undefined ],
        [ 11, 12, "ま", undefined ],
        [ 12, 13, "だ", undefined ],
        [ 13, 14, "無", undefined ],
        [ 14, 15, "い", undefined ],
        [ 15, 16, "。", undefined ],
        [ 16, 17, "ど", undefined ],
        [ 17, 18, "こ", undefined ],
        [ 18, 19, "で", undefined ],
        [ 19, 20, "生", undefined ],
        [ 20, 21, "れ", undefined ],
        [ 21, 22, "た", undefined ],
        [ 22, 23, "か", undefined ],
        [ 23, 24, "と", undefined ],
        [ 24, 25, "ん", undefined ],
        [ 25, 26, "と", undefined ],
        [ 26, 27, "見", undefined ],
        [ 27, 28, "当", undefined ],
        [ 28, 29, "が", undefined ],
        [ 29, 30, "つ", undefined ],
        [ 30, 31, "か", undefined ],
        [ 31, 32, "ぬ", undefined ],
        [ 32, 33, "。", undefined ],
    ];
    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let result = results[cursor++];
        shouldBe(result[0], index);
        shouldBe(result[1], index + segment.length);
        shouldBe(result[2], segment);
        shouldBe(result[3], isWordLike);
    }
}

{
    let segmenter = new Intl.Segmenter("ja", {granularity: "sentence"});

    // https://en.wikipedia.org/wiki/I_Am_a_Cat
    let input = "吾輩は猫である。名前はまだ無い。どこで生れたかとんと見当がつかぬ。";
    let segments = segmenter.segment(input);

    let results = [
        [ 0, 8, "吾輩は猫である。", undefined ],
        [ 8, 16, "名前はまだ無い。", undefined ],
        [ 16, 33, "どこで生れたかとんと見当がつかぬ。", undefined ],
    ];
    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let result = results[cursor++];
        shouldBe(result[0], index);
        shouldBe(result[1], index + segment.length);
        shouldBe(result[2], segment);
        shouldBe(result[3], isWordLike);
    }
}

// Surrogate pairs.
{
    let segmenter = new Intl.Segmenter("ja", {granularity: "grapheme"});
    let input = "𠮷野家";
    let segments = segmenter.segment(input);

    let results = [
        [ 0, 2, "𠮷", undefined ],
        [ 2, 3, "野", undefined ],
        [ 3, 4, "家", undefined ],
    ];
    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let result = results[cursor++];
        shouldBe(result[0], index);
        shouldBe(result[1], index + segment.length);
        shouldBe(result[2], segment);
        shouldBe(result[3], isWordLike);
    }
}

{
    let segmenter = new Intl.Segmenter("ja", {granularity: "word"});
    let input = "𠮷野家";
    let segments = segmenter.segment(input);

    let results = [
        [ 0, 2, "𠮷", true ],
        [ 2, 4, "野家", true ],
    ];
    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let result = results[cursor++];
        shouldBe(result[0], index);
        shouldBe(result[1], index + segment.length);
        shouldBe(result[2], segment);
        shouldBe(result[3], isWordLike);
    }
}

{
    let segmenter = new Intl.Segmenter("ja", {granularity: "sentence"});
    let input = "𠮷野家";
    let segments = segmenter.segment(input);

    let results = [
        [ 0, 4, "𠮷野家", undefined ],
    ];
    let cursor = 0;
    for (let {segment, index, isWordLike} of segments) {
        let result = results[cursor++];
        shouldBe(result[0], index);
        shouldBe(result[1], index + segment.length);
        shouldBe(result[2], segment);
        shouldBe(result[3], isWordLike);
    }
}

{
    let segmenter = new Intl.Segmenter("ja", {granularity: "grapheme"});
    let input = "𠮷野家";
    let segments = segmenter.segment(input);

    shouldBe(JSON.stringify(segments.containing(0)), `{"segment":"𠮷","index":0,"input":"𠮷野家"}`);
    shouldBe(JSON.stringify(segments.containing(1)), `{"segment":"𠮷","index":0,"input":"𠮷野家"}`);
    shouldBe(JSON.stringify(segments.containing(2)), `{"segment":"野","index":2,"input":"𠮷野家"}`);
    shouldBe(JSON.stringify(segments.containing(3)), `{"segment":"家","index":3,"input":"𠮷野家"}`);
    shouldBe(JSON.stringify(segments.containing(4)), undefined);
}

{
    // ┃0 1 2 3 4 5┃6┃7┃8┃9
    // ┃A l l o n s┃-┃y┃!┃
    let input = "Allons-y!";

    let segmenter = new Intl.Segmenter("fr", {granularity: "word"});
    let segments = segmenter.segment(input);
    let current = undefined;

    current = segments.containing(0);
    shouldBe(JSON.stringify(current), `{"segment":"Allons","index":0,"input":"Allons-y!","isWordLike":true}`);

    current = segments.containing(5);
    shouldBe(JSON.stringify(current), `{"segment":"Allons","index":0,"input":"Allons-y!","isWordLike":true}`);

    current = segments.containing(6);
    shouldBe(JSON.stringify(current), `{"segment":"-","index":6,"input":"Allons-y!","isWordLike":false}`);

    current = segments.containing(current.index + current.segment.length);
    shouldBe(JSON.stringify(current), `{"segment":"y","index":7,"input":"Allons-y!","isWordLike":true}`);

    current = segments.containing(current.index + current.segment.length);
    shouldBe(JSON.stringify(current), `{"segment":"!","index":8,"input":"Allons-y!","isWordLike":false}`);

    current = segments.containing(current.index + current.segment.length);
    shouldBe(JSON.stringify(current), undefined);
    // → undefined
}
{
    let input = "";

    let segmenter = new Intl.Segmenter("fr", {granularity: "word"});
    let segments = segmenter.segment(input);

    shouldBe(JSON.stringify(segments.containing(0)), undefined);
    let results = Array.from(segments[Symbol.iterator]());
    shouldBe(results.length, 0);
}
{
    let input = " ";

    let segmenter = new Intl.Segmenter("fr", {granularity: "word"});
    let segments = segmenter.segment(input);

    shouldBe(JSON.stringify(segments.containing(0)), `{"segment":" ","index":0,"input":" ","isWordLike":false}`);
    shouldBe(JSON.stringify(segments.containing(1)), undefined);
    shouldBe(JSON.stringify(segments.containing(2)), undefined);
    let results = Array.from(segments[Symbol.iterator]());
    shouldBe(results.length, 1);
    let {segment, index, isWordLike} = results[0];
    shouldBe(0, index);
    shouldBe(1, index + segment.length);
    shouldBe(" ", segment);
    shouldBe(false, isWordLike);
}
