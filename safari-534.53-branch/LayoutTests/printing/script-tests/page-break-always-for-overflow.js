description("Test page-break-{before,after}:always for overflow:{visible,hidden,scroll,auto} elements.");

function test()
{
    var overflowValues = ["visible", "hidden", "scroll", "auto"];
    var pageBreakPositions = ["page-break-before", "page-break-after"];

    var testHtml = "";
    for (var position = 0; position < pageBreakPositions.length; position++) {
        for (var value = 0; value < overflowValues.length; value++) {
            var overflowStyle = "overflow:" + overflowValues[value];
            var pageBreakStyle = pageBreakPositions[position] + ":always";
            var testId = 'test-' + pageBreakStyle + '-for-' + overflowStyle;
            testHtml += '<div style="' + overflowStyle + '"><p id="' + testId + '" style="' + pageBreakStyle + '">' + overflowStyle + ', ' + pageBreakStyle + '</p></div>\n';
        }
    }
    testHtml += '<p id="test-last-page">end</p>';
    document.getElementById("sandbox").innerHTML = testHtml;

    var expectedValues = [
        // page-break-{before,after}:always should take effect regardless of overflow value.
        ['test-page-break-before:always-for-overflow:visible', 1],
        ['test-page-break-before:always-for-overflow:hidden', 2],
        ['test-page-break-before:always-for-overflow:scroll', 3],
        ['test-page-break-before:always-for-overflow:auto', 4],
        ['test-page-break-after:always-for-overflow:visible', 4],
        ['test-page-break-after:always-for-overflow:hidden', 5],
        ['test-page-break-after:always-for-overflow:scroll', 6],
        ['test-page-break-after:always-for-overflow:auto', 7],
        ['test-last-page', 8]
    ];

    for (var i = 0; i < expectedValues.length; i++)
        pageNumberForElementShouldBe(expectedValues[i][0], expectedValues[i][1]);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
