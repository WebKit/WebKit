description(
'This tests that calling parentRule on the child rule of MediaRule is equal to that MediaRule.'
);

if (window.testRunner)
    testRunner.dumpAsText();

var head = document.getElementsByTagName('head')[0];
head.innerHTML = "<style>@media all { a { border-color: red; } }</style>";

var styleSheetList = document.styleSheets;
var styleSheet = styleSheetList[0];
var mediaRule = styleSheet.cssRules[0];
var childRule = mediaRule.cssRules[0];

shouldBe("childRule.parentRule", "mediaRule")
