description('This layouttest was initially there to test that' +
            ' shorthand property value is correct even if' +
            ' background-repeat property is declared before it in the ' +
            ' style declaration. It used to test regression described in ' + 
            ' <a href="https://bugs.webkit.org/show_bug.cgi?id=28973">this bug</a>.' + 
            ' Now that access to non author stylesheet is blocked, we should instead' +
            ' get null when accessing the css rules on that object.');

function iconMarginValue()
{
    var iconDiv = document.getElementById('icon');
    var rules = window.getMatchedCSSRules(iconDiv,'',false);
    return rules[1] ? rules[1].style.getPropertyValue('margin') : 'null';
}

shouldBe('iconMarginValue()', '"null"');
