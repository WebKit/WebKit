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

    async function runFormattingTest(mode, testName, testURL) {
        let {testText, expectedText} = await loadFormattingTestAndExpectedResults(testURL);
        return new Promise(function(resolve, reject) {
            let workerProxy = WI.FormatterWorkerProxy.singleton();
            const indentString = "    ";

            function callback({formattedText, sourceMapData}) {
                let pass = formattedText === expectedText;
                InspectorTest.passOrFail(pass, testName);

                if (formattedText === null)
                    InspectorTest.log("Failed to parse!");
                else if (!pass) {
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

                resolve();
            }

            switch (mode) {
            case "text/javascript": {
                let isModule = /^module/.test(testName);
                workerProxy.formatJavaScript(testText, isModule, indentString, callback);
                break;
            }
            case "text/css":
                workerProxy.formatCSS(testText, indentString, callback);
                break;
            case "text/html":
                workerProxy.formatHTML(testText, indentString, callback);
                break;
            case "text/xml":
                workerProxy.formatXML(testText, indentString, callback);
                break;
            }
        });
    }

    window.addFormattingTests = function(suite, mode, tests) {
        let testPageURL = WI.networkManager.mainFrame.mainResource.url;
        let testPageResourcesURL = testPageURL.substring(0, testPageURL.lastIndexOf("/"));

        suite.addTestCase({
            name: suite.name,
            timeout: -1,
            async test(resolve, reject) {
                for (let test of tests) {
                    let testName = test.substring(test.lastIndexOf("/") + 1);
                    let testURL = testPageResourcesURL + "/" + test;
                    await runFormattingTest(mode, testName, testURL);
                }
            }
        });
    };
});
