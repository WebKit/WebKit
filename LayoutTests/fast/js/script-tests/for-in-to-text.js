description(
"This tests that JavaScriptCore ForInNodes are converted correctly to text."
);

function test() {
    for (j in index) {
        testProperty(index[j]);
    }
}

shouldBeTrue("test.toString().match('for *[(]j *in *index[)]') != null");
var successfullyParsed = true;
