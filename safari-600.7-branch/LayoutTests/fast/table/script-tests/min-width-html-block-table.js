description("This test checks that the min-width style is applied to block HTML tables.");

// Requires min-width-helpers.js.

function computeLogicalWidth(writingMode, direction, tableStyle)
{
    return computeLogicalWidthHelper("html", "block", writingMode, direction, tableStyle);
}

runTests("html");
