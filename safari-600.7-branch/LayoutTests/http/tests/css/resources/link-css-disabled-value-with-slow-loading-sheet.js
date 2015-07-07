description("Test that HTMLLinkElement's disabled attribute is properly cached while set when loading a stylesheet.");

if (window.testRunner)
    testRunner.waitUntilDone();

window.jsTestIsAsync = true;

mainSheetLink = document.getElementsByTagName("link")[0];
alternateSheetLink = document.getElementsByTagName("link")[1];

mainSheetLink.disabled = true;
alternateSheetLink.disabled = false;

debug("Testing value of 'disabled' prior to load just after setting them");
shouldBeNonNull("mainSheetLink.sheet");
shouldBeNull("alternateSheetLink.sheet");
shouldBeTrue("mainSheetLink.disabled", true);
shouldBeFalse("alternateSheetLink.disabled");

debug("Testing the values when the alternate sheet is loaded (as this is the only one that has sheet() === null)");

function onSheetLoaded(f, elem, maxtime) {
    if (elem.sheet || maxtime <= 0)
        f();
    else
        setTimeout(function () { onSheetLoaded(f, elem, maxtime - 25);}, 25);
}

function testWhenLoaded() {
        // Those next 2 lines are a sanity check.
        // If the second check fails, it is likely that the test timed out and thus
        // you can discard the rest of results as it is not testing what we want
        // (namely that the disabled value is passed to the final sheet).
        shouldBeNonNull("mainSheetLink.sheet");
        shouldBeNonNull("alternateSheetLink.sheet");

        shouldBeTrue("mainSheetLink.disabled");
        shouldBeFalse("alternateSheetLink.disabled");

        finishJSTest();
}

onSheetLoaded(testWhenLoaded, alternateSheetLink, 500);

var successfullyParsed = true;
