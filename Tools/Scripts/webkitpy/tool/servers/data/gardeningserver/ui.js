var ui = ui || {};

(function () {

ui.summarizeResultsByTest = function(resultsByTest)
{
    var block = $('<div class="results-summary"></div>');
    $.each(resultsByTest, function(testName, resultNodesByBuilder) {
        var testBlock = $('<div class="test"><div class="testName"></div><div class="builders"></div></div>');
        block.append(testBlock);
        $('.testName', testBlock).text(testName);
        $.each(resultNodesByBuilder, function(builderName, resultNode) {
            var builderBlock = $('<div class="builder"><div class="builderName"></div><div class="actual"></div><div class="expected"></div><button class="show-results">Show Results</button><button class="regression-range">Regression Range</button></div>');
            $('.builders', testBlock).append(builderBlock);
            $('.builderName', builderBlock).text(builderName);
            $('.actual', builderBlock).text(resultNode.actual);
            $('.expected', builderBlock).text(resultNode.expected);
        });
    });

    return block;
};

ui.results = function(resultsURLs)
{
    var block = $('<div class="results"></div>');
    $.each(resultsURLs, function(index, resultURL) {
        var kind = results.resultKind(resultURL);
        var type = results.resultType(resultURL);
        var fragment = type == results.kImageType ? '<img>' : '<iframe></iframe>';
        block.append($(fragment).attr('src', resultURL).addClass(kind))
    });
    return block;
};

})();
