var Statistics =
{
    sampleMean: function(numberOfSamples, sum)
    {
        if (numberOfSamples < 1)
            return 0;
        return sum / numberOfSamples;
    },

    // With sum and sum of squares, we can compute the sample standard deviation in O(1).
    // See https://rniwa.com/2012-11-10/sample-standard-deviation-in-terms-of-sum-and-square-sum-of-samples/
    unbiasedSampleStandardDeviation: function(numberOfSamples, sum, squareSum)
    {
        if (numberOfSamples < 2)
            return 0;
        return Math.sqrt((squareSum - sum * sum / numberOfSamples) / (numberOfSamples - 1));
    },

    geometricMean: function(values)
    {
        if (!values.length)
            return 0;
        var roots = values.map(function(value) { return  Math.pow(value, 1 / values.length); })
        return roots.reduce(function(a, b) { return a * b; });
    }
}

function Experiment()
{
    this._sum = 0;
    this._squareSum = 0;
    this._numberOfSamples = 0;
    this._maxHeap = Algorithm.createMaxHeap(Experiment.defaults.CONCERN_SIZE);
}

Experiment.defaults =
{
    CONCERN: 5,
    CONCERN_SIZE: 100,
}

Experiment.prototype =
{
    sample: function(value)
    {
        this._sum += value;
        this._squareSum += value * value;
        this._maxHeap.push(value);
        ++this._numberOfSamples;
    },

    mean: function()
    {
        return Statistics.sampleMean(this._numberOfSamples, this._sum);
    },

    standardDeviation: function()
    {
        return Statistics.unbiasedSampleStandardDeviation(this._numberOfSamples, this._sum, this._squareSum);
    },

    percentage: function()
    {
        var mean = this.mean();
        return mean ? this.standardDeviation() * 100 / mean : 0;
    },

    concern: function(percentage)
    {
        var size = Math.ceil(this._numberOfSamples * percentage / 100);
        var values = this._maxHeap.values(size);
        return values.length ? values.reduce(function(a, b) { return a + b; }) / values.length : 0;
    },

    score: function(percentage)
    {
        return Statistics.geometricMean([this.mean(), Math.max(this.concern(percentage), 1)]);
    }
}

function Sampler(seriesCount, expectedSampleCount, processor)
{
    this._processor = processor;

    this.samples = [];
    for (var i = 0; i < seriesCount; ++i) {
        var array = new Array(expectedSampleCount);
        array.fill(0);
        this.samples[i] = array;
    }
    this.sampleCount = 0;
    this.marks = {};
}

Sampler.prototype =
{
    record: function() {
        // Assume that arguments.length == this.samples.length
        for (var i = 0; i < arguments.length; i++) {
            this.samples[i][this.sampleCount] = arguments[i];
        }
        ++this.sampleCount;
    },

    mark: function(comment, data) {
        data = data || {};
        // The mark exists after the last recorded sample
        data.index = this.sampleCount;

        this.marks[comment] = data;
    },

    process: function(options)
    {
        var results = {};

        if (options["adjustment"] == "adaptive")
            results[Strings.json.targetFPS] = +options["frame-rate"];

        // Remove unused capacity
        this.samples = this.samples.map(function(array) {
            return array.slice(0, this.sampleCount);
        }, this);

        this._processor.processSamples(results);

        return results;
    }
}
