description("This test checks that the min-width style is applied to inline HTML tables.");

// Requires min-width-helpers.js.

function computeLogicalWidth(writingMode, direction, tableStyle)
{
    return computeLogicalWidthHelper("html", "inline", writingMode, direction, tableStyle);
}

runTests("html");
