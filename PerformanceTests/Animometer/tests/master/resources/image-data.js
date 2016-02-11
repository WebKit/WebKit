(function() {

var ImageDataStage = Utilities.createSubclass(Stage,
    function() {
        Stage.call(this);

        this.testElements = [];
        this._offsetIndex = 0;
    }, {

    testImageSrc: './../resources/yin-yang.png',

    initialize: function(benchmark)
    {
        Stage.prototype.initialize.call(this, benchmark);

        var waitForLoad = new SimplePromise;
        var img = new Image;

        img.addEventListener('load', function onImageLoad(e) {
            img.removeEventListener('load', onImageLoad);
            waitForLoad.resolve(img);
        });

        img.src = this.testImageSrc;

        waitForLoad.then(function(img) {
            this.testImage = img;
            this.testImageWidth = img.naturalWidth;
            this.testImageHeight = img.naturalHeight;

            this.diffuseXOffset = 4,
            this.diffuseYOffset = this.testImageWidth * 4;

            benchmark.readyPromise.resolve();
        }.bind(this));
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this._offsetIndex = Math.max(this._offsetIndex + count, 0);
            for (var i = this._offsetIndex; i < this.testElements.length; ++i)
                this.testElements[i].style.visibility = "hidden";
            return;
        }

        this._offsetIndex = this._offsetIndex + count;
        var index = Math.min(this._offsetIndex, this.testElements.length);
        for (var i = 0; i < index; ++i)
            this.testElements[i].style.visibility = "";
        if (this._offsetIndex <= this.testElements.length)
            return;

        index = this._offsetIndex - this.testElements.length;
        for (var i = 0; i < index; ++i) {
            var element = this._createTestElement();
            this.testElements.push(element);
            this.element.appendChild(element);
        }
    },

    _createTestElement: function() {
        var element = document.createElement('canvas');
        element.width = this.testImageWidth;
        element.height = this.testImageHeight;

        var context = element.getContext("2d");

        // Put draw image into the canvas
        context.drawImage(this.testImage, 0, 0, this.testImageWidth, this.testImageHeight);

        // randomize location
        var left = Stage.randomInt(0, this.size.width - this.testImageWidth);
        var top = Stage.randomInt(0, this.size.height - this.testImageHeight);

        element.style.top = top + 'px';
        element.style.left = left + 'px';
        element.style.width = this.testImageWidth + 'px';
        element.style.height = this.testImageHeight + 'px';

        return element;
    },

    animate: function(timeDelta) {
        for (var i = 0; i < this._offsetIndex; ++i) {
            var context = this.testElements[i].getContext("2d");

            // Get image data
            var imageData = context.getImageData(0, 0, this.testImageWidth, this.testImageHeight);

            var rgbaLen = 4,
                didDraw = false,
                neighborPixelIndex,
                dataLen = imageData.data.length;
            for (var j = 0; j < dataLen; j += rgbaLen) {
                if (imageData.data[j + 3] === 0)
                    continue;

                // get random neighboring pixel color
                neighborPixelIndex = this._getRandomNeighboringPixelIndex(j, dataLen);

                // Update the RGB data
                imageData.data[j] = imageData.data[neighborPixelIndex];
                imageData.data[j + 1] = imageData.data[neighborPixelIndex + 1];
                imageData.data[j + 2] = imageData.data[neighborPixelIndex + 2];
                imageData.data[j + 3] = Math.max(imageData.data[neighborPixelIndex + 3] - j % 10, 0);
                didDraw = true;
            }

            // Put the image data back into the canvas
            context.putImageData(imageData, 0, 0);

             // If it didn't draw restart
            if (!didDraw)
                context.drawImage(this.testImage, 0, 0, this.testImageWidth, this.testImageHeight)
        }
    },

    _getRandomNeighboringPixelIndex: function(pixelIdx, pixelArrayLength) {
        var xOffset = Stage.randomInt(-1, 1),
            yOffset = Stage.randomInt(-1, 1),
            resultPixelIdx = pixelIdx;

        // Add X to the result
        resultPixelIdx += (this.diffuseXOffset * xOffset);
        // Add Y to the result
        resultPixelIdx += (this.diffuseYOffset * yOffset);

        // Don't fall off the end of the image
        if (resultPixelIdx > pixelArrayLength)
            resultPixelIdx = pixelIdx;

        // Don't fall off the beginning of the image
        if (resultPixelIdx < 0)
            resultPixelIdx = 0;

        return resultPixelIdx;
    },

    complexity: function()
    {
        return this._offsetIndex;
    }
});

var ImageDataBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new ImageDataStage(), options);
    }, {

    waitUntilReady: function() {
        this.readyPromise = new SimplePromise;
        return this.readyPromise;
    }
});

window.benchmarkClass = ImageDataBenchmark;

}());
