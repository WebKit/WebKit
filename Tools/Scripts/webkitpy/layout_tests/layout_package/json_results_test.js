// To run these tests, load json_results.html in a browser.
// You should see a series of PASS lines.

var g_testIndex = 0;
var g_log = ["You should see a serios of PASS lines."];
var g_currentTestSucceeded;

function mockResults()
{
    return {
        tests: {},
        "skipped": 0,
        "num_regressions": 0,
        "version": 0,
        "num_passes": 0,
        "fixable": 0,
        "num_flaky": 0,
        "layout_tests_dir":"/WEBKITROOT",
        "uses_expectations_file":true,
        "has_pretty_patch":true,
        "has_wdiff":true
    };
}

function mockExpectation(expected, actual)
{
    return {
        expected: expected,
        time_ms: 1,
        actual: actual,
        has_stderr: false
    };
}

function log(msg)
{
    g_log.push('TEST-' + g_testIndex + ': ' + msg);
}

function assertTrue(bool)
{
    g_currentTestSucceeded &= bool;
    log(bool ? 'PASS' : 'FAIL')
}

function runTest(results, assertions)
{
    document.body.innerHTML = '';
    g_testIndex++;
    g_state = undefined;

    try {
        ADD_RESULTS(results);
        originalGeneratePage();
        g_currentTestSucceeded = true;
    } catch (e) {
        log("FAIL: uncaught exception " + e.toString());
        g_currentTestSucceeded = false;
    }
    
    try {
        assertions();
    } catch (e) {
        log("FAIL: uncaught exception executing assertions " + e.toString());
        g_currentTestSucceeded = false;
    }

    if (g_currentTestSucceeded)
        log('PASS');
}

function runTests()
{
    var results = mockResults();
    results.tests['foo/bar.html'] = mockExpectation('PASS', 'TEXT');
    runTest(results, function() {
        assertTrue(document.getElementById('image-results-header').textContent == '');
        assertTrue(document.getElementById('text-results-header').textContent != '');
    });
    
    results = mockResults();
    results.tests['foo/bar.html'] = mockExpectation('TEXT', 'MISSING');
    results.tests['foo/bar.html'].is_missing_text = true;
    results.tests['foo/bar.html'].is_missing_audio = true;
    results.tests['foo/bar.html'].is_missing_image = true;
    runTest(results, function() {
        assertTrue(!document.getElementById('results-table'));
        assertTrue(document.querySelector('#new-tests-table .test-link').textContent == 'foo/bar.html');
        assertTrue(document.querySelector('#new-tests-table .result-link:nth-child(1)').textContent == 'audio result');
        assertTrue(document.querySelector('#new-tests-table .result-link:nth-child(2)').textContent == 'result');
        assertTrue(document.querySelector('#new-tests-table .result-link:nth-child(3)').textContent == 'png result');
    });

    results = mockResults();
    results.tests['foo/bar.html'] = mockExpectation('PASS', 'TEXT');
    results.tests['foo/bar.html'].has_stderr = true;
    runTest(results, function() {
        assertTrue(document.getElementById('results-table'));
        assertTrue(document.querySelector('#stderr-table .result-link').textContent == 'stderr');
    });

    results = mockResults();
    results.tests['foo/bar.html'] = mockExpectation('TEXT', 'PASS');
    results.tests['foo/bar1.html'] = mockExpectation('CRASH', 'PASS');
    results.tests['foo/bar2.html'] = mockExpectation('IMAGE', 'PASS');
    runTest(results, function() {
        assertTrue(!document.getElementById('results-table'));

        var testLinks = document.querySelectorAll('#passes-table .test-link');
        assertTrue(testLinks[0].textContent == 'foo/bar.html');
        assertTrue(testLinks[1].textContent == 'foo/bar1.html');
        assertTrue(testLinks[2].textContent == 'foo/bar2.html');

        var expectationTypes = document.querySelectorAll('#passes-table td:last-of-type');
        assertTrue(expectationTypes[0].textContent == 'TEXT');
        assertTrue(expectationTypes[1].textContent == 'CRASH');
        assertTrue(expectationTypes[2].textContent == 'IMAGE');

    });

    document.body.innerHTML = '<pre>' + g_log.join('\n') + '</pre>';
}

var originalGeneratePage = generatePage;
generatePage = runTests;
