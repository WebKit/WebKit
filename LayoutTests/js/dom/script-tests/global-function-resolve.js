description("Test to make sure cached lookups on the global object are performed correctly.");

var functionNames = [
    'addEventListener',
    'alert',
    'clearInterval',
    'clearTimeout',
    'clientInformation',
    'closed',
    'confirm',
    'console',
    'crypto',
    'defaultStatus',
    'defaultstatus',
    'description',
    'devicePixelRatio',
    'dispatchEvent',
    'document',
    'getComputedStyle',
    'getMatchedCSSRules',
    'getSelection',
    'history',
    'innerHeight',
    'innerWidth',
    'location',
    'locationbar',
    'menubar',
    'moveBy',
    'moveTo',
    'name',
    'navigator',
    'open',
    'opener',
    'outerHeight',
    'outerWidth',
    'pageXOffset',
    'pageYOffset',
    'parent',
    'prompt',
    'releaseEvents',
    'removeEventListener',
    'resizeBy',
    'resizeTo',
    'screen',
    'screenLeft',
    'screenTop',
    'screenX',
    'screenY',
    'scroll',
    'scrollBy',
    'scrollTo',
    'scrollX',
    'scrollY',
    'setInterval',
    'setTimeout',
    'showModalDialog',
    'status',
    'stop',
    'window',
];

var cachedFunctions = [];
for (var i = 0; i < functionNames.length; i++)
    cachedFunctions[i] = new Function("return " + functionNames[i]);

for (var i = 0; i < functionNames.length; i++) {
    shouldBe("cachedFunctions["+i+"]()", functionNames[i]);
    shouldBe("cachedFunctions["+i+"]()", functionNames[i]);
}
