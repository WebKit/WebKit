function cmp(a, b) { return a.localeCompare(b); }
noInline(cmp);
var collator = new Intl.Collator();
function ref(a, b) { var r = collator.compare(a, b); return r < 0 ? -1 : r > 0 ? 1 : 0; }
var alphabet = "aAbBzZ09 -_.,~\u00e9\u00c9\u00ff\u0080\t";
var seed = 1;
function rnd(n) { seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff; return (seed >>> 16) % n; }
function randomString(len) { var s = ""; for (var i = 0; i < len; ++i) s += alphabet[rnd(alphabet.length)]; return s; }
function flat(s) { return s.split("").join(""); }
function flipCase(s) {
    var chars = s.split("");
    for (var i = rnd(3) + 1; i--;) {
        var j = rnd(chars.length);
        var c = chars[j];
        chars[j] = c === c.toLowerCase() ? c.toUpperCase() : c.toLowerCase();
    }
    return chars.join("");
}
var pairs = [];
var pairCount = Math.min(testLoopCount, 1000);
for (var i = 0; i < pairCount; ++i) {
    var prefix = randomString(rnd(21));
    var a = flat(prefix + randomString(rnd(4)));
    var b = flat(prefix + randomString(rnd(4)));
    var kind = rnd(4);
    if (kind == 0)
        b = flat(a);
    else if (kind == 1 && a.length)
        b = flipCase(a + randomString(rnd(4)));
    pairs.push([a, b, ref(a, b)]);
}
for (var i = 0; i < testLoopCount; ++i) {
    var p = pairs[i % pairCount];
    var r = cmp(p[0], p[1]);
    r = r < 0 ? -1 : r > 0 ? 1 : 0;
    if (r !== p[2])
        throw new Error("Mismatch for " + JSON.stringify(p[0]) + " vs " + JSON.stringify(p[1]) + ": " + r + " expected " + p[2]);
}
