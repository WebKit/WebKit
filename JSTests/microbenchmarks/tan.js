//@ skip if $hostOS == "windows"
// FIXME: unskip this test when https://bugs.webkit.org/show_bug.cgi?id=165194 is fixed.

(function () {
    for (var i = 0; i < 3000000; ++i)
        x = Math.tan(i);

    if (x != 1.8222665884307354)
        throw "Error: bad result: " + x;
})();
