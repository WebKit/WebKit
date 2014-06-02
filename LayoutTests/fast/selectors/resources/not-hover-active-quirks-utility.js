var testCases = [
    // Having :hover or :active does not qualify the pseudo class.
    ["PLACEHOLDER:hover", false],
    [":hoverPLACEHOLDER", false],
    [":hoverPLACEHOLDER:hover", false],
    ["PLACEHOLDER:active", false],
    [":activePLACEHOLDER", false],
    [":activePLACEHOLDER:active", false],

    // Tag qualify the pseudo class.
    ["testingPLACEHOLDER", true],

    // Id qualify the pseudo class.
    ["#targetPLACEHOLDER", true],
    ["PLACEHOLDER#target", true],
    ["#targetPLACEHOLDER#target", true],

    // Class qualify the pseudo class.
    [".aClassPLACEHOLDER", true],
    ["PLACEHOLDER.aClass", true],
    [".aClassPLACEHOLDER.otherClass", true],

    // Any attribute filter qualify the pseudo class.
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

    // A pseudo-class other than :active/:hover qualify the pseudo class.
    ["testing:nth-child(1)PLACEHOLDER", true],
    ["testingPLACEHOLDER:nth-child(1)", true],
    ["testing:nth-child(1)PLACEHOLDER:nth-child(1)", true],
    // Same with child.
    [":nth-child(n)PLACEHOLDER > #target", false],
    ["PLACEHOLDER:nth-child(n) > #target", false],
    [":nth-child(n)PLACEHOLDER:nth-child(n) > #target", false],
    // Same with descendant.
    [":nth-child(n)PLACEHOLDER #target", false],
    ["PLACEHOLDER:nth-child(n) #target", false],
    [":nth-child(n)PLACEHOLDER:nth-child(n) #target", false],

    [":-webkit-any(PLACEHOLDER) #target", false],
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

function test(original) {
    var pseudoClass = ':not(' + original + ')';

    // Since inFunctionalPseudo is true, :hover and :active can be matched in quirks mode.
    shouldBe('document.querySelectorAll("*").length - document.querySelectorAll("' + pseudoClass + '").length', '3');
    testStyling(pseudoClass, true);
    shouldBe('document.querySelectorAll("*").length - document.querySelectorAll("*' + pseudoClass + '").length', '3');
    testStyling('*' + pseudoClass, true);

    for (var i = 0, length = testCases.length; i < length; ++i) {
        var selector = testCases[i][0].replace('PLACEHOLDER', pseudoClass)
        testQuerySelector(selector, testCases[i][1]);
        testStyling(selector, testCases[i][1]);
    }
}
