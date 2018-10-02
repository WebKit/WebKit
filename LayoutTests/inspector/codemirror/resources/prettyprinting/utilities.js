TestPage.registerInitializer(function() {
    function loadPrettyPrintingTestAndExpectedResults(testURL) {
        let expectedURL = testURL.replace(/\.([^\.]+)$/, "-expected.$1");
        return Promise.all([
            NetworkAgent.loadResource(WI.networkManager.mainFrame.id, testURL),
            NetworkAgent.loadResource(WI.networkManager.mainFrame.id, expectedURL)
        ]).then(function(results) {
            return Promise.resolve({testText: results[0].content, expectedText: results[1].content });
        });
    }

    function runPrettyPrintingTest(mode, testName, testURL) {
        return loadPrettyPrintingTestAndExpectedResults(testURL).then(function(results) {
            let {testText, expectedText} = results;

            let editor = CodeMirror(document.createElement("div"));
            editor.setOption("mode", mode);
            editor.setValue(testText);

            const start = {line: 0, ch: 0};
            const end = {line: editor.lineCount() - 1};
            const indentString = "    ";
            let builder = new FormatterContentBuilder(indentString);
            let formatter = new WI.Formatter(editor, builder);
            formatter.format(start, end);

            let pass = builder.formattedContent === expectedText;
            InspectorTest.log(pass ? "PASS" : "FAIL");

            if (!pass) {
                InspectorTest.log("Input:");
                InspectorTest.log("-----------");
                InspectorTest.log(testText);
                InspectorTest.log("-----------");
                InspectorTest.log("Formatted Output: " + builder.formattedContent.length);
                InspectorTest.log("-----------");
                InspectorTest.log(builder.formattedContent);
                InspectorTest.log("-----------");
                InspectorTest.log("Expected Output: " + expectedText.length);
                InspectorTest.log("-----------");
                InspectorTest.log(expectedText);
                InspectorTest.log("-----------");
            }
        });
    }

    window.addPrettyPrintingTests = function(suite, mode, tests) {
        let testPageURL = WI.networkManager.mainFrame.mainResource.url;
        let testPageResourcesURL = testPageURL.substring(0, testPageURL.lastIndexOf("/"));            

        for (let test of tests) {
            let testName = test.substring(test.lastIndexOf("/") + 1);
            let testURL = testPageResourcesURL + "/" + test;
            suite.addTestCase({
                name: suite.name + "." + testName,
                test(resolve, reject) { 
                    runPrettyPrintingTest(mode, testName, testURL).then(resolve).catch(reject);
                }
            });
        }
    };
});
