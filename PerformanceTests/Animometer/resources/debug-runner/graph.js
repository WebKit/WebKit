Utilities.extendObject(window.benchmarkController, {
    layoutCounter: 0,

    updateGraphData: function(graphData)
    {
        var element = document.getElementById("test-graph-data");
        element.innerHTML = "";
        document.querySelector("hr").style.width = this.layoutCounter++ + "px";

        var margins = new Insets(30, 30, 30, 40);
        var size = Point.elementClientSize(element).subtract(margins.size);

        var svg = d3.select("#test-graph-data").append("svg")
            .attr("width", size.width + margins.left + margins.right)
            .attr("height", size.height + margins.top + margins.bottom)
            .append("g")
                .attr("transform", "translate(" + margins.left + "," + margins.top + ")");

        var axes = graphData.axes;
        var targetFrameLength = graphData.targetFrameLength;

        // Axis scales
        var x = d3.scale.linear()
                .range([0, size.width])
                .domain([0, d3.max(graphData.samples, function(s) { return s.time; })]);
        var yLeft = d3.scale.linear()
                .range([size.height, 0])
                .domain([0, d3.max(graphData.samples, function(s) { return s.complexity; })]);
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
            .attr("class", "y axis")
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
            .attr("class", "y axis")
            .attr("fill", "#FA4925")
            .attr("transform", "translate(" + size.width + ", 0)")
            .call(yAxisRight)
            .append("text")
                .attr("class", "label")
                .attr("transform", "rotate(-90)")
                .attr("y", 6)
                .attr("fill", "#FA4925")
                .attr("dy", ".71em")
                .style("text-anchor", "end")
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

        // left-mean
        svg.append("line")
            .attr("x1", x(0))
            .attr("x2", size.width)
            .attr("y1", yLeft(graphData.mean[0]))
            .attr("y2", yLeft(graphData.mean[0]))
            .attr("class", "left-mean mean");

        // right-mean
        svg.append("line")
            .attr("x1", x(0))
            .attr("x2", size.width)
            .attr("y1", yRight(graphData.mean[1]))
            .attr("y2", yRight(graphData.mean[1]))
            .attr("class", "right-mean mean");

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
        var cursorGroup = svg.append("g").attr("id", "cursor");
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

        // Area to handle mouse events
        var area = svg.append("rect")
            .attr("fill", "transparent")
            .attr("x", 0)
            .attr("y", 0)
            .attr("width", size.x)
            .attr("height", size.y);

        var timeBisect = d3.bisector(function(d) { return d.time; }).right;
        var statsToHighlight = ["complexity", "rawFPS", "filteredFPS"];
        area.on("mouseover", function() {
            document.getElementById("cursor").classList.remove("hidden");
            document.querySelector("#test-graph nav").classList.remove("hide-data");
        }).on("mouseout", function() {
            document.getElementById("cursor").classList.add("hidden");
            document.querySelector("#test-graph nav").classList.add("hide-data");
        }).on("mousemove", function() {
            var form = document.forms["graph-options"].elements;

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
                    document.querySelector("#cursor ." + name).classList.remove("hidden");
                } else
                    document.querySelector("#cursor ." + name).classList.add("hidden");
            });

            if (form["rawFPS"].checked)
                cursor_y = Math.max(cursor_y, data.frameLength);
            cursorGroup.select("line")
                .attr("x1", cursor_x)
                .attr("x2", cursor_x)
                .attr("y1", Math.min.apply(null, ys))
                .attr("y2", Math.max.apply(null, ys));

        });
        this.onGraphOptionsChanged();
    },

    onGraphOptionsChanged: function() {
        var form = document.forms["graph-options"].elements;

        function showOrHideNodes(isShown, selector) {
            var nodeList = document.querySelectorAll(selector);
            if (isShown) {
                for (var i = 0; i < nodeList.length; ++i)
                    nodeList[i].classList.remove("hidden");
            } else {
                for (var i = 0; i < nodeList.length; ++i)
                    nodeList[i].classList.add("hidden");
            }
        }

        showOrHideNodes(form["markers"].checked, ".marker");
        showOrHideNodes(form["averages"].checked, ".mean");
        showOrHideNodes(form["complexity"].checked, "#complexity");
        showOrHideNodes(form["rawFPS"].checked, "#rawFPS");
        showOrHideNodes(form["filteredFPS"].checked, "#filteredFPS");
    }
});
