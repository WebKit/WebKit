TestPage.registerInitializer(function() {
    function loadFormattingTestAndExpectedResults(testURL) {
        let expectedURL = testURL.replace(/\.([^\.]+)$/, "-expected.$1");
        return Promise.all([
            NetworkAgent.loadResource(WI.networkManager.mainFrame.id, testURL),
            NetworkAgent.loadResource(WI.networkManager.mainFrame.id, expectedURL)
        ]).then(function(results) {
            return Promise.resolve({testText: results[0].content, expectedText: results[1].content });
        });
    }

    function runFormattingTest(mode, testName, testURL) {
        return loadFormattingTestAndExpectedResults(testURL).then(function(results) {
            let {testText, expectedText} = results;
            return new Promise(function(resolve, reject) {
                const indentString = "    ";
                let workerProxy = WI.FormatterWorkerProxy.singleton();
                let isModule = /^module/.test(testName);
                workerProxy.formatJavaScript(testText, isModule, indentString, ({formattedText, sourceMapData}) => {
                    let pass = formattedText === expectedText;
                    InspectorTest.log(pass ? "PASS" : "FAIL");

                    if (!pass) {
                        InspectorTest.log("Input:");
                        InspectorTest.log("-----------");
                        InspectorTest.log(testText);
                        InspectorTest.log("-----------");
                        InspectorTest.log("Formatted Output: " + formattedText.length);
                        InspectorTest.log("-----------");
                        InspectorTest.log(formattedText);
                        InspectorTest.log("-----------");
                        InspectorTest.log("Expected Output: " + expectedText.length);
                        InspectorTest.log("-----------");
                        InspectorTest.log(expectedText);
                        InspectorTest.log("-----------");
                    }

                    resolve(pass);
                });
            });
        });
    }

    window.addFormattingTests = function(suite, mode, tests) {
        let testPageURL = WI.networkManager.mainFrame.mainResource.url;
        let testPageResourcesURL = testPageURL.substring(0, testPageURL.lastIndexOf("/"));            

        for (let test of tests) {
            let testName = test.substring(test.lastIndexOf("/") + 1);
            let testURL = testPageResourcesURL + "/" + test;
            suite.addTestCase({
                name: suite.name + "." + testName,
                test(resolve, reject) {
                    runFormattingTest(mode, testName, testURL).then(resolve).catch(reject);
                }
            });
        }
    };
});
