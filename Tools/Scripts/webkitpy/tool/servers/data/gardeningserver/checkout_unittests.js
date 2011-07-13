(function () {

module("checkout");

test("subversionURLForTest", 1, function() {
    equals(checkout.subversionURLForTest("path/to/test.html"), "http://svn.webkit.org/repository/webkit/trunk/LayoutTests/path/to/test.html");
});

})();
