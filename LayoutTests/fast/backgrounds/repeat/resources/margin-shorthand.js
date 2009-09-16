description('Tests that shorthand property value is correct even if' +
            ' background-repeat property is declared before it in the ' +
            ' style declaration. It tests regression described in ' + 
            '<a href="https://bugs.webkit.org/show_bug.cgi?id=28973">this bug</a>.');

function iconMarginValue()
{
    var iconDiv = document.getElementById('icon');
    var rules = window.getMatchedCSSRules(iconDiv,'',false);
    return rules[1].style.getPropertyValue('margin');
}

shouldBe('iconMarginValue()', '"0px"');

var successfullyParsed = true;
