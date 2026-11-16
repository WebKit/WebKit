// The scale is the Body text style size for the category divided by the Body size at the default category (L),
// snapped to the closest of 0.85, 0.94, 1, 1.1, 1.2, 1.75 and 2.5.
const contentSizeCategoryScales = {
    L: 1,
    XXXL: 1.2,
};

const platformHasTextScale = !!(window.testRunner && testRunner.haveTextScaleFactor);

function textScaleForCategory(category) {
    return platformHasTextScale ? contentSizeCategoryScales[category] : 1;
}

const keywordSizes = {
    "xx-small": 9,
    "x-small": 10,
    "small": 13,
    "medium": 16,
    "large": 18,
    "x-large": 24,
    "xx-large": 32,
    "xxx-large": 48,
};

const fontSizeFactors = [0.6, 0.75, 0.89, 1, 1.2, 1.5, 2, 3];

// medium is the default font size times the text scale, rounded to a whole pixel.
function scaledMediumSize(scale) {
    return Math.round(16 * scale);
}

// Keyword sizes come from a table for a medium of 16px, and from medium times a factor above it.
function scaledKeywordSize(keyword, scale) {
    const medium = scaledMediumSize(scale);
    if (medium === 16)
        return keywordSizes[keyword];
    assert_greater_than(medium, 16, "the tests only use a medium of 16px or more");
    return fontSizeFactors[Object.keys(keywordSizes).indexOf(keyword)] * medium;
}

function computedFontSize(element) {
    return parseFloat(getComputedStyle(element).fontSize);
}

function assertFontSize(element, expected, description) {
    assert_approx_equals(computedFontSize(element), expected, 0.01, description);
}

function applyContentSizeCategory(category) {
    if (platformHasTextScale)
        internals.setTextScaleFactor(testRunner.textScaleFactorForContentSizeCategory(`UICTContentSizeCategory${category}`));
}

function setContentSizeCategory(test, category) {
    applyContentSizeCategory(category);
    test.add_cleanup(() => applyContentSizeCategory("L"));
}

// An em value N such that 16N < innerWidth < N * scaledMediumSize(contentSizeCategoryScales.XXXL), so that a
// (min-width: Nem) media query matches without scaling and with category L, but not
// with category XXXL, whatever the viewport width is.
function mediaQueryThresholdEm() {
    return Math.round(innerWidth / 18 * 100) / 100;
}
