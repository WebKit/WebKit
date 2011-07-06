(function() {
  var TEST_RESULTS_SERVER = 'http://test-results.appspot.com/';
  var TEST_TYPE = 'layout-tests';
  var RESULTS_NAME = 'full_results.json';
  var MASTER_NAME = 'ChromiumWebkit';

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

  fetchResults('Webkit Linux', function(data) {
    console.log("== Results ==")
    console.log(data);
  });
})();
