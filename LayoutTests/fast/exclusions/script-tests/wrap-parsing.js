description('Test parsing of the CSS wrap property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitWrap;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-wrap", value);
    var webkitWrapFlowComputedValue = getComputedStyle(div).getPropertyValue("-webkit-wrap-flow");
    var webkitWrapMarginComputedValue = getComputedStyle(div).getPropertyValue("-webkit-wrap-margin");
    var webkitWrapPaddingComputedValue = getComputedStyle(div).getPropertyValue("-webkit-wrap-padding");
    document.body.removeChild(div);
    return webkitWrapFlowComputedValue + " " + webkitWrapMarginComputedValue + " " + webkitWrapPaddingComputedValue;
}

shouldBeEqualToString('test("-webkit-wrap: auto")', "auto");
shouldBeEqualToString('test("-webkit-wrap: start 1px")', "start 1px");
shouldBeEqualToString('test("-webkit-wrap: end 1px 2px")', "end 1px 2px");
shouldBeEqualToString('test("-webkit-wrap: 5px both 10pt;")', "both 5px 10pt");
shouldBeEqualToString('test("-webkit-wrap: 2px 3px clear;")', "clear 2px 3px");
shouldBeEqualToString('test("-webkit-wrap: 5px maximum;")', "maximum 5px");
shouldBeEqualToString('test("-webkit-wrap: 5px;")', "5px");
shouldBeEqualToString('test("-webkit-wrap: 5px 10px;")', "5px 10px");

shouldBeEqualToString('test("-webkit-wrap: none 10px 10em;")', "");
shouldBeEqualToString('test("-webkit-wrap: auto -5px;")', "");
shouldBeEqualToString('test("-webkit-wrap: auto both;")', "");
shouldBeEqualToString('test("-webkit-wrap: auto 1px -10px;")', "");
shouldBeEqualToString('test("-webkit-wrap: -5px start;")', "");
shouldBeEqualToString('test("-webkit-wrap: 5px end \'string\';")', "");

shouldBeEqualToString('testComputedStyle("auto")', "auto 0px 0px");
shouldBeEqualToString('testComputedStyle("start 1px")', "start 1px 0px");
shouldBeEqualToString('testComputedStyle("end 1px 2px")', "end 1px 2px");
shouldBeEqualToString('testComputedStyle("5px maximum")', "maximum 5px 0px");
shouldBeEqualToString('testComputedStyle("5px")', "auto 5px 0px");
shouldBeEqualToString('testComputedStyle("5px 10px")', "auto 5px 10px");

shouldBeEqualToString('testComputedStyle("none 5px 10px")', "auto 0px 0px");
shouldBeEqualToString('testComputedStyle("auto -5px")', "auto 0px 0px");

