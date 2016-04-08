/*
 * THIS FILE INTENTIONALLY LEFT BLANK
 *
 * More specifically, this file is intended for vendors to implement
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
    // Let's restrict calling testharness timeout() to wptserve tests for the moment.
    // That will limit the impact to a small number of tests.
    // The risk is that testharness timeout() might be called to late on slow bots to finish properly.
    if (testRunner.timeout && location.port == 8800)
        setTimeout(timeout, testRunner.timeout * 0.9);
}

// Function used to convert the test status code into
// the corresponding string
function convertResult(resultStatus){
	if(resultStatus == 0)
		return("PASS");
	else if(resultStatus == 1)
		return("FAIL");
	else if(resultStatus == 2)
		return("TIMEOUT");
	else
		return("NOTRUN");
}

/* Disable the default output of testharness.js.  The default output formats
*  test results into an HTML table.  When that table is dumped as text, no
*  spacing between cells is preserved, and it is therefore not readable. By
*  setting output to false, the HTML table will not be created
*/
setup({"output": false, "explicit_timeout": !!testRunner});

/*  Using a callback function, test results will be added to the page in a 
*   manner that allows dumpAsText to produce readable test results
*/
add_completion_callback(function (tests, harness_status){
	
	// Create element to hold results
	var results = document.createElement("pre");
	
	// Declare result string
	var resultStr = "\n";
	
	// Check harness_status.  If it is not 0, tests did not
	// execute correctly, output the error code and message
	if(harness_status.status != 0){
		resultStr += "Harness Error (" + 
					 convertResult(harness_status.status) +
					 "), message = " +
					 harness_status.message + "\n\n";
	}
	// Iterate through tests array and build string that contains
	// results for all tests
	for(var i = 0; i < tests.length; i++) {
		var message = (tests[i].message != null) ? tests[i].message : "";
		if (tests[i].status == 1 && !tests[i].dumpStack) {
			// Remove stack for failed tests for proper string comparison without file paths.
			// For a test to dump the stack set its dumpStack attribute to true.
			var stackIndex = message.indexOf("(stack:");
			if (stackIndex > 0) {
				message = message.substr(0, stackIndex);
			}
		}
		resultStr += convertResult(tests[i].status) + " " + 
					( (tests[i].name!=null) ? tests[i].name : "" ) + " " +
					message +
					"\n";
        }			

	results.innerText = resultStr;

	// Add results element to document
	document.body.appendChild(results);

 	if (self.testRunner)
 		testRunner.notifyDone();
});
