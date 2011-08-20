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
    init: function(delegate)
    {
        this.className = 'results-detail';
        this._delegate = delegate;
    },
    show: function(failureInfo) {
        this._delegate.fetchResultsURLs(failureInfo, function(resultsURLs) {
            var resultsGrid = new ui.results.ResultsGrid();
            resultsGrid.addResults(resultsURLs);
            $(this).empty().append(resultsGrid);
        }.bind(this));
    },
});

ui.results.TestSelector = base.extends('input', {
    init: function()
    {
        this.className = 'test-selector';
        this.type = 'text';
        this.placeholder = 'Test name';
    },
    setTestList: function(testNameList)
    {
        $(this).autocomplete({
            source: testNameList,
            select: function(event, selected) {
                $(this).trigger('testselected', [selected.item.value]);
            }.bind(this)
        });
    }
});

ui.results.BuilderSelector = base.extends('select', {
    init: function()
    {
        this.className = 'builder-selector';
        $(this).hide();
        $(this).change(function() {
            $(this).trigger('builderselected');
        }.bind(this));
    },
    show: function(builderNameList) {
        $(this).empty();
        $(this).show();
        builderNameList.forEach(function(builderName) {
            var builderElement = document.createElement('option');
            builderElement.textContent = ui.displayNameForBuilder(builderName);
            builderElement.value = builderName;
            this.appendChild(builderElement);
        }.bind(this));
    },
    select: function(builderName) {
        var builderIndex = -1;
        for (var i = 0; i < this.options.length; ++i) {
            if (this.options[i].value == builderName) {
                builderIndex = i;
                break;
            }
        }
        if (builderIndex == -1)
            return;
        this.selectedIndex = builderIndex;
    },
    selectedBuilder: function() {
        if (this.selectedIndex == -1)
            return;
        return this.options[this.selectedIndex].value;
    }
});

ui.results.View = base.extends('div', {
    init: function(delegate)
    {
        this.className = 'results-view';
        this.innerHTML = '<div class="toolbar"><div class="selector"></div></div><div class="content"></div>';

        this._testSelector = new ui.results.TestSelector();
        this._builderSelector = new ui.results.BuilderSelector();
        this._resultsDetails = new ui.results.ResultsDetails(delegate);

        $('.toolbar', this).prepend(new ui.actions.List([
            new ui.actions.Rebaseline().makeDefault(),
            new ui.actions.Previous(),
            new ui.actions.Next(),
        ]));
        $('.selector', this).append(this._testSelector).append(this._builderSelector);
        $('.content', this).append(this._resultsDetails);
    },
    setTestList: function(testNameList)
    {
        this._testSelector.setTestList(testNameList);
    },
    currentTestName: function()
    {
        return this._testSelector.value;
    },
    currentBuilderName: function()
    {
        return this._builderSelector.selectedBuilder();
    },
    setBuilderList: function(buildNameList)
    {
        this._builderSelector.show(buildNameList);
    },
    showResults: function(failureInfo)
    {
        this._testSelector.value = failureInfo.testName;
        this._builderSelector.select(failureInfo.builderName);
        this._resultsDetails.show(failureInfo);
    }
});

})();
