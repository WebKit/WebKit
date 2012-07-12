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
setup({"output":false});

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
		resultStr += "Harness Error. harness_status.status = " + 
					 harness_status.status +
					 " , harness_status.message = " +
					 harness_status.message;
	}
	else {
		// Iterate through tests array and build string that contains
		// results for all tests
		for(var i=0; i<tests.length; i++){				 
			resultStr += convertResult(tests[i].status) + " " + 
						( (tests[i].name!=null) ? tests[i].name : "" ) + " " +
						( (tests[i].message!=null) ? tests[i].message : "" ) + 
						"\n";
		}			
	}

	// Set results element's innerHTML to the results string
	results.innerHTML = resultStr;

	// Add results element to document
	document.body.appendChild(results);

	if (self.testRunner)
		testRunner.notifyDone();
});
