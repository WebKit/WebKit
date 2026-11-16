function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`bad value for ${label}: ${actual}, expected ${expected}`);
}

const other = createGlobalObject();

function realmOf(value) {
    const prototype = Object.getPrototypeOf(value);
    if (prototype === Array.prototype)
        return "main";
    if (prototype === other.Array.prototype)
        return "other";
    return "unknown";
}

function realmOfThrownError(func, string, replacement) {
    try {
        func(string, replacement);
    } catch (error) {
        if (error instanceof TypeError)
            return "main";
        if (error instanceof other.TypeError)
            return "other";
    }
    return "unknown";
}

const resetOtherLastMatch = other.Function("/other/.exec('other');");
const otherRegExp = new other.RegExp("b");
const otherSeparator = new other.RegExp(",");
const mainRegExp = /b/;
const mainSeparator = /,/;
const symbol = Symbol();

function matchRegExp(string, regExp) {
    return string.match(regExp);
}
noInline(matchRegExp);

function splitRegExp(string, regExp) {
    return string.split(regExp);
}
noInline(splitRegExp);

function searchRegExp(string, regExp) {
    return string.search(regExp);
}
noInline(searchRegExp);

function replaceRegExp(string, regExp) {
    return string.replace(regExp, "x");
}
noInline(replaceRegExp);

String.prototype.otherMatch = other.String.prototype.match;
String.prototype.otherSplit = other.String.prototype.split;
String.prototype.otherSearch = other.String.prototype.search;
String.prototype.otherReplace = other.String.prototype.replace;
String.prototype.otherReplaceAll = other.String.prototype.replaceAll;

function otherMatch(string) {
    return string.otherMatch("b");
}
noInline(otherMatch);

function otherSplit(string) {
    return string.otherSplit(",");
}
noInline(otherSplit);

function otherSearch(string) {
    return string.otherSearch("b");
}
noInline(otherSearch);

function otherReplace(string, replacement) {
    return string.otherReplace("b", replacement);
}
noInline(otherReplace);

function otherReplaceAll(string, replacement) {
    return string.otherReplaceAll("b", replacement);
}
noInline(otherReplaceAll);

RegExp.prototype.otherTest = other.RegExp.prototype.test;
RegExp.prototype.otherSymbolMatch = other.RegExp.prototype[Symbol.match];
RegExp.prototype.otherSymbolSearch = other.RegExp.prototype[Symbol.search];
RegExp.prototype.otherSymbolSplit = other.RegExp.prototype[Symbol.split];

function otherTest(regExp, string) {
    return regExp.otherTest(string);
}
noInline(otherTest);

function otherSymbolMatch(regExp, string) {
    return regExp.otherSymbolMatch(string);
}
noInline(otherSymbolMatch);

function otherSymbolSearch(regExp, string) {
    return regExp.otherSymbolSearch(string);
}
noInline(otherSymbolSearch);

function otherSymbolSplit(regExp, string) {
    return regExp.otherSymbolSplit(string);
}
noInline(otherSymbolSplit);

function lastMatches() {
    return `${RegExp.lastMatch} ${other.RegExp.lastMatch}`;
}

for (let i = 0; i < testLoopCount; ++i) {
    // A RegExp of the other realm is passed to the methods of the main realm.
    shouldBe(realmOf(matchRegExp("abc", otherRegExp)), "other", "match");
    shouldBe(realmOf(splitRegExp("a,b", otherSeparator)), "other", "split");

    /main/.exec("main");
    shouldBe(searchRegExp("abc", otherRegExp), 1, "search");
    shouldBe(lastMatches(), "main b", "lastMatch after search");
    shouldBe(replaceRegExp("abc", otherRegExp), "axc", "replace");
    shouldBe(lastMatches(), "main b", "lastMatch after replace");

    // The String.prototype methods of the other realm are called from the code of the main realm.
    shouldBe(realmOf(otherMatch("abc")), "other", "match of other realm");
    shouldBe(realmOf(otherSplit("a,b")), "other", "split of other realm");

    resetOtherLastMatch();
    shouldBe(otherSearch("abc"), 1, "search of other realm");
    shouldBe(lastMatches(), "main b", "lastMatch after search of other realm");
    shouldBe(otherReplace("abc", "x"), "axc", "replace of other realm");
    shouldBe(otherReplaceAll("abc", "x"), "axc", "replaceAll of other realm");
    shouldBe(realmOfThrownError(otherReplace, "abc", symbol), "other", "error of replace of other realm");
    shouldBe(realmOfThrownError(otherReplaceAll, "abc", symbol), "other", "error of replaceAll of other realm");

    // The RegExp.prototype methods of the other realm are called on a RegExp of the main realm.
    resetOtherLastMatch();
    shouldBe(otherTest(mainRegExp, "abc"), true, "test of other realm");
    shouldBe(lastMatches(), "b other", "lastMatch after test of other realm");
    /main/.exec("main");
    shouldBe(otherSymbolSearch(mainRegExp, "abc"), 1, "@@search of other realm");
    shouldBe(lastMatches(), "b other", "lastMatch after @@search of other realm");
    shouldBe(realmOf(otherSymbolMatch(mainRegExp, "abc")), "main", "@@match of other realm");
    shouldBe(realmOf(otherSymbolSplit(mainSeparator, "a,b")), "other", "@@split of other realm");
}
