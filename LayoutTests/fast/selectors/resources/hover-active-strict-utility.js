// In strict mode, there is no restriction on when :hover or :active can match.
var testCases = [
    // Tag name.
    ["divPLACEHOLDER", true],

    // Id.
    ["#targetPLACEHOLDER", true],
    ["PLACEHOLDER#target", true],
    ["#targetPLACEHOLDER#target", true],

    // Class name.
    [".aClassPLACEHOLDER", true],
    ["PLACEHOLDER.aClass", true],
    [".aClassPLACEHOLDER.otherClass", true],

    // Attribute filter.
    [".aClass[webkit]", true],
    ["PLACEHOLDER[webkit]", true],
    ["[id]PLACEHOLDER[webkit]", true],

    [".aClass[webkit=rocks]", true],
    ["PLACEHOLDER[webkit=rocks]", true],
    ["[id=target]PLACEHOLDER[webkit=rocks]", true],

    [".aClass[webkit^=ro]", true],
    ["PLACEHOLDER[webkit^=ro]", true],
    ["[id^=ta]PLACEHOLDER[webkit^=ro]", true],

    [".aClass[webkit$=ks]", true],
    ["PLACEHOLDER[webkit$=ks]", true],
    ["[id$=et]PLACEHOLDER[webkit$=ks]", true],

    [".aClass[webkit*=ck]", true],
    ["PLACEHOLDER[webkit*=ck]", true],
    ["[id*=rg]PLACEHOLDER[webkit*=ck]", true],

    [".aClass[webkit~=rocks]", true],
    ["PLACEHOLDER[webkit~=rocks]", true],
    ["[id~=target]PLACEHOLDER[webkit~=rocks]", true],

    [".aClass[webkit|=rocks]", true],
    ["PLACEHOLDER[webkit|=rocks]", true],
    ["[id|=target]PLACEHOLDER[webkit|=rocks]", true],

    // Pseudo-class other than :active/:hover.
    [":nth-child(1)PLACEHOLDER", true],
    ["PLACEHOLDER:nth-child(1)", true],
    [":nth-child(1)PLACEHOLDER:nth-child(1)", true],
    // Same with child relation.
    [":nth-child(n)PLACEHOLDER > #target", true],
    ["PLACEHOLDER:nth-child(n) > #target", true],
    [":nth-child(n)PLACEHOLDER:nth-child(n) > #target", true],
    // Same with descendant relation.
    [":nth-child(n)PLACEHOLDER #target", true],
    ["PLACEHOLDER:nth-child(n) #target", true],
    [":nth-child(n)PLACEHOLDER:nth-child(n) #target", true],

    [":-webkit-any(PLACEHOLDER) #target", true],
];

function testQuerySelector(selector, shouldMatch) {
    shouldBe('document.querySelectorAll("' + selector + '").length', shouldMatch? '1' : '0');
}

function testStyling(selector, shouldMatch) {
    var testStyle = document.getElementById('testStyle');
    var noMatchStyle = "rgb(1, 2, 3)";
    var matchStyle = "rgb(4, 5, 6)"
    testStyle.textContent = "#target { color:" + noMatchStyle + " } " + selector + " { color:" + matchStyle + " !important }";

    shouldBeEqualToString('getComputedStyle(document.getElementById("target")).color', shouldMatch ? matchStyle : noMatchStyle);

    testStyle.textContent = "";
}

function test(pseudoClass) {
    for (var i = 0, length = testCases.length; i < length; ++i) {
        var selector = testCases[i][0].replace('PLACEHOLDER', pseudoClass)
        testQuerySelector(selector, testCases[i][1]);
        testStyling(selector, testCases[i][1]);
    }

    // In strict mode, no tag name or the universal selector also work. This is tested separately because those selectors match
    // every ancestor of the target.
    shouldBe('document.querySelectorAll("' + pseudoClass + '").length', '3');
    testStyling(pseudoClass, true);
    shouldBe('document.querySelectorAll("*' + pseudoClass + '").length', '3');
    testStyling('*' + pseudoClass, true);
}