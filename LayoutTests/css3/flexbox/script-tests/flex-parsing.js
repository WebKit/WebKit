description('Test parsing of the CSS flex() function.');

function testFlex(flexValue, styleAttribute)
{
    var div = document.createElement('div');
    div.style[styleAttribute] = flexValue;
    return div.style[styleAttribute];
}

var attributesToTest = ['width', 'height'];

for (var i in attributesToTest) {
    var attribute = attributesToTest[i];

    shouldBeEqualToString('testFlex("100px", "' + attribute + '")', '100px');
    shouldBeEqualToString('testFlex("junk", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex()", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(1)", "' + attribute + '")', '-webkit-flex(1 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(0)", "' + attribute + '")', '-webkit-flex(0 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(2.4)", "' + attribute + '")', '-webkit-flex(2.4 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(auto)", "' + attribute + '")', '-webkit-flex(1 0 auto)');
    shouldBeEqualToString('testFlex("-webkit-flex(1px)", "' + attribute + '")', '-webkit-flex(1 0 1px)');
    shouldBeEqualToString('testFlex("-webkit-flex(2em)", "' + attribute + '")', '-webkit-flex(1 0 2em)');
    shouldBeEqualToString('testFlex("-webkit-flex(0px)", "' + attribute + '")', '-webkit-flex(1 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(-2)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(foo)", "' + attribute + '")', '');

    shouldBeEqualToString('testFlex("-webkit-flex(0 0)", "' + attribute + '")', '-webkit-flex(0 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(0 1)", "' + attribute + '")', '-webkit-flex(0 1 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(1 0)", "' + attribute + '")', '-webkit-flex(1 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(2 auto)", "' + attribute + '")', '-webkit-flex(2 0 auto)');
    shouldBeEqualToString('testFlex("-webkit-flex(3 4px)", "' + attribute + '")', '-webkit-flex(3 0 4px)');
    shouldBeEqualToString('testFlex("-webkit-flex(auto 5.2)", "' + attribute + '")', '-webkit-flex(5.2 0 auto)');
    shouldBeEqualToString('testFlex("-webkit-flex(6em 4)", "' + attribute + '")', '-webkit-flex(4 0 6em)');
    shouldBeEqualToString('testFlex("-webkit-flex(4 0px)", "' + attribute + '")', '-webkit-flex(4 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(0 0px)", "' + attribute + '")', '-webkit-flex(0 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(-1 5)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(1 -1)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(-1 -1)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(auto 2em)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(2px 4px)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(0px 0px)", "' + attribute + '")', '');

    shouldBeEqualToString('testFlex("-webkit-flex(0 0 0)", "' + attribute + '")', '-webkit-flex(0 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(1 2 0)", "' + attribute + '")', '-webkit-flex(1 2 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(1 2 auto)", "' + attribute + '")', '-webkit-flex(1 2 auto)');
    shouldBeEqualToString('testFlex("-webkit-flex(1.6 2 3px)", "' + attribute + '")', '-webkit-flex(1.6 2 3px)');
    shouldBeEqualToString('testFlex("-webkit-flex(1 3px 2)", "' + attribute + '")', '-webkit-flex(1 2 3px)');
    shouldBeEqualToString('testFlex("-webkit-flex(3px 1 2)", "' + attribute + '")', '-webkit-flex(1 2 3px)');
    shouldBeEqualToString('testFlex("-webkit-flex(auto 0 0)", "' + attribute + '")', '-webkit-flex(0 0 auto)');
    shouldBeEqualToString('testFlex("-webkit-flex(0 0px 0)", "' + attribute + '")', '-webkit-flex(0 0 0px)');
    shouldBeEqualToString('testFlex("-webkit-flex(1 2 3)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(0 2 3)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(1 0 3)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(0 0 1)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(1 -2 3px)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(1 2px 3px)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(1 2px auto)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(0px 0px 0)", "' + attribute + '")', '');

    shouldBeEqualToString('testFlex("-webkit-flex(0 0 0 0)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(0 0 0px 0)", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(0 0 0px 0px)", "' + attribute + '")', '');

    shouldBeEqualToString('testFlex("-webkit-flex(1) 10px", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(1) auto", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("-webkit-flex(1) foo", "' + attribute + '")', '');
    shouldBeEqualToString('testFlex("10px -webkit-flex(1)", "' + attribute + '")', '');
}
successfullyParsed = true;
