//@ skip if $buildType == "debug"
// Splitting on \r\n?|\n, which is what Yarr classifies as SpecificPattern::Newlines. Only a subject
// whose StringImpl is an atom can be memoized, so joining the pieces at runtime is what keeps the
// split cache out of this and leaves the scan being measured.
(function() {
    var pieces = [];
    for (var i = 0; i < 1000; ++i)
        pieces.push("element" + i);
    var unixSubject = pieces.join("\n");
    var windowsSubject = pieces.join("\r\n");

    var result = 0;
    var n = 20000;
    for (var i = 0; i < n; ++i) {
        result += unixSubject.split(/\r\n?|\n/).length;
        result += windowsSubject.split(/\r\n?|\n/).length;
    }
    if (result != n * 2000)
        throw "Error: bad result: " + result;
})();
