(function() {
  var TEST_RESULTS_SERVER = 'http://test-results.appspot.com/';
  var TEST_TYPE = 'layout-tests';
  var RESULTS_NAME = 'full_results.json';
  var MASTER_NAME = 'ChromiumWebkit';
  var FAILING_RESULTS = ['TIMEOUT', 'TEXT', 'CRASH', 'IMAGE','IMAGE+TEXT'];

  function isFailure(result) {
    return FAILING_RESULTS.indexOf(result) != -1;
  }

  function anyIsFailure(results_list) {
    return $.grep(results_list, isFailure).length > 0;
  }

  function addImpliedExpectations(results_list) {
    if (results_list.indexOf('FAIL') == -1)
      return results_list;
    return results_list.concat(FAILING_RESULTS);
  }

  function unexpectedResults(result_node) {
    var actual_results = result_node.actual.split(' ');
    var expected_results = addImpliedExpectations(result_node.expected.split(' '))

    return $.grep(actual_results, function(result) {
      return expected_results.indexOf(result) == -1;
    });
  }

  function isUnexpectedFailure(result_node) {
    return anyIsFailure(unexpectedResults(result_node));
  }

  function isResultNode(node) {
    return !!node.actual;
  }

  function logUnexpectedFailures(results_json) {
    unexpected_failures = base.filterTree(results_json.tests, isResultNode, isUnexpectedFailure);
    console.log('== Unexpected Failures ==')
    console.log(unexpected_failures);
  }

  function resultsURL(builder_name, name) {
    return TEST_RESULTS_SERVER + 'testfile' +
        '?builder=' + builder_name +
        '&master=' + MASTER_NAME +
        '&testtype=' + TEST_TYPE +
        '&name=' + name;
  }

  function fetchResults(builder_name, onsuccess) {
    $.ajax({
      url: resultsURL(builder_name, RESULTS_NAME),
      dataType: 'jsonp',
      success: onsuccess
    });
  }

  fetchResults('Webkit Linux', logUnexpectedFailures);
})();
