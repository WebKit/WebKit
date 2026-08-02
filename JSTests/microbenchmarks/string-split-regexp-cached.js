//@ skip if $buildType == "debug"
// Splitting the same string on the same regexp over and over, which is what the split cache exists
// for. The subjects are string literals so their StringImpls are atoms, which is what makes a result
// cacheable at all.
(function() {
    var commaSubject = "one,two,three,four,five,six,seven,eight,nine,ten";
    var lineSubject = "one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine\nten";
    var paddedSubject = "          one two three four five six seven eight nine ten          ";

    var result = 0;
    var n = 3000000;
    for (var i = 0; i < n; ++i) {
        result += commaSubject.split(/,/).length;
        result += lineSubject.split(/\r\n?|\n/).length;
        result += paddedSubject.split(/^\s+/).length;
    }
    if (result != n * 22)
        throw "Error: bad result: " + result;
})();
