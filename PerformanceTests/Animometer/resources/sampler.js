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
    this._init();
    this._maxHeap = Algorithm.createMaxHeap(Experiment.defaults.CONCERN_SIZE);
}

Experiment.defaults =
{
    CONCERN: 5,
    CONCERN_SIZE: 100,
}

Experiment.prototype =
{
    _init: function()
    {
        this._sum = 0;
        this._squareSum = 0;
        this._numberOfSamples = 0;
    },
    
    // Called after a warm-up period
    startSampling: function()
    {
        var mean = this.mean();
        this._init();
        this._maxHeap.init();
        this.sample(mean);
    },
    
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

function Sampler(count)
{
    this.experiments = [];
    while (count--)
        this.experiments.push(new Experiment());
    this.samples = [];
    this.samplingTimeOffset = 0;
}

Sampler.prototype =
{
    startSampling: function(samplingTimeOffset)
    {
        this.experiments.forEach(function(experiment) {
            experiment.startSampling();
        });
            
        this.samplingTimeOffset = samplingTimeOffset / 1000;
    },
    
    sample: function(timeOffset, values)
    {
        if (values.length < this.experiments.length)
            throw "Not enough sample points";

        this.experiments.forEach(function(experiment, index) {
            experiment.sample(values[index]);
        });
                    
        this.samples.push({ timeOffset: timeOffset / 1000, values: values });
    },
    
    toJSON: function(statistics, graph)
    {
        var results = {};
         
        results[Strings.json.score] = this.experiments[0].score(Experiment.defaults.CONCERN);
           
        if (statistics) {
            this.experiments.forEach(function(experiment, index) {
                var jsonExperiment = !index ? Strings.json.experiments.complexity : Strings.json.experiments.frameRate;
                results[jsonExperiment] = {};
                results[jsonExperiment][Strings.json.measurements.average] = experiment.mean();
                results[jsonExperiment][Strings.json.measurements.concern] = experiment.concern(Experiment.defaults.CONCERN);
                results[jsonExperiment][Strings.json.measurements.stdev] = experiment.standardDeviation();
                results[jsonExperiment][Strings.json.measurements.percent] = experiment.percentage()
            });
        }
        
        if (graph) {
            results[Strings.json.samples] = {};
            results[Strings.json.samples][Strings.json.graph.points] = this.samples;
            results[Strings.json.samples][Strings.json.graph.samplingTimeOffset] = this.samplingTimeOffset;
        }
        
        return results;
    }
}
