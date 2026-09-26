function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`bad value for ${label}: ${actual}, expected ${expected}`);
}

const other = createGlobalObject();
const mainIteratorPrototype = Object.getPrototypeOf("".matchAll(/a/g));
const otherIteratorPrototype = Object.getPrototypeOf(other.eval("''.matchAll(/a/g)"));

function realmOf(value) {
    const prototype = Object.getPrototypeOf(value);
    if (prototype === Array.prototype || prototype === mainIteratorPrototype)
        return "main";
    if (prototype === other.Array.prototype || prototype === otherIteratorPrototype)
        return "other";
    return "unknown";
}

function lastMatchAfter(body) {
    /main/.exec("main");
    other.eval("/other/.exec('other')");
    body();
    return `${RegExp.lastMatch} ${other.RegExp.lastMatch}`;
}

function regExp(source, flags) {
    return new other.RegExp(source, flags);
}

function withOwnProperty(regExp) {
    regExp.unrelated = 1;
    return regExp;
}

shouldBe(realmOf("abc".match(/b/)), "main", "match in main realm");
shouldBe(realmOf(other.eval("'abc'.match(/b/)")), "other", "match in other realm");

// String.prototype methods call regexp[@@method], which is the method of the RegExp's realm.
shouldBe(realmOf("abc".match(regExp("b"))), "other", "match");
shouldBe(realmOf("abc".match(regExp("b", "g"))), "other", "match with global");
shouldBe(realmOf("abc".match(withOwnProperty(regExp("b")))), "other", "match with own property");
shouldBe(realmOf("a,b".split(regExp(","))), "other", "split");
shouldBe(realmOf("a,b".split(regExp(","), 1)), "other", "split with limit");
{
    const iterator = "abc".matchAll(regExp("b", "g"));
    shouldBe(realmOf(iterator), "other", "matchAll iterator");
    shouldBe(realmOf(iterator.next().value), "other", "matchAll result");
}
shouldBe(lastMatchAfter(() => "abc".match(regExp("b"))), "main b", "lastMatch after match");
shouldBe(lastMatchAfter(() => "abc".search(regExp("b"))), "main b", "lastMatch after search");
shouldBe(lastMatchAfter(() => "abc".replace(regExp("b"), "x")), "main b", "lastMatch after replace");
shouldBe(lastMatchAfter(() => "abc".replaceAll(regExp("b", "g"), "x")), "main b", "lastMatch after replaceAll");
shouldBe(lastMatchAfter(() => "a,b".split(regExp(","))), "main ,", "lastMatch after split");
shouldBe("abc".replace(regExp("b"), "x"), "axc", "replace");
shouldBe("abc".search(regExp("c")), 2, "search");

// RegExp.prototype methods call regexp.exec, which is the exec of the RegExp's realm.
const prototype = RegExp.prototype;
shouldBe(realmOf(prototype[Symbol.match].call(regExp("b"), "abc")), "other", "@@match");
shouldBe(realmOf(prototype[Symbol.match].call(regExp("b", "g"), "abc")), "main", "@@match with global");
shouldBe(realmOf(prototype[Symbol.split].call(regExp(","), "a,b")), "main", "@@split");
{
    const iterator = prototype[Symbol.matchAll].call(regExp("b", "g"), "abc");
    shouldBe(realmOf(iterator), "main", "@@matchAll iterator");
    shouldBe(realmOf(iterator.next().value), "other", "@@matchAll result");
}
shouldBe(realmOf(prototype.exec.call(regExp("b"), "abc")), "main", "exec");
shouldBe(lastMatchAfter(() => prototype.test.call(regExp("b"), "abc")), "main b", "lastMatch after test");
shouldBe(lastMatchAfter(() => prototype[Symbol.search].call(regExp("b"), "abc")), "main b", "lastMatch after @@search");
shouldBe(lastMatchAfter(() => prototype[Symbol.replace].call(regExp("b"), "abc", "x")), "main b", "lastMatch after @@replace");

// RegExp.prototype[@@matchAll] creates the matcher with the constructor of the RegExp's realm.
{
    let mainCalls = 0;
    let otherCalls = 0;
    const iterator = prototype[Symbol.matchAll].call(regExp("b", "g"), "abc");
    const mainExec = prototype.exec;
    const otherExec = other.RegExp.prototype.exec;
    prototype.exec = function (string) { mainCalls++; return mainExec.call(this, string); };
    other.RegExp.prototype.exec = function (string) { otherCalls++; return otherExec.call(this, string); };
    iterator.next();
    prototype.exec = mainExec;
    other.RegExp.prototype.exec = otherExec;
    shouldBe(mainCalls, 0, "calls of main realm exec");
    shouldBe(otherCalls, 1, "calls of other realm exec");
}
