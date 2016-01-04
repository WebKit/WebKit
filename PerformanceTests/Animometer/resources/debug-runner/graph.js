Utilities.extendObject(window.benchmarkController, {
    updateGraphData: function(graphData)
    {
        var element = document.getElementById("test-graph-data");
        element.innerHTML = "";
        var margins = new Insets(10, 30, 30, 40);
        var size = Point.elementClientSize(element).subtract(margins.size);

        var svg = d3.select("#test-graph-data").append("svg")
            .attr("width", size.width + margins.left + margins.right)
            .attr("height", size.height + margins.top + margins.bottom)
            .append("g")
                .attr("transform", "translate(" + margins.left + "," + margins.top + ")");

        var axes = graphData.axes;
        var targetFPS = graphData.targetFPS;

        // Axis scales
        var x = d3.scale.linear()
                .range([0, size.width])
                .domain([0, d3.max(graphData.samples, function(s) { return s.time; })]);
        var yLeft = d3.scale.linear()
                .range([size.height, 0])
                .domain([0, d3.max(graphData.samples, function(s) { return s.complexity; })]);
        var yRight = d3.scale.linear()
                .range([size.height, 0])
                .domain([0, 60]);

        // Axes
        var xAxis = d3.svg.axis()
                .scale(x)
                .orient("bottom");
        var yAxisLeft = d3.svg.axis()
                .scale(yLeft)
                .orient("left");
        var yAxisRight = d3.svg.axis()
                .scale(yRight)
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

        // samplingTimeOffset
        svg.append("line")
            .attr("x1", x(graphData.samplingTimeOffset))
            .attr("x2", x(graphData.samplingTimeOffset))
            .attr("y1", yLeft(0))
            .attr("y2", yLeft(yAxisLeft.scale().domain()[1]))
            .attr("class", "sample-time marker");

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
        if (targetFPS) {
            svg.append("line")
                .attr("x1", x(0))
                .attr("x2", size.width)
                .attr("y1", yRight(targetFPS))
                .attr("y2", yRight(targetFPS))
                .attr("class", "target-fps marker");
        }

        // Cursor
        var cursorGroup = svg.append("g").attr("id", "cursor");
        cursorGroup.append("line")
            .attr("x1", 0)
            .attr("x2", 0)
            .attr("y1", yLeft(0))
            .attr("y2", yLeft(0));

        // Data
        var allData = graphData.samples;
        var filteredData = graphData.samples.filter(function (sample) {
            return "smoothedFPS" in sample;
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
        addData("rawFPS", allData, function(d) { return yRight(d.fps); }, 1);
        addData("filteredFPS", filteredData, function(d) { return yRight(d.smoothedFPS); }, 2);
        addData("intervalFPS", filteredData, function(d) { return yRight(d.intervalFPS); }, 3, true);

        // Area to handle mouse events
        var area = svg.append("rect")
            .attr("fill", "transparent")
            .attr("x", 0)
            .attr("y", 0)
            .attr("width", size.x)
            .attr("height", size.y);

        var timeBisect = d3.bisector(function(d) { return d.time; }).right;
        var statsToHighlight = ["complexity", "rawFPS", "filteredFPS", "intervalFPS"];
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
            if (form["rawFPS"].checked)
                cursor_y = Math.max(cursor_y, data.fps);
            cursorGroup.select("line")
                .attr("x1", cursor_x)
                .attr("x2", cursor_x)
                .attr("y2", yRight(cursor_y));

            document.querySelector("#test-graph nav .time").textContent = data.time.toFixed(3) + "s (" + index + ")";
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
                    content = data.fps.toFixed(2);
                    data_y = yRight(data.fps);
                    break;
                case "filteredFPS":
                    if ("smoothedFPS" in data) {
                        content = data.smoothedFPS.toFixed(2);
                        data_y = yRight(data.smoothedFPS);
                    }
                    break;
                case "intervalFPS":
                    if ("intervalFPS" in data) {
                        content = data.intervalFPS.toFixed(2);
                        data_y = yRight(data.intervalFPS);
                    }
                    break;
                }

                element.textContent = content;

                if (form[name].checked && data_y !== null) {
                    cursorGroup.select("." + name)
                        .attr("cx", cursor_x)
                        .attr("cy", data_y);
                    document.querySelector("#cursor ." + name).classList.remove("hidden");
                } else
                    document.querySelector("#cursor ." + name).classList.add("hidden");
            });
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
        showOrHideNodes(form["intervalFPS"].checked, "#intervalFPS");
    }
});
