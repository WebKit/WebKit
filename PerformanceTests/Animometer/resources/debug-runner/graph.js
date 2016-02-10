Utilities.extendObject(window.benchmarkController, {
    layoutCounter: 0,

    updateGraphData: function(graphData)
    {
        var element = document.getElementById("test-graph-data");
        element.innerHTML = "";
        element.graphData = graphData;
        document.querySelector("hr").style.width = this.layoutCounter++ + "px";

        var margins = new Insets(30, 30, 30, 40);
        var size = Point.elementClientSize(element).subtract(margins.size);

        this.createTimeGraph(graphData, margins, size);
        this.onTimeGraphOptionsChanged();

        var hasComplexityRegression = !!graphData.complexityRegression;
        this._showOrHideNodes(hasComplexityRegression, "form[name=graph-type]");
        if (hasComplexityRegression) {
            document.forms["graph-type"].elements["type"] = "complexity";
            this.createComplexityGraph(graphData, margins, size);
            this.onComplexityGraphOptionsChanged();
        }

        this.onGraphTypeChanged();
    },

    _addRegressionLine: function(parent, xScale, yScale, points, stdev, isAlongYAxis)
    {
        var polygon = [];
        var line = []
        var xStdev = isAlongYAxis ? stdev : 0;
        var yStdev = isAlongYAxis ? 0 : stdev;
        for (var i = 0; i < points.length; ++i) {
            var point = points[i];
            polygon.push(xScale(point[0] + xStdev), yScale(point[1] + yStdev));
            line.push(xScale(point[0]), yScale(point[1]));
        }
        for (var i = points.length - 1; i >= 0; --i) {
            var point = points[i];
            polygon.push(xScale(point[0] - xStdev), yScale(point[1] - yStdev));
        }
        parent.append("polygon")
            .attr("points", polygon.join(","));
        parent.append("line")
            .attr("x1", line[0])
            .attr("y1", line[1])
            .attr("x2", line[2])
            .attr("y2", line[3]);
    },

    _addRegression: function(data, svg, xScale, yScale)
    {
        svg.append("circle")
            .attr("cx", xScale(data.segment2[1][0]))
            .attr("cy", yScale(data.segment2[1][1]))
            .attr("r", 5);
        this._addRegressionLine(svg, xScale, yScale, data.segment1, data.stdev);
        this._addRegressionLine(svg, xScale, yScale, data.segment2, data.stdev);
    },

    createComplexityGraph: function(graphData, margins, size)
    {
        var svg = d3.select("#test-graph-data").append("svg")
            .attr("id", "complexity-graph")
            .attr("class", "hidden")
            .attr("width", size.width + margins.left + margins.right)
            .attr("height", size.height + margins.top + margins.bottom)
            .append("g")
                .attr("transform", "translate(" + margins.left + "," + margins.top + ")");

        var xMin = 100000, xMax = 0;
        if (graphData.timeRegressions) {
            graphData.timeRegressions.forEach(function(regression) {
                for (var i = regression.startIndex; i <= regression.endIndex; ++i) {
                    xMin = Math.min(xMin, graphData.samples[i].complexity);
                    xMax = Math.max(xMax, graphData.samples[i].complexity);
                }
            });
        } else {
            xMin = d3.min(graphData.samples, function(s) { return s.complexity; });
            xMax = d3.max(graphData.samples, function(s) { return s.complexity; });
        }

        var xScale = d3.scale.linear()
            .range([0, size.width])
            .domain([xMin, xMax]);
        var yScale = d3.scale.linear()
                .range([size.height, 0])
                .domain([1000/20, 1000/60]);

        var xAxis = d3.svg.axis()
                .scale(xScale)
                .orient("bottom");
        var yAxis = d3.svg.axis()
                .scale(yScale)
                .tickValues([1000/20, 1000/25, 1000/30, 1000/35, 1000/40, 1000/45, 1000/50, 1000/55, 1000/60])
                .tickFormat(function(d) { return (1000 / d).toFixed(0); })
                .orient("left");

        // x-axis
        svg.append("g")
            .attr("class", "x axis")
            .attr("transform", "translate(0," + size.height + ")")
            .call(xAxis);

        // y-axis
        svg.append("g")
            .attr("class", "y axis")
            .call(yAxis);

        // time-based regression
        var mean = svg.append("g")
            .attr("class", "mean complexity");
        var complexity = graphData.averages[Strings.json.experiments.complexity];
        this._addRegressionLine(mean, xScale, yScale, [[complexity.average, yScale.domain()[0]], [complexity.average, yScale.domain()[1]]], complexity.stdev, true);

        // regression
        this._addRegression(graphData.complexityRegression, svg.append("g").attr("class", "regression raw"), xScale, yScale);
        this._addRegression(graphData.complexityAverageRegression, svg.append("g").attr("class", "regression average"), xScale, yScale);

        var svgGroup = svg.append("g")
            .attr("class", "series raw");
        var seriesCounter = 0;
        graphData.timeRegressions.forEach(function(regression, i) {
            seriesCounter++;
            var group = svgGroup.append("g")
                .attr("class", "series-" + seriesCounter)
                .attr("fill", "hsl(" + (i / graphData.timeRegressions.length * 360).toFixed(0) + ", 96%, 56%)");
            group.selectAll("circle")
                .data(graphData.samples)
                .enter()
                .append("circle")
                .filter(function(d, i) { return i >= regression.startIndex && i <= regression.endIndex; })
                .attr("cx", function(d) { return xScale(d.complexity); })
                .attr("cy", function(d) { return yScale(d.frameLength); })
                .attr("r", 2);
        });

        group = svg.append("g")
            .attr("class", "series average")
            .selectAll("circle")
                .data(graphData.complexityAverageSamples)
                .enter();
        group.append("circle")
            .attr("cx", function(d) { return xScale(d.complexity); })
            .attr("cy", function(d) { return yScale(d.frameLength); })
            .attr("r", 3)
        group.append("line")
            .attr("x1", function(d) { return xScale(d.complexity); })
            .attr("x2", function(d) { return xScale(d.complexity); })
            .attr("y1", function(d) { return yScale(d.frameLength - d.stdev); })
            .attr("y2", function(d) { return yScale(d.frameLength + d.stdev); });

        // Cursor
        var cursorGroup = svg.append("g").attr("class", "cursor hidden");
        cursorGroup.append("line")
            .attr("class", "x")
            .attr("x1", 0)
            .attr("x2", 0)
            .attr("y1", yScale(yAxis.scale().domain()[0]) + 10)
            .attr("y2", yScale(yAxis.scale().domain()[1]));
        cursorGroup.append("line")
            .attr("class", "y")
            .attr("x1", xScale(0) - 10)
            .attr("x2", xScale(xAxis.scale().domain()[1]))
            .attr("y1", 0)
            .attr("y2", 0)
        cursorGroup.append("text")
            .attr("class", "label x")
            .attr("x", 0)
            .attr("y", yScale(yAxis.scale().domain()[0]) + 15)
            .attr("baseline-shift", "-100%")
            .attr("text-anchor", "middle");
        cursorGroup.append("text")
            .attr("class", "label y")
            .attr("x", xScale(0) - 15)
            .attr("y", 0)
            .attr("baseline-shift", "-30%")
            .attr("text-anchor", "end");
        // Area to handle mouse events
        var area = svg.append("rect")
            .attr("fill", "transparent")
            .attr("x", 0)
            .attr("y", 0)
            .attr("width", size.width)
            .attr("height", size.height);

        area.on("mouseover", function() {
            document.querySelector("#complexity-graph .cursor").classList.remove("hidden");
        }).on("mouseout", function() {
            document.querySelector("#complexity-graph .cursor").classList.add("hidden");
        }).on("mousemove", function() {
            var location = d3.mouse(this);
            var location_domain = [xScale.invert(location[0]), yScale.invert(location[1])];
            cursorGroup.select("line.x")
                .attr("x1", location[0])
                .attr("x2", location[0]);
            cursorGroup.select("text.x")
                .attr("x", location[0])
                .text(location_domain[0].toFixed(1));
            cursorGroup.select("line.y")
                .attr("y1", location[1])
                .attr("y2", location[1]);
            cursorGroup.select("text.y")
                .attr("y", location[1])
                .text((1000 / location_domain[1]).toFixed(1));
        });
    },

    createTimeGraph: function(graphData, margins, size)
    {
        var svg = d3.select("#test-graph-data").append("svg")
            .attr("id", "time-graph")
            .attr("width", size.width + margins.left + margins.right)
            .attr("height", size.height + margins.top + margins.bottom)
            .append("g")
                .attr("transform", "translate(" + margins.left + "," + margins.top + ")");

        var axes = graphData.axes;
        var targetFrameLength = graphData.targetFrameLength;

        // Axis scales
        var x = d3.scale.linear()
                .range([0, size.width])
                .domain([
                    Math.min(d3.min(graphData.samples, function(s) { return s.time; }), 0),
                    d3.max(graphData.samples, function(s) { return s.time; })]);
        var complexityMax = d3.max(graphData.samples, function(s) { return s.complexity; });
        if (graphData.timeRegressions) {
            complexityMax = Math.max.apply(Math, graphData.timeRegressions.map(function(regression) {
                return regression.maxComplexity || 0;
            }));
        }

        var yLeft = d3.scale.linear()
                .range([size.height, 0])
                .domain([0, complexityMax]);
        var yRight = d3.scale.linear()
                .range([size.height, 0])
                .domain([1000/20, 1000/60]);

        // Axes
        var xAxis = d3.svg.axis()
                .scale(x)
                .orient("bottom")
                .tickFormat(function(d) { return (d/1000).toFixed(0); });
        var yAxisLeft = d3.svg.axis()
                .scale(yLeft)
                .orient("left");
        var yAxisRight = d3.svg.axis()
                .scale(yRight)
                .tickValues([1000/20, 1000/25, 1000/30, 1000/35, 1000/40, 1000/45, 1000/50, 1000/55, 1000/60])
                .tickFormat(function(d) { return (1000/d).toFixed(0); })
                .orient("right");

        // x-axis
        svg.append("g")
            .attr("class", "x axis")
            .attr("fill", "rgb(235, 235, 235)")
            .attr("transform", "translate(0," + size.height + ")")
            .call(xAxis)
            .append("text")
                .attr("class", "label")
                .attr("x", size.width)
                .attr("y", -6)
                .attr("fill", "rgb(235, 235, 235)")
                .style("text-anchor", "end")
                .text("time");

        // yLeft-axis
        svg.append("g")
            .attr("class", "yLeft axis")
            .attr("fill", "#7ADD49")
            .call(yAxisLeft)
            .append("text")
                .attr("class", "label")
                .attr("transform", "rotate(-90)")
                .attr("y", 6)
                .attr("fill", "#7ADD49")
                .attr("dy", ".71em")
                .style("text-anchor", "end")
                .text(axes[0]);

        // yRight-axis
        svg.append("g")
            .attr("class", "yRight axis")
            .attr("fill", "#FA4925")
            .attr("transform", "translate(" + size.width + ", 0)")
            .call(yAxisRight)
            .append("text")
                .attr("class", "label")
                .attr("x", 9)
                .attr("y", -20)
                .attr("fill", "#FA4925")
                .attr("dy", ".71em")
                .style("text-anchor", "start")
                .text(axes[1]);

        // marks
        var yMin = yLeft(0);
        var yMax = yLeft(yAxisLeft.scale().domain()[1]);
        for (var markName in graphData.marks) {
            var mark = graphData.marks[markName];
            var xLocation = x(mark.time);

            var markerGroup = svg.append("g")
                .attr("class", "marker")
                .attr("transform", "translate(" + xLocation + ", 0)");
            markerGroup.append("text")
                .attr("transform", "translate(10, " + (yMin - 10) + ") rotate(-90)")
                .style("text-anchor", "start")
                .text(markName)
            markerGroup.append("line")
                .attr("x1", 0)
                .attr("x2", 0)
                .attr("y1", yMin)
                .attr("y2", yMax);
        }

        if (Strings.json.experiments.complexity in graphData.averages) {
            var complexity = graphData.averages[Strings.json.experiments.complexity];
            var regression = svg.append("g")
                .attr("class", "complexity mean");
            this._addRegressionLine(regression, x, yLeft, [[graphData.samples[0].time, complexity.average], [graphData.samples[graphData.samples.length - 1].time, complexity.average]], complexity.stdev);
        }
        if (Strings.json.experiments.frameRate in graphData.averages) {
            var frameRate = graphData.averages[Strings.json.experiments.frameRate];
            var regression = svg.append("g")
                .attr("class", "fps mean");
            this._addRegressionLine(regression, x, yRight, [[graphData.samples[0].time, 1000/frameRate.average], [graphData.samples[graphData.samples.length - 1].time, 1000/frameRate.average]], frameRate.stdev);
        }

        // right-target
        if (targetFrameLength) {
            svg.append("line")
                .attr("x1", x(0))
                .attr("x2", size.width)
                .attr("y1", yRight(targetFrameLength))
                .attr("y2", yRight(targetFrameLength))
                .attr("class", "target-fps marker");
        }

        // Cursor
        var cursorGroup = svg.append("g").attr("class", "cursor");
        cursorGroup.append("line")
            .attr("x1", 0)
            .attr("x2", 0)
            .attr("y1", yMin)
            .attr("y2", yMin);

        // Data
        var allData = graphData.samples;
        var filteredData = graphData.samples.filter(function (sample) {
            return "smoothedFrameLength" in sample;
        });

        function addData(name, data, yCoordinateCallback, pointRadius, omitLine) {
            var svgGroup = svg.append("g").attr("id", name);
            if (!omitLine) {
                svgGroup.append("path")
                    .datum(data)
                    .attr("d", d3.svg.line()
                        .x(function(d) { return x(d.time); })
                        .y(yCoordinateCallback));
            }
            svgGroup.selectAll("circle")
                .data(data)
                .enter()
                .append("circle")
                .attr("cx", function(d) { return x(d.time); })
                .attr("cy", yCoordinateCallback)
                .attr("r", pointRadius);

            cursorGroup.append("circle")
                .attr("class", name)
                .attr("r", pointRadius + 2);
        }

        addData("complexity", allData, function(d) { return yLeft(d.complexity); }, 2);
        addData("rawFPS", allData, function(d) { return yRight(d.frameLength); }, 1);
        addData("filteredFPS", filteredData, function(d) { return yRight(d.smoothedFrameLength); }, 2);

        // regressions
        var regressionGroup = svg.append("g")
            .attr("id", "regressions");
        if (graphData.timeRegressions) {
            var complexities = [];
            graphData.timeRegressions.forEach(function (regression) {
                regressionGroup.append("line")
                    .attr("x1", x(regression.segment1[0][0]))
                    .attr("x2", x(regression.segment1[1][0]))
                    .attr("y1", yRight(regression.segment1[0][1]))
                    .attr("y2", yRight(regression.segment1[1][1]));
                regressionGroup.append("line")
                    .attr("x1", x(regression.segment2[0][0]))
                    .attr("x2", x(regression.segment2[1][0]))
                    .attr("y1", yRight(regression.segment2[0][1]))
                    .attr("y2", yRight(regression.segment2[1][1]));
                // inflection point
                regressionGroup.append("circle")
                    .attr("cx", x(regression.segment1[1][0]))
                    .attr("cy", yLeft(regression.complexity))
                    .attr("r", 5);
                complexities.push(regression.complexity);
            });
            if (complexities.length) {
                var yLeftComplexities = d3.svg.axis()
                    .scale(yLeft)
                    .tickValues(complexities)
                    .tickSize(10)
                    .orient("left");
                svg.append("g")
                    .attr("class", "complexity yLeft axis")
                    .call(yLeftComplexities);
            }
        }

        // Area to handle mouse events
        var area = svg.append("rect")
            .attr("fill", "transparent")
            .attr("x", 0)
            .attr("y", 0)
            .attr("width", size.width)
            .attr("height", size.height);

        var timeBisect = d3.bisector(function(d) { return d.time; }).right;
        var statsToHighlight = ["complexity", "rawFPS", "filteredFPS"];
        area.on("mouseover", function() {
            document.querySelector("#time-graph .cursor").classList.remove("hidden");
            document.querySelector("#test-graph nav").classList.remove("hide-data");
        }).on("mouseout", function() {
            document.querySelector("#time-graph .cursor").classList.add("hidden");
            document.querySelector("#test-graph nav").classList.add("hide-data");
        }).on("mousemove", function() {
            var form = document.forms["time-graph-options"].elements;

            var mx_domain = x.invert(d3.mouse(this)[0]);
            var index = Math.min(timeBisect(allData, mx_domain), allData.length - 1);
            var data = allData[index];
            var cursor_x = x(data.time);
            var cursor_y = yAxisRight.scale().domain()[1];
            var ys = [yRight(yAxisRight.scale().domain()[0]), yRight(yAxisRight.scale().domain()[1])];

            document.querySelector("#test-graph nav .time").textContent = (data.time / 1000).toFixed(4) + "s (" + index + ")";
            statsToHighlight.forEach(function(name) {
                var element = document.querySelector("#test-graph nav ." + name);
                var content = "";
                var data_y = null;
                switch (name) {
                case "complexity":
                    content = data.complexity;
                    data_y = yLeft(data.complexity);
                    break;
                case "rawFPS":
                    content = (1000/data.frameLength).toFixed(2);
                    data_y = yRight(data.frameLength);
                    break;
                case "filteredFPS":
                    if ("smoothedFrameLength" in data) {
                        content = (1000/data.smoothedFrameLength).toFixed(2);
                        data_y = yRight(data.smoothedFrameLength);
                    }
                    break;
                }

                element.textContent = content;

                if (form[name].checked && data_y !== null) {
                    ys.push(data_y);
                    cursorGroup.select("." + name)
                        .attr("cx", cursor_x)
                        .attr("cy", data_y);
                    document.querySelector("#time-graph .cursor ." + name).classList.remove("hidden");
                } else
                    document.querySelector("#time-graph .cursor ." + name).classList.add("hidden");
            });

            if (form["rawFPS"].checked)
                cursor_y = Math.max(cursor_y, data.frameLength);
            cursorGroup.select("line")
                .attr("x1", cursor_x)
                .attr("x2", cursor_x)
                .attr("y1", Math.min.apply(null, ys))
                .attr("y2", Math.max.apply(null, ys));

        });
    },

    _showOrHideNodes: function(isShown, selector) {
        var nodeList = document.querySelectorAll(selector);
        if (isShown) {
            for (var i = 0; i < nodeList.length; ++i)
                nodeList[i].classList.remove("hidden");
        } else {
            for (var i = 0; i < nodeList.length; ++i)
                nodeList[i].classList.add("hidden");
        }
    },

    onComplexityGraphOptionsChanged: function() {
        var form = document.forms["complexity-graph-options"].elements;
        benchmarkController._showOrHideNodes(form["series-raw"].checked, "#complexity-graph .series.raw");
        benchmarkController._showOrHideNodes(form["series-average"].checked, "#complexity-graph .series.average");
        benchmarkController._showOrHideNodes(form["regression-time-score"].checked, "#complexity-graph .mean.complexity");
        benchmarkController._showOrHideNodes(form["complexity-regression-aggregate-raw"].checked, "#complexity-graph .regression.raw");
        benchmarkController._showOrHideNodes(form["complexity-regression-aggregate-average"].checked, "#complexity-graph .regression.average");
    },

    onTimeGraphOptionsChanged: function() {
        var form = document.forms["time-graph-options"].elements;
        benchmarkController._showOrHideNodes(form["markers"].checked, ".marker");
        benchmarkController._showOrHideNodes(form["averages"].checked, "#test-graph-data .mean");
        benchmarkController._showOrHideNodes(form["complexity"].checked, "#complexity");
        benchmarkController._showOrHideNodes(form["rawFPS"].checked, "#rawFPS");
        benchmarkController._showOrHideNodes(form["filteredFPS"].checked, "#filteredFPS");
    },

    onGraphTypeChanged: function() {
        var form = document.forms["graph-type"].elements;
        var graphData = document.getElementById("test-graph-data").graphData;
        var isTimeSelected = form["graph-type"].value == "time";

        benchmarkController._showOrHideNodes(isTimeSelected, "#time-graph");
        benchmarkController._showOrHideNodes(isTimeSelected, "form[name=time-graph-options]");
        benchmarkController._showOrHideNodes(!isTimeSelected, "#complexity-graph");
        benchmarkController._showOrHideNodes(!isTimeSelected, "form[name=complexity-graph-options]");

        var score, mean;
        if (isTimeSelected) {
            score = graphData.score.toFixed(2);

            var regression = graphData.averages.complexity;
            mean = [
                "mean: ",
                regression.average.toFixed(2),
                " ± ",
                regression.stdev.toFixed(2),
                " (",
                regression.percent.toFixed(2),
                "%)"];
            if (regression.concern) {
                mean = mean.concat([
                    ", worst 5%: ",
                    regression.concern.toFixed(2)]);
            }
            mean = mean.join("");
        } else {
            score = [
                "raw: ",
                graphData.complexityRegression.complexity.toFixed(2),
                ", average: ",
                graphData.complexityAverageRegression.complexity.toFixed(2)].join("");

            mean = [
                "raw: ±",
                graphData.complexityRegression.stdev.toFixed(2),
                "ms, average: ±",
                graphData.complexityAverageRegression.stdev.toFixed(2),
                "ms"].join("");
        }

        sectionsManager.setSectionScore("test-graph", score, mean);
    }
});
