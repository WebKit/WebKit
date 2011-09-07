/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

var ui = ui || {};
ui.results = ui.results || {};

(function(){

var kResultsPrefetchDelayMS = 500;

// FIXME: Rather than using table, should we be using something fancier?
ui.results.Comparison = base.extends('table', {
    init: function()
    {
        this.className = 'comparison';
        this.innerHTML = '<thead><tr><th>Expected</th><th>Actual</th><th>Diff</th></tr></thead>' +
                         '<tbody><tr><td class="expected"></td><td class="actual"></td><td class="diff"></td></tr></tbody>';
    },
    _selectorForKind: function(kind)
    {
        switch (kind) {
        case results.kExpectedKind:
            return '.expected';
        case results.kActualKind:
            return '.actual';
        case results.kDiffKind:
            return '.diff';
        }
        return '.unknown';
    },
    update: function(kind, result)
    {
        var selector = this._selectorForKind(kind);
        $(selector, this).empty().append(result);
        return result;
    },
});

// We'd really like TextResult and ImageResult to extend a common Result base
// class, but we can't seem to do that because they inherit from different
// HTMLElements. We could have them inherit from <div>, but that seems lame.

ui.results.TextResult = base.extends('iframe', {
    init: function(url)
    {
        this.className = 'text-result';
        this.src = url;
    }
});

ui.results.ImageResult = base.extends('img', {
    init: function(url)
    {
        this.className = 'image-result';
        this.src = url;
    }
});

function constructorForResultType(type)
{
    return (type == results.kImageType) ? ui.results.ImageResult : ui.results.TextResult;
}

ui.results.ResultsGrid = base.extends('div', {
    init: function()
    {
        this.className = 'results-grid';
    },
    _addResult: function(comparison, constructor, resultsURLsByKind, kind)
    {
        var url = resultsURLsByKind[kind];
        if (!url)
            return;
        comparison.update(kind, new constructor(url));
    },
    addComparison: function(resultType, resultsURLsByKind)
    {
        var comparison = new ui.results.Comparison();
        var constructor = constructorForResultType(resultType);

        this._addResult(comparison, constructor, resultsURLsByKind, results.kExpectedKind);
        this._addResult(comparison, constructor, resultsURLsByKind, results.kActualKind);
        this._addResult(comparison, constructor, resultsURLsByKind, results.kDiffKind);

        this.appendChild(comparison);
        return comparison;
    },
    addRow: function(resultType, url)
    {
        var constructor = constructorForResultType(resultType);
        var view = new constructor(url);
        this.appendChild(view);
        return view;
    },
    addResults: function(resultsURLs)
    {
        var resultsURLsByTypeAndKind = {};

        resultsURLsByTypeAndKind[results.kImageType] = {};
        resultsURLsByTypeAndKind[results.kTextType] = {};

        resultsURLs.forEach(function(url) {
            resultsURLsByTypeAndKind[results.resultType(url)][results.resultKind(url)] = url;
        });

        $.each(resultsURLsByTypeAndKind, function(resultType, resultsURLsByKind) {
            if ($.isEmptyObject(resultsURLsByKind))
                return;
            if (results.kUnknownKind in resultsURLsByKind) {
                // This is something like "crash" that isn't a comparison.
                this.addRow(resultType, resultsURLsByKind[results.kUnknownKind]);
                return;
            }
            this.addComparison(resultType, resultsURLsByKind);
        }.bind(this));
    }
});

ui.results.ResultsDetails = base.extends('div', {
    init: function(delegate, failureInfo)
    {
        this.className = 'results-detail';
        this._delegate = delegate;
        this._failureInfo = failureInfo;
        this._haveShownOnce = false;
    },
    show: function() {
        if (this._haveShownOnce)
            return;
        this._haveShownOnce = true;
        this._delegate.fetchResultsURLs(this._failureInfo, function(resultsURLs) {
            var resultsGrid = new ui.results.ResultsGrid();
            resultsGrid.addResults(resultsURLs);
            $(this).empty().append(
                new ui.actions.List([
                    new ui.actions.Rebaseline().makeDefault(),
                    new ui.actions.Previous(),
                    new ui.actions.Next()
                ])).append(resultsGrid);
        }.bind(this));
    },
});

ui.results.TestSelector = base.extends('div', {
    init: function(delegate, resultsByTest)
    {
        this.className = 'test-selector';
        this._delegate = delegate;
        this._length = 0;

        Object.keys(resultsByTest).forEach(function (testName) {
            var link = document.createElement('a');
            $(link).attr('href', '#').text(testName);
            this.appendChild(document.createElement('h3')).appendChild(link);
            this.appendChild(this._delegate.contentForTest(testName));
            ++this._length; // There doesn't seem to be any good way to get this information from accordion.
        }, this);

        $(this).accordion({
            collapsible: true,
            autoHeight: false,
        });
        $(this).accordion('activate', false);
    },
    nextResult: function()
    {
        var activeIndex = $(this).accordion('option', 'active');
        if ($('.builder-selector', this)[activeIndex].nextResult())
            return true;
        return this.nextTest();
    },
    previousResult: function()
    {
        var activeIndex = $(this).accordion('option', 'active');
        if ($('.builder-selector', this)[activeIndex].previousResult())
            return true;
        return this.previousTest();
    },
    nextTest: function()
    {
        var nextIndex = $(this).accordion('option', 'active') + 1;
        if (nextIndex >= this._length) {
            $(this).accordion('option', 'active', false);
            return false;
        }
        $(this).accordion('option', 'active', nextIndex);
        $('.builder-selector', this)[nextIndex].firstResult();
        return true;
    },
    previousTest: function()
    {
        var previousIndex = $(this).accordion('option', 'active') - 1;
        if (previousIndex < 0) {
            $(this).accordion('option', 'active', false);
            return false;
        }
        $(this).accordion('option', 'active', previousIndex);
        $('.builder-selector', this)[previousIndex].lastResult();
        return true;
    },
    firstResult: function()
    {
        $(this).accordion('option', 'active', 0);
        $('.builder-selector', this)[0].firstResult();
    }
});

ui.results.BuilderSelector = base.extends('div', {
    init: function(delegate, testName, resultsByBuilder)
    {
        this.className = 'builder-selector';
        this._delegate = delegate;

        var tabStrip = this.appendChild(document.createElement('ul'));

        Object.keys(resultsByBuilder).forEach(function(builderName) {
            var builderDirectory = results.directoryForBuilder(builderName);

            var link = document.createElement('a');
            $(link).attr('href', "#" + builderDirectory).text(ui.displayNameForBuilder(builderName));
            tabStrip.appendChild(document.createElement('li')).appendChild(link);

            var content = this._delegate.contentForTestAndBuilder(testName, builderName);
            content.id = builderDirectory;
            this.appendChild(content);
        }, this);

        $(this).tabs();
    },
    nextResult: function()
    {
        var nextIndex = $(this).tabs('option', 'selected') + 1;
        if (nextIndex >= $(this).tabs('length'))
            return false
        $(this).tabs('option', 'selected', nextIndex);
        return true;
    },
    previousResult: function()
    {
        var previousIndex = $(this).tabs('option', 'selected') - 1;
        if (previousIndex < 0)
            return false;
        $(this).tabs('option', 'selected', previousIndex);
        return true;
    },
    firstResult: function()
    {
        $(this).tabs('option', 'selected', 0);
    },
    lastResult: function()
    {
        $(this).tabs('option', 'selected', $(this).tabs('length') - 1);
    }
});

ui.results.View = base.extends('div', {
    init: function(delegate)
    {
        this.className = 'results-view';
        this._delegate = delegate;
    },
    contentForTest: function(testName)
    {
        var builderSelector = new ui.results.BuilderSelector(this, testName, this._resultsByTest[testName]);
        $(builderSelector).bind('tabsselect', function(event, ui) {
            // We will probably have pre-fetched the tab already, but we need to make sure.
            ui.panel.show();
        });
        return builderSelector;
    },
    contentForTestAndBuilder: function(testName, builderName)
    {
        var failureInfo = results.failureInfoForTestAndBuilder(this._resultsByTest, testName, builderName);
        return new ui.results.ResultsDetails(this, failureInfo);
    },
    setResultsByTest: function(resultsByTest)
    {
        $(this).empty();
        this._resultsByTest = resultsByTest;
        this._testSelector = new ui.results.TestSelector(this, resultsByTest);
        $(this._testSelector).bind("accordionchangestart", function(event, ui) {
            // Prefetch the first results from the network.
            var resultsDetails = $('.results-detail', ui.newContent);
            if (resultsDetails.length)
                resultsDetails[0].show();
            // Prefetch the rest kResultsPrefetchDelayMS later.
            setTimeout(function() {
                resultsDetails.each(function() {
                    this.show();
                });
            }, kResultsPrefetchDelayMS);
        });
        this.appendChild(this._testSelector);
    },
    fetchResultsURLs: function(failureInfo, callback)
    {
        this._delegate.fetchResultsURLs(failureInfo, callback)
    },
    nextResult: function()
    {
        return this._testSelector.nextResult();
    },
    previousResult: function()
    {
        return this._testSelector.previousResult();
    },
    firstResult: function()
    {
        this._testSelector.firstResult()
    }
});

})();
