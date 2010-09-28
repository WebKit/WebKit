/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// requires jQuery

const kTestSuiteVersion = '20100917';
const kTestSuiteHome = '../' + kTestSuiteVersion + '/';
const kTestInfoDataFile = 'testinfo.data';

const kChapterFiles = [
  "chapter-1",
  "chapter-2",
  "chapter-3",
  "chapter-4",
  "chapter-5",
  "chapter-6",
  "chapter-7",
  "chapter-8",
  "chapter-9",
  "chapter-10",
  "chapter-11",
  "chapter-12",
  "chapter-13",
  "chapter-14",
  "chapter-15",
  "chapter-16",
  "chapter-17",
  "chapter-18",
  "chapter-A",
  "chapter-B",
  "chapter-C",
  "chapter-D",
  "chapter-E",
  "chapter-F",
  "chapter-G",
  "chapter-I"
];

const kHTML4Data = {
  'path' : 'html4',
  'suffix' : '.htm'
};

const kXHTML1Data = {
  'path' : 'xhtml1',
  'suffix' : '.xht'
};

/*
  chapter = {
    'name': ,
    'title': ,
    'testcount': 
    'testNames': [] // array of test name
  }
  
  test = {
    'name': ,
    'title': ,
    'ref': ,
    'compare': ,
    'flags': ,
    'assert': ,
  }
*/

// Results popup
const kResultsSelector = [
  {
    'name': 'All Tests',
    'handler' : function(self) { self.showResultsForAllTests(); },
    'exporter' : function(self) { self.exportResultsForAllTests(); }
  },
  {
    'name': 'Completed Tests',
    'handler' : function(self) { self.showResultsForCompletedTests(); }
  },
  {
    'name': 'Passing Tests',
    'handler' : function(self) { self.showResultsForTestsWithStatus('pass'); }
  },
  {
    'name': 'Failing Tests',
    'handler' : function(self) { self.showResultsForTestsWithStatus('fail'); }
  },
  {
    'name': 'Skipped Tests',
    'handler' : function(self) { self.showResultsForTestsWithStatus('skipped'); }
  },
  {
    'name': 'Invalid Tests',
    'handler' : function(self) { self.showResultsForTestsWithStatus('invalid'); }
  },
  {
    'name': 'Tests where HTML4 and XHTML1 results differ',
    'handler' : function(self) { self.showResultsForTestsWithMismatchedResults('invalid'); }
  },
  {
    'name': 'Tests Not Run',
    'handler' : function(self) { self.showResultsForTestsNotRun(); }
  }
]; 

// Utils
String.prototype.trim = function() { return this.replace(/^\s+|\s+$/g, ''); }

function TestSuite()
{
  this.chapterInfoMap = {}; // map of chapter name to chapter info
  
  this.testInfoMap = {}; // map of test name to test info
  this.tests = []; // array of test names
  
  this.currChapterName = '';
  this.currChapterTestIndex = 0; // index of test in the current chapter
  
  this.collectTests();
  
  this.format = '';
  this.formatChanged('html4');
  
  this.openDatabase();
}

// testinfo.data just gives flat data, so we'll scrap the toc files to break tests into categories.

TestSuite.prototype.collectTests = function()
{
  this.findData(kXHTML1Data);
}

TestSuite.prototype.findData = function(data)
{
  var _self = this;
  for (var i = 0; i < kChapterFiles.length; ++i) {
    var chapterFileName = kChapterFiles[i];
    var curChapter = kTestSuiteHome + data.path + '/' + chapterFileName + data.suffix;
    
    var loadFunction = function(url, chapterName) {
      _self.asyncLoad(curChapter, 'xml', function(data) {
        _self.chapterLoaded(chapterName, data);
      });
    }
    
    loadFunction(curChapter, chapterFileName);
  }
}

TestSuite.prototype.chapterLoaded = function(chapterFileName, data)
{
  var title = data.querySelectorAll('h2')[0];

  this.chapterInfoMap[chapterFileName] = {};
  this.chapterInfoMap[chapterFileName].name = chapterFileName;
  this.chapterInfoMap[chapterFileName].title = title.innerText;
  this.chapterInfoMap[chapterFileName].testNames = [];
  
  var tbodies = data.querySelectorAll('tbody');

  for (var i = 0; i < tbodies.length; ++i) {
    var currTbody = tbodies[i];
    
    var testRows = currTbody.getElementsByTagName('tr');

    // first row contains the title
    var title = testRows[0].querySelectorAll('th a + a')[0].innerText;
    
    for (var row = 1; row < testRows.length; ++row) {
      var currRow = testRows[row];
      
      var cells = currRow.getElementsByTagName('td');
      
      var test = {};
      test.name = cells[0].innerText.trim();
      test.ref = cells[1].innerText;
      
      var compareHref = '';
      if (cells[1].firstChild) {
        compareHref = cells[1].firstChild.getAttribute('href');
        compareHref = compareHref.replace(/\.\w+$/, '');  // strip file extension
      }
      test.compare = compareHref;
      test.flags = cells[2].innerText;

      var titleText = "";
      var detailsNode = cells[3].firstChild;
      while (detailsNode && detailsNode.nodeType == Node.TEXT_NODE) {
        titleText += detailsNode.textContent;
        detailsNode = detailsNode.nextSibling;
      }

      test.title = titleText;
      test.assert = '';
      
      var assertions = cells[3].querySelectorAll('.assert > li');
      for (var n = 0; n < assertions.length; ++n) {
        test.assert += assertions[n].innerText + ' ';
      }
      
      this.addTest(chapterFileName, test);
    }
  }

  this.checkAllChaptersLoaded();
}

TestSuite.prototype.addTest = function(chapterFileName, test)
{
  // Some tests are replicated in the chapter files, so only push the test if it's new.
  if (this.testInfoMap[test.name])
    return;
  
  this.tests.push(test.name); // note, chapters may not load in order.
  this.testInfoMap[test.name] = test;

  this.chapterInfoMap[chapterFileName].testNames.push(test.name);
}

TestSuite.prototype.checkAllChaptersLoaded = function()
{
  var chaptersLoaded = 0;
  for (var key in this.chapterInfoMap)
    ++chaptersLoaded;

  if (chaptersLoaded < kChapterFiles.length)
    return;
  
  this.initializeControls();
}

TestSuite.prototype.initializeControls = function()
{
  var chapters = $('#chapters'); // returns an array, oddly.
  chapters.empty();
  
  for (var i = 0; i < kChapterFiles.length; ++i) {
    var currChapterName = kChapterFiles[i];
    
    var option = document.createElement('option');
    option.innerText =  this.chapterInfoMap[currChapterName].title;
    option._chapterName = currChapterName;
    
    chapters[0].appendChild(option);
  }
  
  var _self = this;
  chapters[0].addEventListener('change', function() {
    _self.chapterPopupChanged();
  }, false);

  this.chapterPopupChanged();
  
  // Results popup
  var resultsPopup = $('#results-popup'); // returns an array, oddly.
  resultsPopup.empty();
  
  for (var i = 0; i < kResultsSelector.length; ++i) {
    var option = document.createElement('option');
    option.innerText =  kResultsSelector[i].name;
    
    resultsPopup[0].appendChild(option);
  }
}

TestSuite.prototype.chapterPopupChanged = function()
{
  var chaptersPopup = document.getElementById('chapters');
  var chapterName = chaptersPopup.options[chaptersPopup.selectedIndex]._chapterName;

  this.setSelectedChapter(chapterName);
}

TestSuite.prototype.fillTestList = function()
{
  $('#test-list').empty();

  var testList = document.getElementById('test-list');
  
  var currChapterInfo = this.chapterInfoMap[this.currChapterName];
  for (var i = 0; i < currChapterInfo.testNames.length; ++i) {
    var testName = currChapterInfo.testNames[i];
    var option = document.createElement('option');
    option.innerText = testName;
    option._testName = testName;
    testList.appendChild(option);
  }
}

TestSuite.prototype.setSelectedChapter = function(chapterName)
{
  this.currChapterName = chapterName;
  this.currChapterTestIndex = -1;
  
  this.fillTestList();
  this.goToTestIndex(0);
  
  var chaptersPopup = document.getElementById('chapters');
  chaptersPopup.selectedIndex = kChapterFiles.indexOf(chapterName)
}

/* ------------------------------------------------------- */

TestSuite.prototype.passTest = function()
{
  this.recordResult(this.currentTestName(), 'pass');
  this.nextTest();
}

TestSuite.prototype.failTest = function()
{
  this.recordResult(this.currentTestName(), 'fail');
  this.nextTest();
}

TestSuite.prototype.invalidTest = function()
{
  this.recordResult(this.currentTestName(), 'invalid');
  this.nextTest();
}

TestSuite.prototype.skipTest = function(reason)
{
  this.recordResult(this.currentTestName(), 'skipped', reason);
  this.nextTest();
}

TestSuite.prototype.nextTest = function()
{
  if (this.currChapterTestIndex < this.numTestsInCurrentChapter() - 1)
    this.goToTestIndex(this.currChapterTestIndex + 1);
  else {
    var currChapterIndex = kChapterFiles.indexOf(this.currChapterName);
    this.goToChapterIndex(currChapterIndex + 1);
  }
}

TestSuite.prototype.previousTest = function()
{
  if (this.currChapterTestIndex > 0)
    this.goToTestIndex(this.currChapterTestIndex - 1);
  else {
    var currChapterIndex = kChapterFiles.indexOf(this.currChapterName);
    if (currChapterIndex > 0)
      this.goToChapterIndex(currChapterIndex - 1);
  }
}

/* ------------------------------------------------------- */

TestSuite.prototype.goToTestIndex = function(index)
{
  var currChapterInfo = this.chapterInfoMap[this.currChapterName];
  if (index >= 0 && index < currChapterInfo.testNames.length) {
    this.currChapterTestIndex = index;
    this.loadCurrentTest();
  }
}

TestSuite.prototype.goToChapterIndex = function(chapterIndex)
{
  if (chapterIndex >= 0 && chapterIndex < kChapterFiles.length)
    this.setSelectedChapter(kChapterFiles[chapterIndex]);
}

TestSuite.prototype.numTestsInCurrentChapter = function()
{
  var currChapterInfo = this.chapterInfoMap[this.currChapterName];
  return currChapterInfo.testNames.length;
}

TestSuite.prototype.currentTestName = function()
{
  var currChapterInfo = this.chapterInfoMap[this.currChapterName];
  if (!currChapterInfo || this.currChapterTestIndex >= currChapterInfo.testNames.length)
    return undefined;

  return currChapterInfo.testNames[this.currChapterTestIndex];
}

TestSuite.prototype.loadCurrentTest = function()
{
  var currTestName = this.currentTestName();
  if (!currTestName)
    return;

  var testInfo = this.testInfoMap[currTestName];
  if (testInfo.ref) {
    window.console.log('got ref')
    this.configureForRefTest();
    this.loadRef(testInfo);
  } else {
    this.configureForManualTest();
  }

  this.loadTest(testInfo);

  document.getElementById('test-index').innerText = this.currChapterTestIndex + 1;

  var currChapterInfo = this.chapterInfoMap[this.currChapterName];
  document.getElementById('chapter-test-count').innerText = currChapterInfo.testNames.length;
  
  document.getElementById('test-list').selectedIndex = this.currChapterTestIndex;
}

TestSuite.prototype.configureForRefTest = function()
{
  $('#test-content').addClass('with-ref');
}

TestSuite.prototype.configureForManualTest = function()
{
  $('#test-content').removeClass('with-ref');
}

TestSuite.prototype.loadTest = function(testInfo)
{
  var iframe = document.getElementById('test-frame');
  iframe.src = this.urlForTest(testInfo.name);
  
  document.getElementById('test-title').innerText = testInfo.title;
  document.getElementById('test-url').innerText = this.pathForTest(testInfo.name);
  document.getElementById('test-assertion').innerText = testInfo.assert;
  document.getElementById('test-flags').innerText = testInfo.flags;
}

TestSuite.prototype.loadRef = function(testInfo)
{
  var iframe = document.getElementById('ref-frame');
  iframe.src = this.urlForTest(testInfo.compare);
}

TestSuite.prototype.loadTestByName = function(testName)
{
  var currChapterInfo = this.chapterInfoMap[this.currChapterName];

  var testIndex = currChapterInfo.testNames.indexOf(testName);
  if (testIndex >= 0 && testIndex < currChapterInfo.testNames.length)
    this.goToTestIndex(testIndex);
}

TestSuite.prototype.pathForTest = function(testName)
{
  var prefix = this.formatInfo.path;
  var suffix = this.formatInfo.suffix;
  
  return prefix + '/' + testName + suffix;
}

TestSuite.prototype.urlForTest = function(testName)
{
  return kTestSuiteHome + this.pathForTest(testName);
}

/* ------------------------------------------------------- */

TestSuite.prototype.recordResult = function(testName, resolution, comment)
{
  if (!testName)
    return;

  this.beginAppendingOutput();
  this.appendResultToOutput(this.formatInfo, testName, resolution, comment);
  this.endAppendingOutput();
  
  if (comment == undefined)
    comment = '';
  
  this.storeTestResult(testName, this.format, resolution, comment, navigator.userAgent);
  this.updateSummaryData();
}

TestSuite.prototype.beginAppendingOutput = function()
{
}

TestSuite.prototype.endAppendingOutput = function()
{
  var output = document.getElementById('output');
  output.scrollTop = output.scrollHeight;
}

TestSuite.prototype.appendResultToOutput = function(formatData, testName, resolution, comment)
{
  var output = document.getElementById('output');
  
  var result = formatData.path + '/' + testName + formatData.suffix + '\t' + resolution;
  if (comment)
    result += '\t(' + comment + ')';

  var line = document.createElement('p');
  line.className = resolution;
  line.appendChild(document.createTextNode(result));
  output.appendChild(line);
}

TestSuite.prototype.clearOutput = function()
{
  document.getElementById('output').innerHTML = '';
}

/* ------------------------------------------------------- */

TestSuite.prototype.formatChanged = function(formatString)
{
  if (this.format == formatString)
    return;
  
  this.format = formatString;

  if (formatString == 'html4')
    this.formatInfo = kHTML4Data;
  else
    this.formatInfo = kXHTML1Data;

  this.loadCurrentTest();
}

/* ------------------------------------------------------- */

TestSuite.prototype.asyncLoad = function(url, type, handler)
{
  $.get(url, handler, type);
}

/* ------------------------------------------------------- */

TestSuite.prototype.exportResults = function(resultTypeIndex)
{
  var resultInfo = kResultsSelector[resultTypeIndex];
  if (!resultInfo)
    return;

  resultInfo.exporter(this);
}

TestSuite.prototype.exportHeader = function()
{
  var result = '# Safari 5.0.2' + ' ' + navigator.platform + '\n';
  result += '# ' + navigator.userAgent + '\n';
  result += '# http://test.csswg.org/suites/css2.1/' + kTestSuiteVersion + '/\n';
  result += 'testname\tresult\n';

  return result;
}

TestSuite.prototype.createExportLine = function(formatData, testName, resolution, comment)
{
  var result = formatData.path + '/' + testName + '\t' + resolution;
  if (comment)
    result += '\t(' + comment + ')';
  return result;
}

TestSuite.prototype.exportQueryComplete = function(data)
{
  window.open("data:text/plain," + escape(data))
}

TestSuite.prototype.resultsPopupChanged = function(index)
{
  var resultInfo = kResultsSelector[index];
  if (!resultInfo)
    return;

  this.clearOutput();
  resultInfo.handler(this);
  
  var enableExport = resultInfo.exporter != undefined;
  document.getElementById('export-button').disabled = !enableExport;
}

TestSuite.prototype.showResultsForCompletedTests = function()
{
  this.beginAppendingOutput();
  
  var _self = this;
  this.queryDatabaseForCompletedTests(
      function(item) {
        if (item.hstatus)
          _self.appendResultToOutput(kHTML4Data, item.test, item.hstatus, item.hcomment);
        if (item.xstatus)
          _self.appendResultToOutput(kXHTML1Data, item.test, item.xstatus, item.xcomment);
      },
      function() {
        _self.endAppendingOutput();
      }
    );
}

TestSuite.prototype.showResultsForAllTests = function()
{
  this.beginAppendingOutput();

  var _self = this;
  this.queryDatabaseForAllTests('test',
      function(item) {
        _self.appendResultToOutput(kHTML4Data, item.test, item.hstatus ? item.hstatus : '?', item.hcomment);
        _self.appendResultToOutput(kXHTML1Data, item.test, item.xstatus ? item.xstatus : '?', item.xcomment);
      },
      function() {
        _self.endAppendingOutput();
      }
    );
}

TestSuite.prototype.exportResultsForAllTests = function()
{
  var exportTests = []; // each test will have html and xhtml items on it

  var _self = this;
  this.queryDatabaseForAllTests('test',
      function(item) {
        var htmlLine = _self.createExportLine(kHTML4Data, item.test, item.hstatus ? item.hstatus : '?', item.hcomment);
        var xhtmlLine = _self.createExportLine(kXHTML1Data, item.test, item.xstatus ? item.xstatus : '?', item.xcomment);
        exportTests.push({
          'test' : item.test,
          'html4' : htmlLine,
          'xhtml1' : xhtmlLine })
      },
      function() {
        // Lame workaround for ORDER BY not working
        exportTests.sort(function(a, b) {
            return a.test.localeCompare(b.test);
        });
        
        var exportLines = [];
        for (var i = 0; i < exportTests.length; ++i) {
          var currTest = exportTests[i];
          exportLines.push(currTest.html4);
          exportLines.push(currTest.xhtml1);
        }
        
        var exportString = _self.exportHeader() + exportLines.join('\n');
        _self.exportQueryComplete(exportString);
      }
    );
}

TestSuite.prototype.showResultsForTestsNotRun = function()
{
  this.beginAppendingOutput();

  var _self = this;
  this.queryDatabaseForTestsNotRun(
      function(item) {
        if (!item.hstatus)
          _self.appendResultToOutput(kHTML4Data, item.test, '?', item.hcomment);
        if (!item.xstatus)
          _self.appendResultToOutput(kXHTML1Data, item.test, '?', item.xcomment);
      },
      function() {
        _self.endAppendingOutput();
      }
    );
}

TestSuite.prototype.showResultsForTestsWithStatus = function(status)
{
  this.beginAppendingOutput();

  var _self = this;
  this.queryDatabaseForTestsWithStatus(status,
      function(item) {
        if (item.hstatus == status)
          _self.appendResultToOutput(kHTML4Data, item.test, item.hstatus, item.hcomment);
        if (item.xstatus == status)
          _self.appendResultToOutput(kXHTML1Data, item.test, item.xstatus, item.xcomment);
      },
      function() {
        _self.endAppendingOutput();
      }
    );
}

TestSuite.prototype.showResultsForTestsWithMismatchedResults = function()
{
  this.beginAppendingOutput();

  var _self = this;
  this.queryDatabaseForTestsWithMixedStatus(
      function(item) {
        _self.appendResultToOutput(kHTML4Data, item.test, item.hstatus, item.hcomment);
        _self.appendResultToOutput(kXHTML1Data, item.test, item.xstatus, item.xcomment);
      },
      function() {
        _self.endAppendingOutput();
      }
    );
}

TestSuite.prototype.updateSummaryData = function()
{
  this.queryDatabaseForSummary(
      function(results) {
        
        var hTotal, xTotal;
        var hDone, xDone;
        
        for (var i = 0; i < results.length; ++i) {
          var result = results[i];
          
          switch (result.name) {
            case 'h-total': hTotal = result.count; break;
            case 'x-total': xTotal = result.count; break;
            case 'h-tested': hDone = result.count; break;
            case 'x-tested': xDone = result.count; break;
          }

          document.getElementById(result.name).innerText = result.count;
        }
        
        // We should get these all together.
        if (hTotal) {
          document.getElementById('h-percent').innerText = Math.round(100.0 * hDone / hTotal);
          document.getElementById('x-percent').innerText = Math.round(100.0 * xDone / xTotal);
        }
      }
    );
}

/* ------------------------------------------------------- */
// Database stuff

function errorHandler(transaction, error)
{
  alert('database error: ' + error.message);
}

TestSuite.prototype.openDatabase = function()
{
  var _self = this;
  this.db = window.openDatabase('css21testsuite', '1.0', 'CSS 2.1 test suite results', 4 * 1024 * 1024, function() {
    _self.databaseCreated();
  }, errorHandler);

  this.updateSummaryData();
}

TestSuite.prototype.databaseCreated = function(db)
{
  this.db.transaction(function (tx) {
    // hstatus: HTML4 result
    // xstatus: XHTML1 result
    tx.executeSql('CREATE TABLE tests (test PRIMARY KEY UNIQUE, ref, title, flags, links, assertion, hstatus, hcomment, xstatus, xcomment)', null, null, errorHandler);
  });
  
  this.populateDatabaseFromTestInfoData();
}

TestSuite.prototype.storeTestResult = function(test, format, result, comment, useragent)
{
  this.db.transaction(function (tx) {
    window.console.log('storing result ' + result + ' for ' + format);
    if (format == 'html4')
      tx.executeSql('UPDATE tests SET hstatus=?, hcomment=? WHERE test=?\n', [result, comment, test], null, errorHandler);
    else if (format == 'xhtml1')
      tx.executeSql('UPDATE tests SET xstatus=?, xcomment=? WHERE test=?\n', [result, comment, test], null, errorHandler);
  });  
}

TestSuite.prototype.populateDatabaseFromTestInfoData = function(testInfoURL)
{
  var testInfoPath = kTestSuiteHome + kTestInfoDataFile;
  
  var _self = this;
  this.asyncLoad(testInfoPath, 'data', function(data, status) {
    _self.testInfoDataLoaded(data, status);
  });

}

TestSuite.prototype.queryDatabaseForAllTests = function(sortKey, perRowHandler, completionHandler)
{
  this.db.transaction(function (tx) {
    var query;
    var args = [];
    if (sortKey != '') {
      query = 'SELECT * FROM tests ORDER BY ? ASC';  // ORDERY BY doesn't seem to work
      args.push(sortKey);
    }
    else
      query = 'SELECT * FROM tests';

    tx.executeSql(query, args, function(tx, results) {

      var len = results.rows.length;
      for (var i = 0; i < len; ++i)
        perRowHandler(results.rows.item(i));
      
      completionHandler();
    }, errorHandler);
  });  
}

TestSuite.prototype.queryDatabaseForTestsWithStatus = function(status, perRowHandler, completionHandler)
{
  this.db.transaction(function (tx) {
    tx.executeSql('SELECT * FROM tests WHERE hstatus=? OR xstatus=?', [status, status], function(tx, results) {

      var len = results.rows.length;
      for (var i = 0; i < len; ++i)
        perRowHandler(results.rows.item(i));
      
      completionHandler();
    }, errorHandler);
  });  
}

TestSuite.prototype.queryDatabaseForTestsWithMixedStatus = function(perRowHandler, completionHandler)
{
  this.db.transaction(function (tx) {
    tx.executeSql('SELECT * FROM tests WHERE hstatus IS NOT NULL AND xstatus IS NOT NULL AND hstatus <> xstatus', [], function(tx, results) {

      var len = results.rows.length;
      for (var i = 0; i < len; ++i)
        perRowHandler(results.rows.item(i));
      
      completionHandler();
    }, errorHandler);
  });  
}

TestSuite.prototype.queryDatabaseForCompletedTests = function(perRowHandler, completionHandler)
{
  this.db.transaction(function (tx) {
    tx.executeSql('SELECT * FROM tests WHERE hstatus IS NOT NULL AND xstatus IS NOT NULL', [], function(tx, results) {

      var len = results.rows.length;
      for (var i = 0; i < len; ++i)
        perRowHandler(results.rows.item(i));
      
      completionHandler();
    }, errorHandler);
  });  
}

TestSuite.prototype.queryDatabaseForTestsNotRun = function(perRowHandler, completionHandler)
{
  this.db.transaction(function (tx) {
    tx.executeSql('SELECT * FROM tests WHERE hstatus IS NULL OR xstatus IS NULL', [], function(tx, results) {

      var len = results.rows.length;
      for (var i = 0; i < len; ++i)
        perRowHandler(results.rows.item(i));

      completionHandler();
    }, errorHandler);
  });  
}

/*

  completionHandler gets called an array of results,
  which may be some or all of:
  
  data = [
    { 'name' : ,
      'count' : 
    },
  ]
  
  where name is one of:
  
    'h-total'
    'h-tested'
    'h-passed'
    'h-failed'
    'h-skipped'

    'x-total'
    'x-tested'
    'x-passed'
    'x-failed'
    'x-skipped'

 */


TestSuite.prototype.countTestsWithColumnValue = function(tx, completionHandler, column, value, label)
{  
  var allRowsCount = 'COUNT(*)';

  tx.executeSql('SELECT COUNT(*) FROM tests WHERE ' + column + '=?', [value], function(tx, results) {
    var data = [];
    if (results.rows.length > 0)
      data.push({ 'name' : label, 'count' : results.rows.item(0)[allRowsCount] })
    completionHandler(data);
  }, errorHandler);
}

TestSuite.prototype.queryDatabaseForSummary = function(completionHandler)
{
  var _self = this;
  this.db.transaction(function (tx) {

    var allRowsCount = 'COUNT(*)';
    var html4RowsCount = 'COUNT(hstatus)';
    var xhtml1RowsCount = 'COUNT(xstatus)';

    tx.executeSql('SELECT COUNT(*), COUNT(hstatus), COUNT(xstatus) FROM tests', [], function(tx, results) {

      var data = [];
      if (results.rows.length > 0) {
        var rowItem = results.rows.item(0);
        data.push({ 'name' : 'h-total' , 'count' : rowItem[allRowsCount] })
        data.push({ 'name' : 'x-total' , 'count' : rowItem[allRowsCount] })
        data.push({ 'name' : 'h-tested', 'count' : rowItem[html4RowsCount] })
        data.push({ 'name' : 'x-tested', 'count' : rowItem[xhtml1RowsCount] })
      }
      completionHandler(data);
      
    }, errorHandler);

    _self.countTestsWithColumnValue(tx, completionHandler, 'hstatus', 'pass', 'h-passed');
    _self.countTestsWithColumnValue(tx, completionHandler, 'xstatus', 'pass', 'x-passed');

    _self.countTestsWithColumnValue(tx, completionHandler, 'hstatus', 'fail', 'h-failed');
    _self.countTestsWithColumnValue(tx, completionHandler, 'xstatus', 'fail', 'x-failed');

    _self.countTestsWithColumnValue(tx, completionHandler, 'hstatus', 'skipped', 'h-skipped');
    _self.countTestsWithColumnValue(tx, completionHandler, 'xstatus', 'skipped', 'x-skipped');

    _self.countTestsWithColumnValue(tx, completionHandler, 'hstatus', 'invalid', 'h-invalid');
    _self.countTestsWithColumnValue(tx, completionHandler, 'xstatus', 'invalid', 'x-invalid');

  });
}

TestSuite.prototype.testInfoDataLoaded = function(data, status)
{
  if (status != 'success') {
    alert("Failed to load testinfo.data. Database of tests will not be initialized.");
    return;
  }
    
  var lines = data.split('\n');
  
  var tests = [];

  // First line is column labels
  for (var i = 1; i < lines.length; ++i) {
    var fields = lines[i].split('\t');
    
    var test = {};
    test.name = fields[0];
    test.ref = fields[1];
    test.title = fields[2];
    test.flags = fields[3];
    test.links = fields[4];
    test.assertion = fields[5];
    
    if (test.name.length > 0)
      tests.push(test);
  }

  this.db.transaction(function (tx) {
    for (var i = 0; i < tests.length ; ++i) {
      var currTest = tests[i];
      tx.executeSql('INSERT INTO tests (test, ref, title, flags, links, assertion) VALUES (?, ?, ?, ?, ?, ?)', [currTest.name, currTest.ref, currTest.title, currTest.flags, currTest.links, currTest.assertion], null, errorHandler);
    }
  });  
  
}


