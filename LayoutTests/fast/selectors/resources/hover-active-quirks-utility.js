var testCases = [
    // The universal selector does not qualify the pseudo class.
    ["PLACEHOLDER", false],
    ["*PLACEHOLDER", false],

    // Having :hover or :active does not qualify the pseudo class.
    ["PLACEHOLDER:hover", false],
    [":hoverPLACEHOLDER", false],
    [":hoverPLACEHOLDER:hover", false],
    ["PLACEHOLDER:active", false],
    [":activePLACEHOLDER", false],
    [":activePLACEHOLDER:active", false],

    // Tag qualify the pseudo class.
    ["divPLACEHOLDER", true],

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
    [":nth-child(1)PLACEHOLDER", true],
    ["PLACEHOLDER:nth-child(1)", true],
    [":nth-child(1)PLACEHOLDER:nth-child(1)", true],
    // Same with child.
    [":nth-child(n)PLACEHOLDER > #target", true],
    ["PLACEHOLDER:nth-child(n) > #target", true],
    [":nth-child(n)PLACEHOLDER:nth-child(n) > #target", true],
    // Same with descendant.
    [":nth-child(n)PLACEHOLDER #target", true],
    ["PLACEHOLDER:nth-child(n) #target", true],
    [":nth-child(n)PLACEHOLDER:nth-child(n) #target", true],

    ["PLACEHOLDER:-webkit-any(*) #target", true],
    [":-webkit-any(PLACEHOLDER) #target", true],
    [":-webkit-any(*)PLACEHOLDER #target", true],
    [":-webkit-any(*)PLACEHOLDER:-webkit-any(*) #target", true],

    ["PLACEHOLDER:not(gecko) #target", true],
    [":not(gecko)PLACEHOLDER #target", true],
    [":not(gecko)PLACEHOLDER:not(gecko) #target", true],

    // A :not() functional pseudo class that cannot match anything should still qualify.
    ["PLACEHOLDER:not([webkit^=\\\"\\\"]) #target", true],
    [":not([webkit^=\\\"\\\"])PLACEHOLDER #target", true],
    ["PLACEHOLDER:not(:nth-child(-1)) #target", true],
    [":not(:nth-child(-1))PLACEHOLDER #target", true],
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
}
