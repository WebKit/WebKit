//@ requireOptions("--useRecursiveJSONParse=0")
var string = '['.repeat(1000000) + ']'.repeat(1000000);
JSON.parse(string);
