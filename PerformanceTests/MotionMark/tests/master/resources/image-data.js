(function() {

var ImageDataStage = Utilities.createSubclass(Stage,
    function() {
        Stage.call(this);

        this.testElements = [];
        this._offsetIndex = 0;
    }, {

    imageWidth: 50,
    imageHeight: 50,
    pixelStride: 4,
    rowStride: 200,
    weightNegativeThreshold: 0.04,
    weightPositiveThreshold: 0.96,
    imageSrcs: [
        "compass",
        "console",
        "contribute",
        "debugger",
        "inspector",
        "layout",
        "performance",
        "script",
        "shortcuts",
        "standards",
        "storage",
        "styles",
        "timeline"
    ],
    images: [],

    initialize: function(benchmark)
    {
        Stage.prototype.initialize.call(this, benchmark);

        var lastPromise;
        var images = this.images;
        this.imageSrcs.forEach(function(imageSrc) {
            var promise = this._loadImage("resources/" + imageSrc + ".svg");
            if (!lastPromise)
                lastPromise = promise;
            else {
                lastPromise = lastPromise.then(function(img) {
                    images.push(img);
                    return promise;
                });
            }
        }, this);

        lastPromise.then(function(img) {
            images.push(img);
            benchmark.readyPromise.resolve();
        }.bind(this));
    },

    _loadImage: function(src) {
        var img = new Image;
        var promise = new SimplePromise;

        img.addEventListener('load', function onImageLoad(e) {
            img.removeEventListener('load', onImageLoad);
            promise.resolve(img);
        });

        img.src = src;
        return promise;
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this._offsetIndex = Math.max(this._offsetIndex + count, 0);
            for (var i = this._offsetIndex; i < this.testElements.length; ++i)
                this.testElements[i].style.display = "none";
            return;
        }

        this._offsetIndex = this._offsetIndex + count;
        var index = Math.min(this._offsetIndex, this.testElements.length);
        for (var i = 0; i < index; ++i) {
            this.testElements[i].style.display = "block";
            this._refreshElement(this.testElements[i]);
        }
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
        element.width = this.imageWidth;
        element.height = this.imageHeight;
        element.style.width = this.imageWidth + 'px';
        element.style.height = this.imageHeight + 'px';

        this._refreshElement(element);
        return element;
    },

    _refreshElement: function(element) {
        var top = Stage.randomInt(0, Math.floor((this.size.height - this.imageHeight) / this.imageHeight)) * this.imageHeight;
        var left = Stage.randomInt(0, Math.floor((this.size.width - this.imageWidth) / this.imageWidth)) * this.imageWidth;

        element.style.top = top + 'px';
        element.style.left = left + 'px';
    },

    animate: function(timeDelta) {
        for (var i = 0; i < this._offsetIndex; ++i) {
            var element = this.testElements[i];
            var context = element.getContext("2d");

            // Get image data
            var imageData = context.getImageData(0, 0, this.imageWidth, this.imageHeight);

            var didDraw = false,
                neighborPixelIndex,
                dataLen = imageData.data.length;
            for (var j = 0; j < dataLen; j += this.pixelStride) {
                if (imageData.data[j + 3] === 0)
                    continue;

                // get random neighboring pixel color
                neighborPixelIndex = this._getRandomNeighboringPixelIndex(j, dataLen);

                // Update the RGB data
                imageData.data[j] = imageData.data[neighborPixelIndex];
                imageData.data[j + 1] = imageData.data[neighborPixelIndex + 1];
                imageData.data[j + 2] = imageData.data[neighborPixelIndex + 2];
                imageData.data[j + 3] = imageData.data[neighborPixelIndex + 3];
                didDraw = true;
            }

            if (didDraw)
                context.putImageData(imageData, 0, 0);
            else {
                this._refreshElement(element);
                element.getContext("2d").drawImage(Stage.randomElementInArray(this.images), 0, 0, this.imageWidth, this.imageHeight);
            }
        }
    },

    _getRandomNeighboringPixelIndex: function(pixelIdx, pixelArrayLength)
    {
        var xOffset = Math.floor((Pseudo.random() - this.weightNegativeThreshold) / (this.weightPositiveThreshold - this.weightNegativeThreshold));
        var yOffset = Math.floor((Pseudo.random() - this.weightNegativeThreshold) / (this.weightPositiveThreshold - this.weightNegativeThreshold));
        return (pixelIdx + this.pixelStride * xOffset + this.rowStride * yOffset) % pixelArrayLength;
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
