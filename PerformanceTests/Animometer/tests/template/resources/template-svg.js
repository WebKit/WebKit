function TemplateSvgStage(element, options)
{
    Stage.call(this, element, options);
}

TemplateSvgStage.prototype = Object.create(Stage.prototype);
TemplateSvgStage.prototype.constructor = TemplateSvgStage;

TemplateSvgStage.prototype.tune = function(count)
{
    // If count is -ve, -count elements need to be removed form the
    // stage. If count is +ve, +count elements need to be added to
    // the stage.
    
    // TODO: Change objects in the stage.
    
    // Return the number of all the elements in the stage.
    // This number is recorded in the sampled data.
    
    // TODO: Return the count of the objects in the stage.
    return 0;
}

TemplateSvgStage.prototype.animate = function(timeDelta)
{
    // Animate the elements such that all of them are redrawn. You 
    // may need to define your object so it keeps its animation data.
    // This object should encapsulate a corrosponding SVGElement.
    // You may also define a method called animate() in this object
    // and just call this function here for all the elements.
    
    // TODO: Loop through all your objects and ask them to animate.
}

function TemplateSvgBenchmark(suite, test, options, progressBar)
{
    StageBenchmark.call(this, suite, test, options, progressBar);
}

TemplateSvgBenchmark.prototype = Object.create(StageBenchmark.prototype);
TemplateSvgBenchmark.prototype.constructor = TemplateSvgBenchmark;

TemplateSvgBenchmark.prototype.createStage = function(element)
{
    // You need to override this method such that your stage is hooked
    // up to the benchmark.
    return new TemplateSvgStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    // This function is called from the test harness which starts the
    // test by creating your benchmark object.
    return new TemplateSvgBenchmark(suite, test, options, progressBar);
}
