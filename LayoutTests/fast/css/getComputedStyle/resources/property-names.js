// Since the following properties are configuration specific, we skip them.
// Any testing of these can go in platform-specific tests.
var propertiesToSkip = {
    // CSS_GRID_LAYOUT
    "-webkit-grid-columns": true,
    "-webkit-grid-rows": true,
    
    // DASHBOARD_SUPPORT
    "-webkit-dashboard-region": true,
    
    // TOUCH_EVENTS
    // "-webkit-tap-highlight-color" is only available on the ports
    // which have enabled TOUCH_EVENTS flag. We test it in layout test
    // fast/events/touch/tap-highlight-color.html.
    "-webkit-tap-highlight-color": true,
};

// There properties don't show up when iterating a computed style object,
// but we do want to dump their values in tests.
var hiddenComputedStyleProperties = [
    "background-position-x",
    "background-position-y",
    "border-spacing",
    "overflow",
    "-webkit-mask-position-x",
    "-webkit-mask-position-y",
    "-webkit-match-nearest-mail-blockquote-color",
    "-webkit-text-size-adjust",
];
