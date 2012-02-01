description("This test checks that the min-width style is applied to block CSS tables.");

// Requires min-width-helpers.js.

function computeLogicalWidth(writingMode, direction, tableStyle)
{
    return computeLogicalWidthHelper("css", "block", writingMode, direction, tableStyle);
}

runTests("css");
