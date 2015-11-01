function graph(selector, margins, axes, samples, samplingTimeOffset)
{
    var element = document.querySelector(selector);
    element.innerHTML = '';

    var size = Point.elementClientSize(element).subtract(margins.size);

    var x = d3.scale.linear()
            .range([0, size.width])
            .domain(d3.extent(samples, function(d) { return d.timeOffset; }));            

    var yLeft = d3.scale.linear()
            .range([size.height, 0])
            .domain([0, d3.max(samples, function(d) { return d.values[0]; })]);

    var yRight = d3.scale.linear()
            .range([size.height, 0])
            .domain([0, d3.max(samples, function(d) { return d.values[1]; })]);

    var xAxis = d3.svg.axis()
            .scale(x)
            .orient("bottom");

    var yAxisLeft = d3.svg.axis()
            .scale(yLeft)
            .orient("left");

    var yAxisRight = d3.svg.axis()
            .scale(yRight)
            .orient("right");

    var lineLeft = d3.svg.line()
            .x(function(d) { return x(d.timeOffset); })
            .y(function(d) { return yLeft(d.values[0]); });

    var lineRight = d3.svg.line()
            .x(function(d) { return x(d.timeOffset); })
            .y(function(d) { return yRight(d.values[1]); });

    samples.forEach(function(d) {
        d.timeOffset = +d.timeOffset;
        d.values[0] = +d.values[0];
        d.values[1] = +d.values[1];        
    });    

    var sampledSamples = samples.filter(function(d) {
        return d.timeOffset >= samplingTimeOffset;
    });
    
    var meanLeft = d3.mean(sampledSamples, function(d) {
        return +d.values[0];
    });

    var meanRight = d3.mean(sampledSamples, function(d) {
        return +d.values[1];
    });
                            
    var svg = d3.select(selector).append("svg")
        .attr("width", size.width + margins.left + margins.right)
        .attr("height", size.height + margins.top + margins.bottom)
        .append("g")
            .attr("transform", "translate(" + margins.left + "," + margins.top + ")");

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

    // left-mean
    svg.append("svg:line")
        .attr("x1", x(0))
        .attr("x2", size.width)
        .attr("y1", yLeft(meanLeft))
        .attr("y2", yLeft(meanLeft))
        .attr("class", "left-mean");

    // right-mean
    svg.append("svg:line")
        .attr("x1", x(0))
        .attr("x2", size.width)
        .attr("y1", yRight(meanRight))
        .attr("y2", yRight(meanRight))
        .attr("class", "right-mean");        

    // samplingTimeOffset
    svg.append("line")
        .attr("x1", x(samplingTimeOffset))
        .attr("x2", x(samplingTimeOffset))
        .attr("y1", yLeft(0))
        .attr("y2", yLeft(yAxisLeft.scale().domain()[1]))
        .attr("class", "sample-time");

    // left-samples
    svg.append("path")
        .datum(samples)
        .attr("class", "left-samples")
        .attr("d", lineLeft);
        
    // right-samples
    svg.append("path")
        .datum(samples)
        .attr("class", "right-samples")
        .attr("d", lineRight);
}
