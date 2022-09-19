/*
 * This file is intended for vendors to implement
 * code needed to integrate testharness.js tests with their own test systems.
 *
 * Typically such integration will attach callbacks when each test is
 * has run, using add_result_callback(callback(test)), or when the whole test file has
 * completed, using add_completion_callback(callback(tests, harness_status)).
 *
 * For more documentation about the callback functions and the
 * parameters they are called with see testharness.js
 */

// Setup for WebKit JavaScript tests
if (self.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    testRunner.grantWebNotificationPermission(self.origin);
    // Let's restrict calling testharness timeout() to wptserve tests for the moment.
    // That will limit the impact to a small number of tests.
    // The risk is that testharness timeout() might be called to late on slow bots to finish properly.
    if (testRunner.timeout && (location.port == 8800 || location.port == 9443))
        setTimeout(timeout, testRunner.timeout * 0.9);

    if (location.port == 8800 || location.port == 9443) {
        if (self.internals) {
            // Use 44100 sample rate by default instead of the hardware sample rate so that we get consistent results across machines.
            internals.settings.setDefaultAudioContextSampleRate(44100);
        }
    }

    if (testRunner.setStatisticsShouldDowngradeReferrer) 
       testRunner.setStatisticsShouldDowngradeReferrer(false, function() { });
}

if (self.internals && internals.setICECandidateFiltering)
    internals.setICECandidateFiltering(false);

// Function used to convert the test status code into
// the corresponding string
function convertResult(resultStatus)
{
    if(resultStatus == 0)
        return("PASS");
    else if(resultStatus == 1)
        return("FAIL");
    else if(resultStatus == 2)
        return("TIMEOUT");
    else
        return("NOTRUN");
}

if (self.testRunner) {
    /* Disable the default output of testharness.js.  The default output formats
    *  test results into an HTML table.  When that table is dumped as text, no
    *  spacing between cells is preserved, and it is therefore not readable. By
    *  setting output to false, the HTML table will not be created
    */
    setup({"output": false, "explicit_timeout": true});

    /*  Using a callback function, test results will be added to the page in a
    *   manner that allows dumpAsText to produce readable test results
    */
    add_completion_callback(function (tests, harness_status) {
        var resultStr = "\n";

        // Sanitizes the given text for display in test results.
        function sanitize(text) {
            if (!text) {
                return "";
            }
            text = text.replace(/\0/g, "\\0");
            return text.replace(/\r/g, "\\r");
        }

        if(harness_status.status != 0)
            resultStr += "Harness Error (" + convertResult(harness_status.status) + "), message = " + harness_status.message + "\n\n";

        // Truncate decimal values from output in webaudio tests to avoid test flakiness due to floating point precision issues.
        const isWebAudioTest = document.URL.indexOf('/webaudio') >= 0;

        for (var i = 0; i < tests.length; i++) {
            var message = sanitize(tests[i].message);
            if (tests[i].status == 1 && !tests[i].dumpStack) {
                // Remove stack for failed tests for proper string comparison without file paths.
                // For a test to dump the stack set its dumpStack attribute to true.
                var stackIndex = message.indexOf("(stack:");
                if (stackIndex > 0)
                    message = message.substr(0, stackIndex);
            }
 
            let testName = tests[i].name;
            if (isWebAudioTest) {
                  testName = testName.replace(/\[[0-9.,\-e]*[0-9]\.[0-9]{7,}[0-9.,\-e]*\]/, "[expected array]");
                  testName = testName.replace(/[0-9]\.[0-9]{7,}/g, (match, offset, string) => {
                      return parseFloat(match).toFixed(6);
                  });
                  testName = testName.replace(/ \(contains [0-9]+ different values\)./, ".");
                  testName = testName.replace(/ \(contains 1 different value\)./, ".");
                  testName = testName.replace(/: Actual Tail Frame [0-9]+ is greater than or equal to [0-9]+./, ".");
            }

            resultStr += convertResult(tests[i].status) + " " + sanitize(testName) + " " + message + "\n";
        }

        var results = document.createElementNS("http://www.w3.org/1999/xhtml", "pre");
        results.innerText = resultStr;

        var log = document.getElementById("log");
        if (log)
            log.appendChild(results);
        else if (document.body)
            document.body.appendChild(results);
        else {
            var root = document.documentElement;
            var is_html = root
                    && root.namespaceURI == "http://www.w3.org/1999/xhtml"
                    && root.localName == "html";
            var is_svg = document.defaultView
                    && "SVGSVGElement" in document.defaultView
                    && root instanceof document.defaultView.SVGSVGElement;

            if (is_svg) {
                var foreignObject = document.createElementNS("http://www.w3.org/2000/svg", "foreignObject");
                foreignObject.setAttribute("width", "100%");
                foreignObject.setAttribute("height", "100%");
                root.appendChild(foreignObject);
                foreignObject.appendChild(results);
            } else if (is_html) {
                root.appendChild(document.createElementNS("http://www.w3.org/1999/xhtml", "body"))
                    .appendChild(results);
            } else {
                root.appendChild(results);
            }
        }

        // Wait for any other completion callbacks, which may eliminate test elements
        // from the page and therefore reduce the output.
        setTimeout(function () {
            testRunner.forceImmediateCompletion();
        }, 0);
    });
}
