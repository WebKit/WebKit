var ui = ui || {};

(function () {

function displayNameForBuilder(builderName)
{
    return builderName.replace(/Webkit /, '');
}

ui.urlForTest = function(testName)
{
    return 'http://trac.webkit.org/browser/trunk/LayoutTests/' + testName;
}

ui.summarizeTest = function(testName, resultNodesByBuilder)
{
    var unexpectedResults = results.collectUnexpectedResults(resultNodesByBuilder);
    var block = $(
        '<div class="test">' +
          '<span class="what"><a draggable></a></span>' +
          '<span>fails on</span>' +
          '<ul class="where"></ul>' +
        '</div>');
    $('.what a', block).text(testName).attr('href', ui.urlForTest(testName)).attr('class', unexpectedResults.join(' '));

    var where = $('.where', block);
    $.each(resultNodesByBuilder, function(builderName, resultNode) {
        where.append($('<li></li>').text(displayNameForBuilder(builderName)));
    });

    return block;
};

ui.summarizeResultsByTest = function(resultsByTest)
{
    var block = $('<div class="results-summary"></div>');
    $.each(resultsByTest, function(testName, resultNodesByBuilder) {
        block.append(ui.summarizeTest(testName, resultNodesByBuilder));
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
