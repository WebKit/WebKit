description("This test checks that the min-width style is applied to inline CSS tables.");

// Requires min-width-helpers.js.

function computeLogicalWidth(writingMode, direction, tableStyle)
{
    return computeLogicalWidthHelper("css", "inline", writingMode, direction, tableStyle);
}

runTests("css");