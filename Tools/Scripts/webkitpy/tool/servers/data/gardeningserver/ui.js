var ui = ui || {};

(function () {

ui.resultsByTest = function(resultsByTest)
{
    var block = $('<div class="results"></div>');
    $.each(resultsByTest, function(testName, resultNodesByBuilder) {
        var testBlock = $('<div class="test"><div class="testName"></div><div class="builders"></div></div>');
        block.append(testBlock);
        $('.testName', testBlock).text(testName);
        $.each(resultNodesByBuilder, function(builderName, resultNode) {
            var builderBlock = $('<div class="builder"><div class="builderName"></div><div class="actual"></div><div class="expected"></div></div>');
            $('.builders', testBlock).append(builderBlock);
            $('.builderName', builderBlock).text(builderName);
            $('.actual', builderBlock).text(resultNode.actual);
            $('.expected', builderBlock).text(resultNode.expected);
        });
    });

    return block;
};

})();
