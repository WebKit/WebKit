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

const kChapterData = [
  {
    'file' : 'about.html',
    'title' : 'About the CSS 2.1 Specification',
  },
  {
    'file' : 'intro.html',
    'title' : 'Introduction to CSS 2.1',
  },
  {
    'file' : 'conform.html',
    'title' : 'Conformance: Requirements and Recommendations',
  },
  {
    'file' : "syndata.html",
    'title' : 'Syntax and basic data types',
  },
  {
    'file' : 'selector.html' ,
    'title' : 'Selectors',
  },
  {
    'file' : 'cascade.html',
    'title' : 'Assigning property values, Cascading, and Inheritance',
  },
  {
    'file' : 'media.html',
    'title' : 'Media types',
  },
  {
    'file' : 'box.html' ,
    'title' : 'Box model',
  },
  {
    'file' : 'visuren.html',
    'title' : 'Visual formatting model',
  },
  {
    'file' :'visudet.html',
    'title' : 'Visual formatting model details',
  },
  {
    'file' : 'visufx.html',
    'title' : 'Visual effects',
  },
  {
    'file' : 'generate.html',
    'title' : 'Generated content, automatic numbering, and lists',
  },
  {
    'file' : 'page.html',
    'title' : 'Paged media',
  },
  {
    'file' : 'colors.html',
    'title' : 'Colors and Backgrounds',
  },
  {
    'file' : 'fonts.html',
    'title' : 'Fonts',
  },
  {
    'file' : 'text.html',
    'title' : 'Text',
  },
  {
    'file' : 'tables.html',
    'title' : 'Tables',
  },
  {
    'file' : 'ui.html',
    'title' : 'User interface',
  },
  {
    'file' : 'aural.html',
    'title' : 'Appendix A. Aural style sheets',
  },
  {
    'file' : 'refs.html',
    'title' : 'Appendix B. Bibliography',
  },
  {
    'file' : 'changes.html',
    'title' : 'Appendix C. Changes',
  },
  {
    'file' : 'sample.html',
    'title' : 'Appendix D. Default style sheet for HTML 4',
  },
  {
    'file' : 'zindex.html',
    'title' : 'Appendix E. Elaborate description of Stacking Contexts',
  },
  {
    'file' : 'propidx.html',
    'title' : 'Appendix F. Full property table',
  },
  {
    'file' : 'grammar.html',
    'title' : 'Appendix G. Grammar of CSS',
  },
  {
    'file' : 'other.html',
    'title' : 'Other',
  },
];


const kHTML4Data = {
  'path' : 'html4',
  'suffix' : '.htm'
};

const kXHTML1Data = {
  'path' : 'xhtml1',
  'suffix' : '.xht'
};

// Results popup
const kResultsSelector = [
  {
    'name': 'All Tests',
    'handler' : function(self) { self.showResultsForAllTests(); },
    'exporter' : function(self) { self.exportResultsForAllTests(); }
  },
  {
    'name': 'Completed Tests',
    'handler' : function(self) { self.showResultsForCompletedTests(); },
    'exporter' : function(self) { self.exportResultsForCompletedTests(); }
  },
  {
    'name': 'Passing Tests',
    'handler' : function(self) { self.showResultsForTestsWithStatus('pass'); },
    'exporter' : function(self) { self.exportResultsForTestsWithStatus('pass'); }
  },
  {
    'name': 'Failing Tests',
    'handler' : function(self) { self.showResultsForTestsWithStatus('fail'); },
    'exporter' : function(self) { self.exportResultsForTestsWithStatus('fail'); }
  },
  {
    'name': 'Skipped Tests',
    'handler' : function(self) { self.showResultsForTestsWithStatus('skipped'); },
    'exporter' : function(self) { self.exportResultsForTestsWithStatus('skipped'); }
  },
  {
    'name': 'Invalid Tests',
    'handler' : function(self) { self.showResultsForTestsWithStatus('invalid'); },
    'exporter' : function(self) { self.exportResultsForTestsWithStatus('invalid'); }
  },
  {
    'name': 'Tests where HTML4 and XHTML1 results differ',
    'handler' : function(self) { self.showResultsForTestsWithMismatchedResults(); },
    'exporter' : function(self) { self.exportResultsForTestsWithMismatchedResults(); }
  },
  {
    'name': 'Tests Not Run',
    'handler' : function(self) { self.showResultsForTestsNotRun(); },
    'exporter' : function(self) { self.exportResultsForTestsNotRun(); }
  }
];

function Test(testInfoLine)
{
  var fields = testInfoLine.split('\t');
  
  this.id = fields[0];
  this.reference = fields[1];
  this.title = fields[2];
  this.flags = fields[3];
  this.links = fields[4];
  this.assertion = fields[5];
  
  this.completed = false; // true if this test has a result (pass, fail or skip)
  
  if (!this.links)
    this.links = "other.html"
}

function ChapterSection(link)
{
  var result= link.match(/^([.\w]+)(#.+)?$/);
  if (result != null) {
    this.file = result[1];
    this.anchor = result[2];
  }
  
  this.tests = [];
}

function Chapter(chapterInfo)
{
  this.file = chapterInfo.file;
  this.title = chapterInfo.title;
  this.testCount = 0;
  this.sections = []; // array of ChapterSection
}

Chapter.prototype.description = function()
{
  return this.title + ' (' + this.testCount + ' tests, ' + this.untestedCount() + ' untested)';
}

Chapter.prototype.untestedCount = function()
{
  var count = 0;
  for (var i = 0; i < this.sections.length; ++i) {
    var currSection = this.sections[i];
    for (var j = 0; j < currSection.tests.length; ++j) {
      count += currSection.tests[j].completed ? 0 : 1;
    }
    
  }
  return count;
}

// Utils
String.prototype.trim = function() { return this.replace(/^\s+|\s+$/g, ''); }

function TestSuite()
{
  this.chapterSections = {}; // map of links to ChapterSections
  this.tests = {}; // map of test id to test info
  
  this.chapters = {}; // map of file name to chapter
  this.currentChapter = null;
  
  this.currentChapterTests = []; // array of tests for the current chapter.
  this.currChapterTestIndex = -1; // index of test in the current chapter
  
  this.format = '';
  this.formatChanged('html4');
  
  this.testInfoLoaded = false;

  this.populatingDatabase = false;
  
  var testInfoPath = kTestSuiteHome + kTestInfoDataFile;
  this.loadTestInfo(testInfoPath);
}

TestSuite.prototype.loadTestInfo = function(testInfoPath)
{
  var _self = this;
  this.asyncLoad(testInfoPath, 'data', function(data, status) {
    _self.testInfoDataLoaded(data, status);
  });
}

TestSuite.prototype.testInfoDataLoaded = function(data, status)
{
  if (status != 'success') {
    alert("Failed to load testinfo.data. Database of tests will not be initialized.");
    return;
  }

  this.parseTests(data);
  this.buildChapters();

  this.testInfoLoaded = true;
  
  this.fillChapterPopup();

  this.initializeControls();

  this.openDatabase();
}

TestSuite.prototype.parseTests = function(data)
{
  var lines = data.split('\n');
  
  // First line is column labels
  for (var i = 1; i < lines.length; ++i) {
    var test = new Test(lines[i]);
    if (test.id.length > 0)
      this.tests[test.id] = test;
  }
}

TestSuite.prototype.buildChapters = function()
{
  for (var testID in this.tests) {
    var currTest = this.tests[testID];

    // FIXME: tests with more than one link will be presented to the user
    // twice. Be smarter about avoiding this.
    var testLinks = currTest.links.split(',');
    for (var i = 0; i < testLinks.length; ++i) {
      var link = testLinks[i];
      var section = this.chapterSections[link];
      if (!section) {
        section = new ChapterSection(link);
        this.chapterSections[link] = section;
      }
      
      section.tests.push(currTest);
    }
  }
  
  for (var i = 0; i < kChapterData.length; ++i) {
    var chapter = new Chapter(kChapterData[i]);
    chapter.index = i;
    this.chapters[chapter.file] = chapter;
  }
  
  for (var sectionName in this.chapterSections) {
    var section = this.chapterSections[sectionName];

    var file = section.file;
    var chapter = this.chapters[file];
    if (!chapter)
      window.console.log('failed to find chapter ' + file + ' in chapter data.');
    chapter.sections.push(section);
  }
  
  for (var chapterName in this.chapters) {
    var currChapter = this.chapters[chapterName];
    currChapter.sections.sort();
    
    var testCount = 0;
    for (var s = 0; s < currChapter.sections.length; ++s)
      testCount += currChapter.sections[s].tests.length;
      
    currChapter.testCount = testCount;
  }
}

TestSuite.prototype.indexOfChapter = function(chapter)
{
  for (var i = 0; i < kChapterData.length; ++i) {
    if (kChapterData[i].file == chapter.file)
      return i;
  }
  
  window.console.log('indexOfChapter for ' + chapter.file + ' failed');
  return -1;
}

TestSuite.prototype.chapterAtIndex = function(index)
{
  if (index < 0 || index >= kChapterData.length)
    return null;

  return this.chapters[kChapterData[index].file];
}

TestSuite.prototype.fillChapterPopup = function()
{
  var select = document.getElementById('chapters')
  select.innerHTML = ''; // Remove all children.
  
  for (var i = 0; i < kChapterData.length; ++i) {
    var chapterData = kChapterData[i];
    var chapter = this.chapters[chapterData.file];
    
    var option = document.createElement('option');
    option.innerText = chapter.description();
    option._chapter = chapter;
    
    select.appendChild(option);
  }
}

TestSuite.prototype.updateChapterPopup = function()
{
  var select = document.getElementById('chapters')
  var currOption = select.firstChild;
  
  for (var i = 0; i < kChapterData.length; ++i) {
    var chapterData = kChapterData[i];
    var chapter = this.chapters[chapterData.file];

    currOption.innerText = chapter.description();
    currOption = currOption.nextSibling;
  }
}

TestSuite.prototype.buildTestListForChapter = function(chapter)
{
  this.currentChapterTests = this.testListForChapter(chapter);
}

TestSuite.prototype.testListForChapter = function(chapter)
{
  var testList = [];
  
  for (var i in chapter.sections) {
    var currSection = chapter.sections[i];
    // FIXME: why do I need the assignment?
    testList = testList.concat(currSection.tests);
  }
  
  // FIXME: test may occur more than once.
  testList.sort(function(a, b) {
    return a.id.localeCompare(b.id);
  });
  
  return testList;
}

TestSuite.prototype.initializeControls = function()
{
  var chaptersPopup = document.getElementById('chapters');

  var _self = this;
  chaptersPopup.addEventListener('change', function() {
    _self.chapterPopupChanged();
  }, false);

  this.chapterPopupChanged();
  
  // Results popup
  var resultsPopup = document.getElementById('results-popup');
  resultsPopup.innerHTML = '';
  
  for (var i = 0; i < kResultsSelector.length; ++i) {
    var option = document.createElement('option');
    option.innerText =  kResultsSelector[i].name;
    
    resultsPopup.appendChild(option);
  }
}

TestSuite.prototype.chapterPopupChanged = function()
{
  var chaptersPopup = document.getElementById('chapters');
  var selectedChapter = chaptersPopup.options[chaptersPopup.selectedIndex]._chapter;

  this.setSelectedChapter(selectedChapter);
}

TestSuite.prototype.fillTestList = function()
{
  var testList = document.getElementById('test-list');
  testList.innerHTML = '';
  
  for (var i = 0; i < this.currentChapterTests.length; ++i) {
    var currTest = this.currentChapterTests[i];

    var option = document.createElement('option');
    option.innerText = currTest.id;
    option.className = currTest.completed ? 'completed' : 'untested';
    option._test = currTest;
    testList.appendChild(option);
  }
}

TestSuite.prototype.updateTestList = function()
{
  var testList = document.getElementById('test-list');
  
  var options = testList.getElementsByTagName('option');
  for (var i = 0; i < options.length; ++i) {
    var currOption = options[i];
    currOption.className = currOption._test.completed ? 'completed' : 'untested';
  }
}

TestSuite.prototype.setSelectedChapter = function(chapter)
{
  this.currentChapter = chapter;
  this.buildTestListForChapter(this.currentChapter);
  this.currChapterTestIndex = -1;
  
  this.fillTestList();
  this.goToTestIndex(0);
  
  var chaptersPopup = document.getElementById('chapters');
  chaptersPopup.selectedIndex = this.indexOfChapter(chapter);
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
  if (this.currChapterTestIndex < this.currentChapterTests.length - 1)
    this.goToTestIndex(this.currChapterTestIndex + 1);
  else {
    var currChapterIndex = this.indexOfChapter(this.currentChapter);
    this.goToChapterIndex(currChapterIndex + 1);
  }
}

TestSuite.prototype.previousTest = function()
{
  if (this.currChapterTestIndex > 0)
    this.goToTestIndex(this.currChapterTestIndex - 1);
  else {
    var currChapterIndex = this.indexOfChapter(this.currentChapter);
    if (currChapterIndex > 0)
      this.goToChapterIndex(currChapterIndex - 1);
  }
}

TestSuite.prototype.goToNextIncompleteTest = function()
{
  // Look to the end of this chapter.
  for (var i = this.currChapterTestIndex + 1; i < this.currentChapterTests.length; ++i) {
    if (!this.currentChapterTests[i].completed) {
      this.goToTestIndex(i);
      return;
    }
  }

  // Start looking through later chapter
  var currChapterIndex = this.indexOfChapter(this.currentChapter);
  for (var c = currChapterIndex + 1; c < kChapterData.length; ++c) {
    var chapterData = this.chapterAtIndex(c);
    
    var testIndex = this.firstIncompleteTestIndex(chapterData);
    if (testIndex != -1) {
      this.goToChapterIndex(c);
      this.goToTestIndex(testIndex);
      break;
    }
  }
}

TestSuite.prototype.firstIncompleteTestIndex = function(chapter)
{
  var chapterTests = this.testListForChapter(chapter);
  for (var i = 0; i < chapterTests.length; ++i) {
    if (!chapterTests[i].completed)
      return i;
  }
  
  return -1;
}

/* ------------------------------------------------------- */

TestSuite.prototype.goToTestIndex = function(index)
{
  if (index >= 0 && index < this.currentChapterTests.length) {
    this.currChapterTestIndex = index;
    this.loadCurrentTest();
  }
}

TestSuite.prototype.goToChapterIndex = function(chapterIndex)
{
  if (chapterIndex >= 0 && chapterIndex < kChapterData.length) {
    var chapterFile = kChapterData[chapterIndex].file;
    this.setSelectedChapter(this.chapters[chapterFile]);
  }
}

TestSuite.prototype.currentTestName = function()
{
  if (this.currChapterTestIndex < 0 || this.currChapterTestIndex >= this.currentChapterTests.length)
    return undefined;

  return this.currentChapterTests[this.currChapterTestIndex].id;
}

TestSuite.prototype.loadCurrentTest = function()
{
  var theTest = this.currentChapterTests[this.currChapterTestIndex];
  if (!theTest) {
    this.configureForManualTest();
    this.clearTest();
    return;
  }

  if (theTest.reference) {
    this.configureForRefTest();
    this.loadRef(theTest);
  } else {
    this.configureForManualTest();
  }

  this.loadTest(theTest);

  document.getElementById('test-index').innerText = this.currChapterTestIndex + 1;
  document.getElementById('chapter-test-count').innerText = this.currentChapterTests.length;
  
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

TestSuite.prototype.loadTest = function(test)
{
  var iframe = document.getElementById('test-frame');
  iframe.src = this.urlForTest(test.id);
  
  document.getElementById('test-title').innerText = test.title;
  document.getElementById('test-url').innerText = this.pathForTest(test.id);
  document.getElementById('test-assertion').innerText = test.assertion;
  document.getElementById('test-flags').innerText = test.flags;
  
  this.processFlags(test);
}

TestSuite.prototype.processFlags = function(test)
{ 
  var isPaged = test.flags.indexOf('paged') != -1;
  if (isPaged)
    $('#test-content').addClass('print');
  else
    $('#test-content').removeClass('print');

  var showWarning = false;
  var warning = '';
  if (test.flags.indexOf('font') != -1)
    warning = 'Requires a specific font to be installed.';
  
  if (test.flags.indexOf('http') != -1) {
    if (warning != '')
      warning += ' ';
    warning += 'Must be tested over HTTP, with custom HTTP headers.';
  }
  
  if (isPaged) {
    if (warning != '')
      warning += ' ';
    warning += 'Test via the browser\'s Print Preview.';
  }

  document.getElementById('warning').innerText = warning;

  if (warning.length > 0)
    $('#test-content').addClass('warn');
  else
    $('#test-content').removeClass('warn');

}

TestSuite.prototype.clearTest = function()
{
  var iframe = document.getElementById('test-frame');
  iframe.src = 'about:blank';
  
  document.getElementById('test-title').innerText = '';
  document.getElementById('test-url').innerText = '';
  document.getElementById('test-assertion').innerText = '';
  document.getElementById('test-flags').innerText = '';

  $('#test-content').removeClass('print');
  $('#test-content').removeClass('warn');
  document.getElementById('warning').innerText = '';
}

TestSuite.prototype.loadRef = function(test)
{
  // Refs are always .xht files
  var refURL = kTestSuiteHome + kXHTML1Data.path + '/' + test.reference;
  
  var iframe = document.getElementById('ref-frame');
  iframe.src = refURL;
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
  this.markTestCompleted(testName);
  this.updateTestList();

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

/* ------------------------- Import ------------------------------- */
/*
  Import format is the same as the export format, namely:
  
  testname<tab>result

  with optional trailing <tab>comment.

html4/absolute-non-replaced-height-002<tab>pass
xhtml1/absolute-non-replaced-height-002<tab>?
  
  Lines starting with # are ignored.
  The "testname<tab>result" line is ignored.
*/
TestSuite.prototype.importResults = function(data)
{
  var testsToImport = [];

  var lines = data.split('\n');
  for (var i = 0; i < lines.length; ++i) {
    var currLine = lines[i];
    if (currLine.length == 0 || currLine.charAt(0) == '#')
      continue;

    var match = currLine.match(/^(html4|xhtml1)\/([\w-_]+)\t([\w?]+)\t?(.+)?$/);
    if (match) {
      var test = { 'id' : match[2] };
      test.format =  match[1];
      test.result = match[3];
      test.comment = match[4];
      
      if (test.result != '?')
        testsToImport.push(test);
    } else {
      window.console.log('failed to match line \'' + currLine + '\'');
    }
  }

  this.importTestResults(testsToImport);
  
  this.resetTestStatus();
  this.updateSummaryData();
}



/* --------------------- Clear Results --------------------------- */
/*
  Clear results format is either same as the export format, or
  a list of bare test IDs (e.g. absolute-non-replaced-height-001)
  in which case both HTML4 and XHTML1 results are cleared.
*/
TestSuite.prototype.clearResults = function(data)
{
  var testsToClear = [];

  var lines = data.split('\n');
  for (var i = 0; i < lines.length; ++i) {
    var currLine = lines[i];
    if (currLine.length == 0 || currLine.charAt(0) == '#')
      continue;

    // Look for format/test with possible extension
    var result = currLine.match(/^((html4|xhtml1)?)\/?([\w-_]+)/);
    if (result) {
      var testId = result[3];
      var format = result[1];
      
      var clearHTML = format.length == 0 || format == 'html4';
      var clearXHTML = format.length == 0 || format == 'xhtml1';
      
      var result = { 'id' : testId };
      result.clearHTML = clearHTML;
      result.clearXHTML = clearXHTML;

      testsToClear.push(result);
    } else {
      window.console.log('failed to match line ' + currLine);
    }
  }
  
  this.clearTestResults(testsToClear);
  
  this.resetTestStatus();
  this.updateSummaryData();
}

/* -------------------------------------------------------- */

TestSuite.prototype.exportResultsCompletion = function(exportTests)
{
  // Lame workaround for ORDER BY not working
  exportTests.sort(function(a, b) {
      return a.test.localeCompare(b.test);
  });
  
  var exportLines = [];
  for (var i = 0; i < exportTests.length; ++i) {
    var currTest = exportTests[i];
    if (currTest.html4 != '')
      exportLines.push(currTest.html4);
    if (currTest.xhtml1 != '')
      exportLines.push(currTest.xhtml1);
  }
  
  var exportString = this.exportHeader() + exportLines.join('\n');
  this.exportQueryComplete(exportString);
}

/* -------------------------------------------------------- */

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

TestSuite.prototype.exportResultsForCompletedTests = function()
{
  var exportTests = []; // each test will have html and xhtml items on it

  var _self = this;
  this.queryDatabaseForCompletedTests(
      function(item) {
        var htmlLine = '';
        if (item.hstatus)
          htmlLine= _self.createExportLine(kHTML4Data, item.test, item.hstatus, item.hcomment);

        var xhtmlLine = '';
        if (item.xstatus)
          xhtmlLine = _self.createExportLine(kXHTML1Data, item.test, item.xstatus, item.xcomment);

        exportTests.push({
          'test' : item.test,
          'html4' : htmlLine,
          'xhtml1' : xhtmlLine });
      },
      function() {
        _self.exportResultsCompletion(exportTests);
      }
    );
}


/* -------------------------------------------------------- */

TestSuite.prototype.showResultsForAllTests = function()
{
  this.beginAppendingOutput();

  var _self = this;
  this.queryDatabaseForAllTests('test',
    function(item) {
      _self.appendResultToOutput(kHTML4Data, item.test, item.hstatus, item.hcomment);
      _self.appendResultToOutput(kXHTML1Data, item.test, item.xstatus, item.xcomment);
    },
    function() {
      _self.endAppendingOutput();
    });
}

TestSuite.prototype.exportResultsForAllTests = function()
{
  var exportTests = [];

  var _self = this;
  this.queryDatabaseForAllTests('test',
      function(item) {
        var htmlLine= _self.createExportLine(kHTML4Data, item.test, item.hstatus ? item.hstatus : '?', item.hcomment);
        var xhtmlLine = _self.createExportLine(kXHTML1Data, item.test, item.xstatus ? item.xstatus : '?', item.xcomment);
        exportTests.push({
          'test' : item.test,
          'html4' : htmlLine,
          'xhtml1' : xhtmlLine });
      },
      function() {
        _self.exportResultsCompletion(exportTests);
      }
    );
}

/* -------------------------------------------------------- */

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

TestSuite.prototype.exportResultsForTestsNotRun = function()
{
  var exportTests = [];

  var _self = this;
  this.queryDatabaseForTestsNotRun(
      function(item) {
        var htmlLine = '';
        if (!item.hstatus)
          htmlLine= _self.createExportLine(kHTML4Data, item.test, '?', item.hcomment);

        var xhtmlLine = '';
        if (!item.xstatus)
          xhtmlLine = _self.createExportLine(kXHTML1Data, item.test, '?', item.xcomment);

        exportTests.push({
          'test' : item.test,
          'html4' : htmlLine,
          'xhtml1' : xhtmlLine });
      },
      function() {
        _self.exportResultsCompletion(exportTests);
      }
    );
}

/* -------------------------------------------------------- */

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

TestSuite.prototype.exportResultsForTestsWithStatus = function(status)
{
  var exportTests = [];

  var _self = this;
  this.queryDatabaseForTestsWithStatus(status,
      function(item) {
        var htmlLine = '';
        if (item.hstatus == status)
          htmlLine= _self.createExportLine(kHTML4Data, item.test, item.hstatus, item.hcomment);

        var xhtmlLine = '';
        if (item.xstatus == status)
          xhtmlLine = _self.createExportLine(kXHTML1Data, item.test, item.xstatus, item.xcomment);

        exportTests.push({
          'test' : item.test,
          'html4' : htmlLine,
          'xhtml1' : xhtmlLine });
      },
      function() {
        _self.exportResultsCompletion(exportTests);
      }
    );
}

/* -------------------------------------------------------- */

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

TestSuite.prototype.exportResultsForTestsWithMismatchedResults = function()
{
  var exportTests = [];

  var _self = this;
  this.queryDatabaseForTestsWithMixedStatus(
      function(item) {
        var htmlLine= _self.createExportLine(kHTML4Data, item.test, item.hstatus ? item.hstatus : '?', item.hcomment);
        var xhtmlLine = _self.createExportLine(kXHTML1Data, item.test, item.xstatus ? item.xstatus : '?', item.xcomment);
        exportTests.push({
          'test' : item.test,
          'html4' : htmlLine,
          'xhtml1' : xhtmlLine });
      },
      function() {
        _self.exportResultsCompletion(exportTests);
      }
    );
}

/* -------------------------------------------------------- */

TestSuite.prototype.markTestCompleted = function(testID)
{
  var test = this.tests[testID];
  if (!test) {
    window.console.log('markTestCompleted failed to find test ' + testID);
    return;
  }
  
  test.completed = true;
}

TestSuite.prototype.testCompletionStateChanged = function()
{
  this.updateTestList();
  this.updateChapterPopup();
}

TestSuite.prototype.loadTestStatus = function()
{
  var _self = this;
  this.queryDatabaseForCompletedTests(
      function(item) {
        _self.markTestCompleted(item.test);
      },
      function() {
        _self.testCompletionStateChanged();
      }
    );
    
    this.updateChapterPopup();
}

TestSuite.prototype.resetTestStatus = function()
{
  for (var testID in this.tests) {
    var currTest = this.tests[testID];
    currTest.completed = false;
  }
  this.loadTestStatus();
}

/* -------------------------------------------------------- */

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
  alert('Database error: ' + error.message);
  window.console.log('Database error: ' + error.message);
}

TestSuite.prototype.openDatabase = function()
{
  if (!'openDatabase' in window) {
    alert('Your browser does not support client-side SQL databases, so results will not be stored.');
    return;
  }
  
  var _self = this;
  this.db = window.openDatabase('css21testsuite', '1.0', 'CSS 2.1 test suite results', 10 * 1024 * 1024, function() {
    _self.databaseCreated();
  }, errorHandler);

  this.updateSummaryData();
  this.loadTestStatus();
}

TestSuite.prototype.databaseCreated = function(db)
{
  this.populatingDatabase = true;

  var _self = this;
  this.db.transaction(function (tx) {
    // hstatus: HTML4 result
    // xstatus: XHTML1 result
    tx.executeSql('CREATE TABLE tests (test PRIMARY KEY UNIQUE, ref, title, flags, links, assertion, hstatus, hcomment, xstatus, xcomment)', null,
      function(tx, results) {
        _self.populateDatabaseFromTestInfoData();
      }, errorHandler);
  });
}

TestSuite.prototype.storeTestResult = function(test, format, result, comment, useragent)
{
  if (!this.db)
    return;

  this.db.transaction(function (tx) {
    if (format == 'html4')
      tx.executeSql('UPDATE tests SET hstatus=?, hcomment=? WHERE test=?\n', [result, comment, test], null, errorHandler);
    else if (format == 'xhtml1')
      tx.executeSql('UPDATE tests SET xstatus=?, xcomment=? WHERE test=?\n', [result, comment, test], null, errorHandler);
  });
}

TestSuite.prototype.importTestResults = function(results)
{
  if (!this.db)
    return;

  this.db.transaction(function (tx) {

    for (var i = 0; i < results.length; ++i) {
      var currResult = results[i];

      var query;
      if (currResult.format == 'html4')
        query = 'UPDATE tests SET hstatus=?, hcomment=? WHERE test=?\n';
      else if (currResult.format == 'xhtml1')
        query = 'UPDATE tests SET xstatus=?, xcomment=? WHERE test=?\n';

      tx.executeSql(query, [currResult.result, currResult.comment, currResult.id], null, errorHandler);
    }
  });
}

TestSuite.prototype.clearTestResults = function(results)
{
  if (!this.db)
    return;

  this.db.transaction(function (tx) {
    
    for (var i = 0; i < results.length; ++i) {
      var currResult = results[i];

      if (currResult.clearHTML)
        tx.executeSql('UPDATE tests SET hstatus=NULL, hcomment=NULL WHERE test=?\n', [currResult.id], null, errorHandler);

      if (currResult.clearXHTML)
        tx.executeSql('UPDATE tests SET xstatus=NULL, xcomment=NULL WHERE test=?\n', [currResult.id], null, errorHandler);
      
    }
  });
}

TestSuite.prototype.populateDatabaseFromTestInfoData = function(testInfoURL)
{
  if (!this.testInfoLoaded) {
    window.console.log('Tring to populate database before testinfo.data has been loaded');
    return;
  }
  
  var _self = this;
  this.db.transaction(function (tx) {
    for (var testID in _self.tests) {
      var currTest = _self.tests[testID];
      tx.executeSql('INSERT INTO tests (test, ref, title, flags, links, assertion) VALUES (?, ?, ?, ?, ?, ?)', [currTest.id, currTest.reference, currTest.title, currTest.flags, currTest.links, currTest.assertion], null, errorHandler);
    }

    _self.populatingDatabase = false;
  });

}

TestSuite.prototype.queryDatabaseForAllTests = function(sortKey, perRowHandler, completionHandler)
{
  if (this.populatingDatabase)
    return;

  var _self = this;
  this.db.transaction(function (tx) {
    if (_self.populatingDatabase)
      return;
    var query;
    var args = [];
    if (sortKey != '') {
      query = 'SELECT * FROM tests ORDER BY ? ASC';  // ORDER BY doesn't seem to work
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
  if (this.populatingDatabase)
    return;

  var _self = this;
  this.db.transaction(function (tx) {
    if (_self.populatingDatabase)
      return;
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
  if (this.populatingDatabase)
    return;

  var _self = this;
  this.db.transaction(function (tx) {
    if (_self.populatingDatabase)
      return;
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
  if (this.populatingDatabase)
    return;

  var _self = this;
  this.db.transaction(function (tx) {
    
    if (_self.populatingDatabase)
      return;

    tx.executeSql('SELECT * FROM tests WHERE hstatus IS NOT NULL OR xstatus IS NOT NULL', [], function(tx, results) {
      var len = results.rows.length;
      for (var i = 0; i < len; ++i)
        perRowHandler(results.rows.item(i));
      
      completionHandler();
    }, errorHandler);
  });  
}

TestSuite.prototype.queryDatabaseForTestsNotRun = function(perRowHandler, completionHandler)
{
  if (this.populatingDatabase)
    return;

  var _self = this;
  this.db.transaction(function (tx) {
    if (_self.populatingDatabase)
      return;

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
  if (!this.db || this.populatingDatabase)
    return;

  var _self = this;
  this.db.transaction(function (tx) {

    if (_self.populatingDatabase)
      return;

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

