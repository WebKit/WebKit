
function shouldBe(actual, expected) {
    if (actual !== expected && !(actual !== null && typeof(expected) === "string" && actual.toString() == expected))
        throw new Error('expected: ' + expected + ', bad value: ' + actual);
}

function shouldThrowInvalidGroupSpecifierName(func) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('not thrown');
    shouldBe(String(error), "SyntaxError: Invalid regular expression: invalid group specifier name");
}

/*
 Valid ID_Start / ID_Continue Unicode characters

 𝑓  \u{1d453}  \ud835 \udc53
 𝑜  \u{1d45c}  \ud835 \udc5c
 𝑥  \u{id465}  \ud835 \udc65

 𝓓  \u{1d4d3}  \ud835 \udcd3
 𝓸  \u{1d4f8}  \ud835 \udcf8
 𝓰  \u{1d4f0}  \ud835 \udcf0

 𝓑  \u{1d4d1}  \ud835 \udcd1
 𝓻  \u{1d4fb}  \ud835 \udcfb
 𝓸  \u{1d4f8}  \ud835 \udcf8
 𝔀  \u{1d500}  \ud835 \udd00
 𝓷  \u{1d4f7}  \ud835 \udcf7

 𝖰  \u{1d5b0}  \ud835 \uddb0
 𝖡  \u{1d5a1}  \ud835 \udda1
 𝖥  \u{1d5a5}  \ud835 \udda5

 (fox) 狸  \u{72f8}  \u72f8
 (dog) 狗  \u{72d7}  \u72d7  

 Valid ID_Continue Unicode characters (Can't be first identifier character.)

 𝟚  \u{1d7da}  \ud835 \udfda

Invalid ID_Start / ID_Continue

 (fox face emoji) 🦊  \u{1f98a}  \ud83e \udd8a
 (dog emoji)  🐕  \u{1f415}  \ud83d \udc15
*/

var string = "The quick brown fox jumped over the lazy dog's back";
var string2 = "It is a dog eat dog world.";

let match = null;

// Try valid names

// Unicode RegExp's
shouldBe(string.match(/(?<animal>fox|dog)/u).groups.animal, "fox");
shouldBe(string.match(/(?<the2>the)/u).groups.the2, "the");

match = string.match(/(?<𝑓𝑜𝑥>fox).*(?<𝓓𝓸𝓰>dog)/u);
shouldBe(match.groups.𝑓𝑜𝑥, "fox");
shouldBe(match.groups.𝓓𝓸𝓰, "dog");
shouldBe(match[1], "fox");
shouldBe(match[2], "dog");

match = string.match(/(?<狸>fox).*(?<狗>dog)/u);
shouldBe(match.groups.狸, "fox");
shouldBe(match.groups.狗, "dog");
shouldBe(match[1], "fox");
shouldBe(match[2], "dog");

shouldBe(string.match(/(?<𝓑𝓻𝓸𝔀𝓷>brown)/u).groups.𝓑𝓻𝓸𝔀𝓷, "brown");
shouldBe(string.match(/(?<𝓑𝓻𝓸𝔀𝓷>brown)/u).groups.\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}, "brown");
shouldBe(string.match(/(?<\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}>brown)/u).groups.𝓑𝓻𝓸𝔀𝓷, "brown");
shouldBe(string.match(/(?<\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}>brown)/u).groups.\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}, "brown");
shouldBe(string.match(/(?<\ud835\udcd1\ud835\udcfb\ud835\udcf8\ud835\udd00\ud835\udcf7>brown)/u).groups.𝓑𝓻𝓸𝔀𝓷, "brown");
shouldBe(string.match(/(?<\ud835\udcd1\ud835\udcfb\ud835\udcf8\ud835\udd00\ud835\udcf7>brown)/u).groups.\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}, "brown");

shouldBe(string.match(/(?<𝖰𝖡𝖥>q\w*\W\w*\W\w*)/u).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<𝖰𝖡\u{1d5a5}>q\w*\W\w*\W\w*)/u).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<𝖰\u{1d5a1}𝖥>q\w*\W\w*\W\w*)/u).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<𝖰\u{1d5a1}\u{1d5a5}>q\w*\W\w*\W\w*)/u).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<\u{1d5b0}𝖡𝖥>q\w*\W\w*\W\w*)/u).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<\u{1d5b0}𝖡\u{1d5a5}>q\w*\W\w*\W\w*)/u).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<\u{1d5b0}\u{1d5a1}𝖥>q\w*\W\w*\W\w*)/u).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<\u{1d5b0}\u{1d5a1}\u{1d5a5}>q\w*\W\w*\W\w*)/u).groups.𝖰𝖡𝖥, "quick brown fox");

shouldBe(string.match(/(?<the𝟚>the)/u).groups.the𝟚, "the");
shouldBe(string.match(/(?<the\u{1d7da}>the)/u).groups.the𝟚, "the");
shouldBe(string.match(/(?<the\ud835\udfda>the)/u).groups.the𝟚, "the");

match = string2.match(/(?<dog>dog)(.*?)(\k<dog>)/u);
shouldBe(match.groups.dog, "dog");
shouldBe(match[1], "dog");
shouldBe(match[2], " eat ");
shouldBe(match[3], "dog");

match = string2.match(/(?<𝓓𝓸𝓰>dog)(.*?)(\k<𝓓𝓸𝓰>)/u);
shouldBe(match.groups.𝓓𝓸𝓰, "dog");
shouldBe(match[1], "dog");
shouldBe(match[2], " eat ");
shouldBe(match[3], "dog");

match = string2.match(/(?<狗>dog)(.*?)(\k<狗>)/u);
shouldBe(match.groups.狗, "dog");
shouldBe(match[1], "dog");
shouldBe(match[2], " eat ");
shouldBe(match[3], "dog");

// Non-unicode RegExp's
shouldBe(string.match(/(?<animal>fox|dog)/).groups.animal, "fox");

match = string.match(/(?<𝑓𝑜𝑥>fox).*(?<𝓓𝓸𝓰>dog)/);
shouldBe(match.groups.𝑓𝑜𝑥, "fox");
shouldBe(match.groups.𝓓𝓸𝓰, "dog");
shouldBe(match[1], "fox");
shouldBe(match[2], "dog");

match = string.match(/(?<狸>fox).*(?<狗>dog)/);
shouldBe(match.groups.狸, "fox");
shouldBe(match.groups.狗, "dog");
shouldBe(match[1], "fox");
shouldBe(match[2], "dog");

shouldBe(string.match(/(?<𝓑𝓻𝓸𝔀𝓷>brown)/).groups.𝓑𝓻𝓸𝔀𝓷, "brown");
shouldBe(string.match(/(?<𝓑𝓻𝓸𝔀𝓷>brown)/).groups.\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}, "brown");
shouldBe(string.match(/(?<\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}>brown)/).groups.𝓑𝓻𝓸𝔀𝓷, "brown");
shouldBe(string.match(/(?<\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}>brown)/).groups.\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}, "brown");
shouldBe(string.match(/(?<\ud835\udcd1\ud835\udcfb\ud835\udcf8\ud835\udd00\ud835\udcf7>brown)/).groups.𝓑𝓻𝓸𝔀𝓷, "brown");
shouldBe(string.match(/(?<\ud835\udcd1\ud835\udcfb\ud835\udcf8\ud835\udd00\ud835\udcf7>brown)/).groups.\u{1d4d1}\u{1d4fb}\u{1d4f8}\u{1d500}\u{1d4f7}, "brown");

shouldBe(string.match(/(?<𝖰𝖡𝖥>q\w*\W\w*\W\w*)/).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<𝖰𝖡\u{1d5a5}>q\w*\W\w*\W\w*)/).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<𝖰\u{1d5a1}𝖥>q\w*\W\w*\W\w*)/).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<𝖰\u{1d5a1}\u{1d5a5}>q\w*\W\w*\W\w*)/).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<\u{1d5b0}𝖡𝖥>q\w*\W\w*\W\w*)/).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<\u{1d5b0}𝖡\u{1d5a5}>q\w*\W\w*\W\w*)/).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<\u{1d5b0}\u{1d5a1}𝖥>q\w*\W\w*\W\w*)/).groups.𝖰𝖡𝖥, "quick brown fox");
shouldBe(string.match(/(?<\u{1d5b0}\u{1d5a1}\u{1d5a5}>q\w*\W\w*\W\w*)/).groups.𝖰𝖡𝖥, "quick brown fox");

shouldBe(string.match(/(?<the𝟚>the)/).groups.the𝟚, "the");
shouldBe(string.match(/(?<the\u{1d7da}>the)/).groups.the𝟚, "the");
shouldBe(string.match(/(?<the\ud835\udfda>the)/).groups.the𝟚, "the");

match = string2.match(/(?<dog>dog)(.*?)(\k<dog>)/);
shouldBe(match.groups.dog, "dog");
shouldBe(match[1], "dog");
shouldBe(match[2], " eat ");
shouldBe(match[3], "dog");

match = string2.match(/(?<𝓓𝓸𝓰>dog)(.*?)(\k<𝓓𝓸𝓰>)/);
shouldBe(match.groups.𝓓𝓸𝓰, "dog");
shouldBe(match[1], "dog");
shouldBe(match[2], " eat ");
shouldBe(match[3], "dog");

match = string2.match(/(?<狗>dog)(.*?)(\k<狗>)/);
shouldBe(match.groups.狗, "dog");
shouldBe(match[1], "dog");
shouldBe(match[2], " eat ");
shouldBe(match[3], "dog");

// Invalid identifiers

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<🦊>fox)");
});

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<\u{1f98a}>fox)");
});

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<\ud83e\udd8a>fox)");
});

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<🐕>dog)");
});

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<\u{1f415}>dog)");
});

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<\ud83d \udc15>dog)");
});

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<𝟚the>the)");
});

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<\u{1d7da}the>the)");
});

shouldThrowInvalidGroupSpecifierName(function() {
    return new RegExp("(?<\ud835\udfdathe>the)");
});
