//@ skip if $buildType == "debug"
// Splitting on a regexp that is a plain literal string, which is what Yarr classifies as
// SpecificPattern::Atom. Only a subject whose StringImpl is an atom can be memoized, so joining the
// pieces at runtime is what keeps the split cache out of this and leaves the scan being measured.
(function() {
    var pieces = [];
    for (var i = 0; i < 1000; ++i)
        pieces.push("element" + i);
    var oneCharacterSeparatorSubject = pieces.join(",");
    var multiCharacterSeparatorSubject = pieces.join("-|-");

    var result = 0;
    var n = 20000;
    for (var i = 0; i < n; ++i) {
        result += oneCharacterSeparatorSubject.split(/,/).length;
        result += multiCharacterSeparatorSubject.split(/-\|-/).length;
    }
    if (result != n * 2000)
        throw "Error: bad result: " + result;
})();
