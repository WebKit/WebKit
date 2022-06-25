TestPage.registerInitializer(function() {
    const caret = "#";
    const linesOfContext = 2;

    function logLocationWithContext(line, column, sourceLines) {
        let startLine = Math.max(0, line - linesOfContext);
        let endLine = Math.min(line + linesOfContext, sourceLines.length - 1);
        for (let currentLine = startLine; currentLine <= endLine; ++currentLine) {
            let text = sourceLines[currentLine];
            let output = ("" + currentLine).padStart(3, " ") + "| ";
            if (currentLine === line) {
                // Line with the column to highlight.
                for (let i = 0; i < column; ++i)
                    output += text[i];
                output += caret;
                for (let i = column; i < text.length; ++i)
                    output += text[i];
            } else {
                // Simple line.
                output += text;
            }
            InspectorTest.log(output);
        }
    }

    async function loadFormattedContentAndSourceMap(mode, testText, testName, testURL) {
        return await new Promise((resolve, reject) => {
            let workerProxy = WI.FormatterWorkerProxy.singleton();
            const includeSourceMapData = true;
            const indentString = "    ";
            function callback(result) {
                resolve(result);
            }

            switch (mode) {
            case "text/javascript": {
                let isModule = /^module/.test(testName);
                workerProxy.formatJavaScript(testText, isModule, indentString, includeSourceMapData, callback);
                break;
            }
            case "text/css":
                workerProxy.formatCSS(testText, indentString, includeSourceMapData, callback);
                break;
            case "text/html":
                workerProxy.formatHTML(testText, indentString, includeSourceMapData, callback);
                break;
            }
        });
    }

    async function loadSourceMapTestResource(testURL) {
        let result = await NetworkAgent.loadResource(WI.networkManager.mainFrame.id, testURL);
        return result.content;
    }

    async function runSourceMapTest(mode, testName, testURL) {
        let testText = await loadSourceMapTestResource(testURL);
        let {formattedText, sourceMapData} = await loadFormattedContentAndSourceMap(mode, testText, testName, testURL);
        let formatterSourceMap = WI.FormatterSourceMap.fromSourceMapData(sourceMapData);

        let originalLines = testText.split("\n");
        let formattedLines = formattedText.split("\n");

        InspectorTest.log("Original Content Length", testText.length);
        InspectorTest.log("Formatted Content Length", formattedText.length);

        InspectorTest.log("---- Original to Formatted ----");
        {
            let line = 0;
            let column = 0;
            for (let i = 0; i < testText.length; ++i) {
                let c = testText[i];
                if (c === "\n") {
                    line++;
                    column = 0;
                } else
                    column++;

                let formattedLocation = formatterSourceMap.originalToFormatted(line, column);
                let formattedPosition = formatterSourceMap.originalPositionToFormattedPosition(i);
                let formattedLine = formattedLocation.lineNumber;
                let formattedColumn = formattedLocation.columnNumber;

                InspectorTest.log(`Original: ${i} (${line}, ${column})`);
                logLocationWithContext(line, column, originalLines);
                InspectorTest.log(`Formatted: ${formattedPosition} (${formattedLine}, ${formattedColumn})`);
                logLocationWithContext(formattedLine, formattedColumn, formattedLines);

                let nextOriginalChar = originalLines[line][column];
                let nextFormattedChar = formattedLines[formattedLine][formattedColumn];
                if (nextOriginalChar !== undefined)
                    InspectorTest.assert(nextOriginalChar === nextFormattedChar, `FAIL: Mapping appears to be to a different token. ${JSON.stringify(nextOriginalChar)} => ${JSON.stringify(nextFormattedChar)}`);

                InspectorTest.log("");
            }
        }

        InspectorTest.log("");
        InspectorTest.log("---- Formatted to Original ----");
        {
            let line = 0;
            let column = 0;
            for (let i = 0; i < formattedText.length; ++i) {
                let c = formattedText[i];
                if (c === "\n") {
                    line++;
                    column = 0;
                } else
                    column++;

                let originalLocation = formatterSourceMap.formattedToOriginal(line, column);
                let originalPosition = formatterSourceMap.formattedPositionToOriginalPosition(i);
                let originalLine = originalLocation.lineNumber;
                let originalColumn = originalLocation.columnNumber;

                InspectorTest.log(`Formatted: ${i} (${line}, ${column})`);
                logLocationWithContext(line, column, formattedLines);
                InspectorTest.log(`Original: ${originalPosition} (${originalLine}, ${originalColumn})`);
                logLocationWithContext(originalLine, originalColumn, originalLines);

                InspectorTest.log("");
            }
        }
    }

    window.addSourceMapTest = function(suite, mode, name, test) {
        let testPageURL = WI.networkManager.mainFrame.mainResource.url;
        let testPageResourcesURL = testPageURL.substring(0, testPageURL.lastIndexOf("/"));

        suite.addTestCase({
            name: suite.name + "." + name,
            timeout: -1,
            async test(resolve, reject) {
                let testName = test.substring(test.lastIndexOf("/") + 1);
                let testURL = testPageResourcesURL + "/" + test;
                await runSourceMapTest(mode, testName, testURL);
            }
        });
    };
});
