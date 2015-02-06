App.InteractiveChartComponent = Ember.Component.extend({
    chartData: null,
    showXAxis: true,
    showYAxis: true,
    interactive: false,
    enableSelection: true,
    classNames: ['chart'],
    init: function ()
    {
        this._super();
        this._needsConstruction = true;
        this._eventHandlers = [];
        $(window).resize(this._updateDimensionsIfNeeded.bind(this));
    },
    chartDataDidChange: function ()
    {
        var chartData = this.get('chartData');
        if (!chartData)
            return;
        this._needsConstruction = true;
        this._constructGraphIfPossible(chartData);
    }.observes('chartData').on('init'),
    didInsertElement: function ()
    {
        var chartData = this.get('chartData');
        if (chartData)
            this._constructGraphIfPossible(chartData);
    },
    willClearRender: function ()
    {
        this._eventHandlers.forEach(function (item) {
            $(item[0]).off(item[1], item[2]);
        })
    },
    _attachEventListener: function(target, eventName, listener)
    {
        this._eventHandlers.push([target, eventName, listener]);
        $(target).on(eventName, listener);
    },
    _constructGraphIfPossible: function (chartData)
    {
        if (!this._needsConstruction || !this.get('element'))
            return;

        var element = this.get('element');

        this._x = d3.time.scale();
        this._y = d3.scale.linear();

        // FIXME: Tear down the old SVG element.
        this._svgElement = d3.select(element).append("svg")
                .attr("width", "100%")
                .attr("height", "100%");

        var svg = this._svg = this._svgElement.append("g");

        var clipId = element.id + "-clip";
        this._clipPath = svg.append("defs").append("clipPath")
            .attr("id", clipId)
            .append("rect");

        if (this.get('showXAxis')) {
            this._xAxis = d3.svg.axis().scale(this._x).orient("bottom").ticks(6);
            this._xAxisLabels = svg.append("g")
                .attr("class", "x axis");
        }

        if (this.get('showYAxis')) {
            this._yAxis = d3.svg.axis().scale(this._y).orient("left").ticks(6).tickFormat(chartData.formatter);
            this._yAxisLabels = svg.append("g")
                .attr("class", "y axis");
        }

        this._clippedContainer = svg.append("g")
            .attr("clip-path", "url(#" + clipId + ")");

        var xScale = this._x;
        var yScale = this._y;
        this._timeLine = d3.svg.line()
            .x(function(point) { return xScale(point.time); })
            .y(function(point) { return yScale(point.value); });

        this._confidenceArea = d3.svg.area()
//            .interpolate("cardinal")
            .x(function(point) { return xScale(point.time); })
            .y0(function(point) { return point.interval ? yScale(point.interval[0]) : null; })
            .y1(function(point) { return point.interval ? yScale(point.interval[1]) : null; });

        if (this._paths)
            this._paths.forEach(function (path) { path.remove(); });
        this._paths = [];
        if (this._areas)
            this._areas.forEach(function (area) { area.remove(); });
        this._areas = [];
        if (this._dots)
            this._dots.forEach(function (dot) { dots.remove(); });
        this._dots = [];
        if (this._highlights)
            this._highlights.remove();
        this._highlights = null;

        this._currentTimeSeries = chartData.current;
        this._currentTimeSeriesData = this._currentTimeSeries.series();
        this._baselineTimeSeries = chartData.baseline;
        this._targetTimeSeries = chartData.target;

        this._yAxisUnit = chartData.unit;

        if (this._baselineTimeSeries) {
            this._paths.push(this._clippedContainer
                .append("path")
                .datum(this._baselineTimeSeries.series())
                .attr("class", "baseline"));
        }
        if (this._targetTimeSeries) {
            this._paths.push(this._clippedContainer
                .append("path")
                .datum(this._targetTimeSeries.series())
                .attr("class", "target"));
        }

        this._areas.push(this._clippedContainer
            .append("path")
            .datum(this._currentTimeSeriesData)
            .attr("class", "area"));

        this._paths.push(this._clippedContainer
            .append("path")
            .datum(this._currentTimeSeriesData)
            .attr("class", "current"));

        this._dots.push(this._clippedContainer
            .selectAll(".dot")
                .data(this._currentTimeSeriesData)
            .enter().append("circle")
                .attr("class", "dot")
                .attr("r", this.get('chartPointRadius') || 1));

        if (this.get('interactive')) {
            this._attachEventListener(element, "mousemove", this._mouseMoved.bind(this));
            this._attachEventListener(element, "mouseleave", this._mouseLeft.bind(this));
            this._attachEventListener(element, "mousedown", this._mouseDown.bind(this));
            this._attachEventListener($(element).parents("[tabindex]"), "keydown", this._keyPressed.bind(this));

            this._currentItemLine = this._clippedContainer
                .append("line")
                .attr("class", "current-item");

            this._currentItemCircle = this._clippedContainer
                .append("circle")
                .attr("class", "dot current-item")
                .attr("r", 3);
        }

        this._brush = null;
        if (this.get('enableSelection')) {
            this._brush = d3.svg.brush()
                .x(this._x)
                .on("brush", this._brushChanged.bind(this));

            this._brushRect = this._clippedContainer
                .append("g")
                .attr("class", "x brush");
        }

        this._updateDomain();
        this._updateDimensionsIfNeeded();

        // Work around the fact the brush isn't set if we updated it synchronously here.
        setTimeout(this._selectionChanged.bind(this), 0);

        setTimeout(this._selectedItemChanged.bind(this), 0);

        this._needsConstruction = false;

        this._highlightedItemsChanged();
        this._rangesChanged();
    },
    _updateDomain: function ()
    {
        var xDomain = this.get('domain');
        var intrinsicXDomain = this._computeXAxisDomain(this._currentTimeSeriesData);
        if (!xDomain)
            xDomain = intrinsicXDomain;
        var currentDomain = this._x.domain();
        if (currentDomain && App.domainsAreEqual(currentDomain, xDomain))
            return currentDomain;

        var yDomain = this._computeYAxisDomain(xDomain[0], xDomain[1]);
        this._x.domain(xDomain);
        this._y.domain(yDomain);
        return xDomain;
    },
    _updateDimensionsIfNeeded: function (newSelection)
    {
        var element = $(this.get('element'));

        var newTotalWidth = element.width();
        var newTotalHeight = element.height();
        if (this._totalWidth == newTotalWidth && this._totalHeight == newTotalHeight)
            return;

        this._totalWidth = newTotalWidth;
        this._totalHeight = newTotalHeight;

        if (!this._rem)
            this._rem = parseFloat(getComputedStyle(document.documentElement).fontSize);
        var rem = this._rem;

        var padding = 0.5 * rem;
        var margin = {top: padding, right: padding, bottom: padding, left: padding};
        if (this._xAxis)
            margin.bottom += rem;
        if (this._yAxis)
            margin.left += 3 * rem;

        this._margin = margin;
        this._contentWidth = this._totalWidth - margin.left - margin.right;
        this._contentHeight = this._totalHeight - margin.top - margin.bottom;

        this._svg.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

        this._clipPath
            .attr("width", this._contentWidth)
            .attr("height", this._contentHeight);

        this._x.range([0, this._contentWidth]);
        this._y.range([this._contentHeight, 0]);

        if (this._xAxis) {
            this._xAxis.tickSize(-this._contentHeight);
            this._xAxisLabels.attr("transform", "translate(0," + this._contentHeight + ")");
        }

        if (this._yAxis)
            this._yAxis.tickSize(-this._contentWidth);

        if (this._currentItemLine) {
            this._currentItemLine
                .attr("y1", 0)
                .attr("y2", margin.top + this._contentHeight);
        }

        this._relayoutDataAndAxes(this._currentSelection());
    },
    _updateBrush: function ()
    {
        this._brushRect
            .call(this._brush)
        .selectAll("rect")
            .attr("y", 1)
            .attr("height", this._contentHeight - 2);
        this._updateSelectionToolbar();
    },
    _relayoutDataAndAxes: function (selection)
    {
        var timeline = this._timeLine;
        this._paths.forEach(function (path) { path.attr("d", timeline); });

        var confidenceArea = this._confidenceArea;
        this._areas.forEach(function (path) { path.attr("d", confidenceArea); });

        var xScale = this._x;
        var yScale = this._y;
        this._dots.forEach(function (dot) {
            dot
                .attr("cx", function(measurement) { return xScale(measurement.time); })
                .attr("cy", function(measurement) { return yScale(measurement.value); });
        });
        this._updateHighlightPositions();
        this._updateRangeBarRects();

        if (this._brush) {
            if (selection)
                this._brush.extent(selection);
            else
                this._brush.clear();
            this._updateBrush();
        }

        this._updateCurrentItemIndicators();

        if (this._xAxis)
            this._xAxisLabels.call(this._xAxis);
        if (!this._yAxis)
            return;

        this._yAxisLabels.call(this._yAxis);
        if (this._yAxisUnitContainer)
            this._yAxisUnitContainer.remove();
        var x = - 3.2 * this._rem;
        var y = this._contentHeight / 2;
        this._yAxisUnitContainer = this._yAxisLabels.append("text")
            .attr("transform", "rotate(90 0 0) translate(" + y + ", " + (-x) + ")")
            .style("text-anchor", "middle")
            .text(this._yAxisUnit);
    },
    _updateHighlightPositions: function () {
        if (!this._highlights)
            return;

        var xScale = this._x;
        var yScale = this._y;
        this._highlights
            .attr("cy", function(point) { return yScale(point.value); })
            .attr("cx", function(point) { return xScale(point.time); });
    },
    _computeXAxisDomain: function (timeSeries)
    {
        var extent = d3.extent(timeSeries, function(point) { return point.time; });
        var margin = 3600 * 1000; // Use x.inverse to compute the right amount from a margin.
        return [+extent[0] - margin, +extent[1] + margin];
    },
    _computeYAxisDomain: function (startTime, endTime)
    {
        var range = this._minMaxForAllTimeSeries(startTime, endTime);
        var min = range[0];
        var max = range[1];
        if (max < min)
            min = max = 0;
        var diff = max - min;
        var margin = diff * 0.05;

        yExtent = [min - margin, max + margin];
//        if (yMin !== undefined)
//            yExtent[0] = parseInt(yMin);
        return yExtent;
    },
    _minMaxForAllTimeSeries: function (startTime, endTime)
    {
        var currentRange = this._currentTimeSeries.minMaxForTimeRange(startTime, endTime);
        var baselineRange = this._baselineTimeSeries ? this._baselineTimeSeries.minMaxForTimeRange(startTime, endTime) : [Number.MAX_VALUE, Number.MIN_VALUE];
        var targetRange = this._targetTimeSeries ? this._targetTimeSeries.minMaxForTimeRange(startTime, endTime) : [Number.MAX_VALUE, Number.MIN_VALUE];
        return [
            Math.min(currentRange[0], baselineRange[0], targetRange[0]),
            Math.max(currentRange[1], baselineRange[1], targetRange[1]),
        ];
    },
    _currentSelection: function ()
    {
        return this._brush && !this._brush.empty() ? this._brush.extent() : null;
    },
    _domainChanged: function ()
    {
        var selection = this._currentSelection() || this.get('sharedSelection');
        var newXDomain = this._updateDomain();

        if (selection && newXDomain && selection[0] <= newXDomain[0] && newXDomain[1] <= selection[1])
            selection = null; // Otherwise the user has no way of clearing the selection.

        this._relayoutDataAndAxes(selection);
    }.observes('domain'),
    _selectionChanged: function ()
    {
        this._updateSelection(this.get('selection'));
    }.observes('selection'),
    _updateSelection: function (newSelection)
    {
        if (!this._brush)
            return;

        var currentSelection = this._currentSelection();
        if (newSelection && currentSelection && App.domainsAreEqual(newSelection, currentSelection))
            return;

        var domain = this._x.domain();
        if (!newSelection || App.domainsAreEqual(domain, newSelection))
            this._brush.clear();
        else
            this._brush.extent(newSelection);
        this._updateBrush();

        this._setCurrentSelection(newSelection);
    },
    _brushChanged: function ()
    {
        if (this._brush.empty()) {
            if (!this._brushExtent)
                return;

            this.set('selectionIsLocked', false);
            this._setCurrentSelection(undefined);

            // Avoid locking the indicator in _mouseDown when the brush was cleared in the same mousedown event.
            this._brushJustChanged = true;
            var self = this;
            setTimeout(function () {
                self._brushJustChanged = false;
            }, 0);

            return;
        }

        this.set('selectionIsLocked', true);
        this._setCurrentSelection(this._brush.extent());
    },
    _keyPressed: function (event)
    {
        if (!this._currentItemIndex || this._currentSelection())
            return;

        var newIndex;
        switch (event.keyCode) {
        case 37: // Left
            newIndex = this._currentItemIndex - 1;
            break;
        case 39: // Right
            newIndex = this._currentItemIndex + 1;
            break;
        case 38: // Up
        case 40: // Down
        default:
            return;
        }

        // Unlike mousemove, keydown shouldn't move off the edge.
        if (this._currentTimeSeriesData[newIndex])
            this._setCurrentItem(newIndex);
    },
    _mouseMoved: function (event)
    {
        if (!this._margin || this._currentSelection() || this._currentItemLocked)
            return;

        var point = this._mousePointInGraph(event);

        this._selectClosestPointToMouseAsCurrentItem(point);
    },
    _mouseLeft: function (event)
    {
        if (!this._margin || this._currentItemLocked)
            return;

        this._selectClosestPointToMouseAsCurrentItem(null);
    },
    _mouseDown: function (event)
    {
        if (!this._margin || this._currentSelection() || this._brushJustChanged)
            return;

        var point = this._mousePointInGraph(event);
        if (!point)
            return;

        if (this._currentItemLocked) {
            this._currentItemLocked = false;
            this.set('selectedItem', null);
            return;
        }

        this._currentItemLocked = true;
        this._selectClosestPointToMouseAsCurrentItem(point);
    },
    _mousePointInGraph: function (event)
    {
        var offset = $(this.get('element')).offset();
        if (!offset || !$(event.target).closest('svg').length)
            return null;

        var point = {
            x: event.pageX - offset.left - this._margin.left,
            y: event.pageY - offset.top - this._margin.top
        };

        var xScale = this._x;
        var yScale = this._y;
        var xDomain = xScale.domain();
        var yDomain = yScale.domain();
        if (point.x >= xScale(xDomain[0]) && point.x <= xScale(xDomain[1])
            && point.y <= yScale(yDomain[0]) && point.y >= yScale(yDomain[1]))
            return point;

        return null;
    },
    _selectClosestPointToMouseAsCurrentItem: function (point)
    {
        var xScale = this._x;
        var yScale = this._y;
        var distanceHeuristics = function (m) {
            var mX = xScale(m.time);
            var mY = yScale(m.value);
            var xDiff = mX - point.x;
            var yDiff = mY - point.y;
            return xDiff * xDiff + yDiff * yDiff / 16; // Favor horizontal affinity.
        };
        distanceHeuristics = function (m) {
            return Math.abs(xScale(m.time) - point.x);
        }

        var newItemIndex;
        if (point && !this._currentSelection()) {
            var distances = this._currentTimeSeriesData.map(distanceHeuristics);
            var minDistance = Number.MAX_VALUE;
            for (var i = 0; i < distances.length; i++) {
                if (distances[i] < minDistance) {
                    newItemIndex = i;
                    minDistance = distances[i];
                }
            }
        }

        this._setCurrentItem(newItemIndex);
        this._updateSelectionToolbar();
    },
    _currentTimeChanged: function ()
    {
        if (!this._margin || this._currentSelection() || this._currentItemLocked)
            return

        var currentTime = this.get('currentTime');
        if (currentTime) {
            for (var i = 0; i < this._currentTimeSeriesData.length; i++) {
                var point = this._currentTimeSeriesData[i];
                if (point.time >= currentTime) {
                    this._setCurrentItem(i, /* doNotNotify */ true);
                    return;
                }
            }
        }
        this._setCurrentItem(undefined, /* doNotNotify */ true);
    }.observes('currentTime'),
    _setCurrentItem: function (newItemIndex, doNotNotify)
    {
        if (newItemIndex === this._currentItemIndex) {
            if (this._currentItemLocked)
                this.set('selectedItem', this.get('currentItem') ? this.get('currentItem').measurement.id() : null);
            return;
        }

        var newItem = this._currentTimeSeriesData[newItemIndex];
        this._brushExtent = undefined;
        this._currentItemIndex = newItemIndex;

        if (!newItem) {
            this._currentItemLocked = false;
            this.set('selectedItem', null);
        }

        this._updateCurrentItemIndicators();

        if (!doNotNotify)
            this.set('currentTime', newItem ? newItem.time : undefined);

        this.set('currentItem', newItem);
        if (this._currentItemLocked)
            this.set('selectedItem', newItem ? newItem.measurement.id() : null);
    },
    _selectedItemChanged: function ()
    {
        if (!this._margin)
            return;

        var selectedId = this.get('selectedItem');
        var currentItem = this.get('currentItem');
        if (currentItem && currentItem.measurement.id() == selectedId)
            return;

        var series = this._currentTimeSeriesData;
        var selectedItemIndex = undefined;
        for (var i = 0; i < series.length; i++) {
            if (series[i].measurement.id() == selectedId) {
                this._updateSelection(null);
                this._currentItemLocked = true;
                this._setCurrentItem(i);
                this._updateSelectionToolbar();
                return;
            }
        }
    }.observes('selectedItem').on('init'),
    _highlightedItemsChanged: function () {
        var highlightedItems = this.get('highlightedItems');

        if (!this._clippedContainer || !highlightedItems)
            return;

        var data = this._currentTimeSeriesData.filter(function (item) { return highlightedItems[item.measurement.id()]; });

        if (this._highlights)
            this._highlights.remove();
        this._highlights = this._clippedContainer
            .selectAll(".highlight")
                .data(data)
            .enter().append("circle")
                .attr("class", "highlight")
                .attr("r", (this.get('chartPointRadius') || 1) * 1.8);

        this._updateHighlightPositions();
    }.observes('highlightedItems'),
    _rangesChanged: function ()
    {
        if (!this._currentTimeSeries)
            return;

        function midPoint(firstPoint, secondPoint) {
            if (firstPoint && secondPoint)
                return (+firstPoint.time + +secondPoint.time) / 2;
            if (firstPoint)
                return firstPoint.time;
            return secondPoint.time;
        }
        var currentTimeSeries = this._currentTimeSeries;
        var linkRoute = this.get('rangeRoute');
        this.set('rangeBars', (this.get('ranges') || []).map(function (range) {
            var start = currentTimeSeries.findPointByMeasurementId(range.get('startRun'));
            var end = currentTimeSeries.findPointByMeasurementId(range.get('endRun'));
            return Ember.Object.create({
                startTime: midPoint(currentTimeSeries.previousPoint(start), start),
                endTime: midPoint(end, currentTimeSeries.nextPoint(end)),
                range: range,
                left: null,
                right: null,
                rowIndex: null,
                top: null,
                bottom: null,
                linkRoute: linkRoute,
                linkId: range.get('id'),
                label: range.get('label'),
            });
        }));

        this._updateRangeBarRects();
    }.observes('ranges'),
    _updateRangeBarRects: function () {
        var rangeBars = this.get('rangeBars');
        if (!rangeBars || !rangeBars.length)
            return;

        var xScale = this._x;
        var yScale = this._y;

        // Expand the width of each range as needed and sort ranges by the left-edge of ranges.
        var minWidth = 3;
        var sortedBars = rangeBars.map(function (bar) {
            var left = xScale(bar.get('startTime'));
            var right = xScale(bar.get('endTime'));
            if (right - left < minWidth) {
                left -= minWidth / 2;
                right += minWidth / 2;
            }
            bar.set('left', left);
            bar.set('right', right);
            return bar;
        }).sort(function (first, second) { return first.get('left') - second.get('left'); });

        // At this point, left edges of all ranges prior to a range R1 is on the left of R1.
        // Place R1 into a row in which right edges of all ranges prior to R1 is on the left of R1 to avoid overlapping ranges.
        var rows = [];
        sortedBars.forEach(function (bar) {
            var rowIndex = 0;
            for (; rowIndex < rows.length; rowIndex++) {
                var currentRow = rows[rowIndex];
                if (currentRow[currentRow.length - 1].get('right') < bar.get('left')) {
                    currentRow.push(bar);
                    break;
                }
            }
            if (rowIndex >= rows.length)
                rows.push([bar]);
            bar.set('rowIndex', rowIndex);
        });
        var rowHeight = 0.6 * this._rem;
        var firstRowTop = this._contentHeight - rows.length * rowHeight;
        var barHeight = 0.5 * this._rem;

        $(this.get('element')).find('.rangeBarsContainerInlineStyle').css({
            left: this._margin.left + 'px',
            top: this._margin.top + firstRowTop + 'px',
            width: this._contentWidth + 'px',
            height: rows.length * barHeight + 'px',
            overflow: 'hidden',
            position: 'absolute',
        });

        var margin = this._margin;
        sortedBars.forEach(function (bar) {
            var top = bar.get('rowIndex') * rowHeight;
            var height = barHeight;
            var left = bar.get('left');
            var width = bar.get('right') - left;
            bar.set('inlineStyle', 'left: ' + left + 'px; top: ' + top + 'px; width: ' + width + 'px; height: ' + height + 'px;');
        });
    },
    _updateCurrentItemIndicators: function ()
    {
        if (!this._currentItemLine)
            return;

        var item = this._currentTimeSeriesData[this._currentItemIndex];
        if (!item) {
            this._currentItemLine.attr("x1", -1000).attr("x2", -1000);
            this._currentItemCircle.attr("cx", -1000);
            return;
        }

        var x = this._x(item.time);
        var y = this._y(item.value);

        this._currentItemLine
            .attr("x1", x)
            .attr("x2", x);

        this._currentItemCircle
            .attr("cx", x)
            .attr("cy", y);
    },
    _setCurrentSelection: function (newSelection)
    {
        if (this._brushExtent === newSelection)
            return;

        var points = null;
        if (newSelection) {
            points = this._currentTimeSeriesData
                .filter(function (point) { return point.time >= newSelection[0] && point.time <= newSelection[1]; });
            if (!points.length)
                points = null;
        }

        this._brushExtent = newSelection;
        this._setCurrentItem(undefined);
        this._updateSelectionToolbar();

        if (!App.domainsAreEqual(this.get('selection'), newSelection))
            this.set('selection', newSelection);
        this.set('selectedPoints', points);
    },
    _updateSelectionToolbar: function ()
    {
        if (!this.get('interactive'))
            return;

        var selection = this._currentSelection();
        var selectionToolbar = $(this.get('element')).children('.selection-toolbar');
        if (selection) {
            var left = this._x(selection[0]);
            var right = this._x(selection[1]);
            selectionToolbar
                .css({left: this._margin.left + right, top: this._margin.top + this._contentHeight})
                .show();
        } else
            selectionToolbar.hide();
    },
    actions: {
        zoom: function ()
        {
            this.sendAction('zoom', this._currentSelection());
            this.set('selection', null);
        },
        openRange: function (range)
        {
            this.sendAction('openRange', range);
        },
    },
});
