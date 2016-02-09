(function() {

LayeringTextStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);
        this._textElementParent = this.element;
        this._textElements = [];
        this._textItemIndex = 0;
        this._colorIndex = 0;
        this._animateCounts = 0;
    },

    _nextTextItem: function(textItemFlags)
    {
        var textItem = LayeringTextStage.textItems[this._textItemIndex];
        Utilities.extendObject(textItemFlags, LayeringTextStage.textItemsFlags[this._textItemIndex]);
        this._textItemIndex = (this._textItemIndex + 1) % LayeringTextStage.textItems.length;
        return textItem;
    },

    _previousTextItem: function(textItemFlags)
    {
        this._textItemIndex = (this._textItemIndex + LayeringTextStage.textItems.length - 1) % LayeringTextStage.textItems.length;
        Utilities.extendObject(textItemFlags, LayeringTextStage.textItemsFlags[this._textItemIndex]);
        return LayeringTextStage.textItems[this._textItemIndex];
    },

    _pushTextElement: function()
    {
        var textItemFlags = {};
        var textItem = this._nextTextItem(textItemFlags);
        for ( ; textItemFlags.isClosing; textItem = this._nextTextItem(textItemFlags))
            this._textElementParent = this._textElementParent.parentNode;

        var parseResult = LayeringTextStage.parseTextItem(textItem);
        textItem = textItem.substring(parseResult.tagStart.length, textItem.length - parseResult.tagEnd.length);

        var textElement = document.createElement(parseResult.nodeName);

        for (var attrname in parseResult.args)
            textElement.setAttribute(attrname, parseResult.args[attrname]);

        this._textElementParent.appendChild(textElement);

        if (!parseResult.tagEnd.length)
            this._textElementParent = textElement;

        textElement.innerHTML = textItem;

        this._textElements.push(textElement);
        return this._textElements.length;
    },

    _popTextElement: function()
    {
        var textItemFlags = {};
        var textItem = this._previousTextItem(textItemFlags);
        for ( ; textItemFlags.isClosing; textItem = this._previousTextItem(textItemFlags))
            this._textElementParent = this._textElementParent.lastChild;

        if (textItemFlags.isOpening)
            this._textElementParent = this._textElementParent.parentNode;

        this._textElements[this._textElements.length - 1].remove();

        this._textElements.pop();
        return this._textElements.length;
    },

    _colorTextItem: function(color)
    {
        var textElementIndex = LayeringTextStage.colorIndexToTextElementIndex(this._colorIndex);
        for ( ; textElementIndex < this._textElements.length; textElementIndex += LayeringTextStage.insertableTextItems)
            this._textElements[textElementIndex].style.backgroundColor = color;
    },

    animate: function(timeDelta)
    {
        this._colorTextItem("transparent");
        this._colorIndex = (this._colorIndex + 1) % LayeringTextStage.colorableTextItems;
        this._colorTextItem("YellowGreen");

        var blackTextElements = Math.min(this._textElements.length, LayeringTextStage.insertableTextItems);
        var i = 0;
        for ( ; i < this._textElements.length - blackTextElements; ++i)
            this._textElements[i].style.color = (this._animateCounts & 1) ? "LightYellow" : "white";

        for ( ; i < this._textElements.length; ++i)
            this._textElements[i].style.color = "black";

        ++this._animateCounts;
    },

    tune: function(count)
    {
        if (count == 0)
            return this._textElements.length;

        if (count > 0) {
            for (var i = 0; i < count; ++i)
                this._pushTextElement();
            return this._textElements.length;
        }

        count = Math.min(-count, this._textElements.length);
        for (var i = 0; i < count; ++i)
            this._popTextElement();

        return this._textElements.length;
    },

    complexity: function()
    {
        return this._textElements.length;
    }
});

Utilities.extendObject(LayeringTextStage, {
    textItems: [
        "<div class='text-layer'>",
            "<h2>Types of benchmarks</h2>",
            "<ol>",
                "<li>Real program",
                    "<ul>",
                        "<li>word processing software</li>",
                        "<li>tool software of CAD</li>",
                        "<li>user's application software (i.e.: MIS)</li>",
                    "</ul>",
                "</li>",
                "<li>Kernel",
                    "<ul>",
                        "<li>contains key codes</li>",
                        "<li>normally abstracted from actual program</li>",
                        "<li>popular kernel: Livermore loop</li>",
                        "<li>linpack benchmark (contains basic linear algebra subroutine written in FORTRAN language)</li>",
                        "<li>results are represented in MFLOPS</li>",
                    "</ul>",
                "</li>",
                "<li>Synthetic Benchmark",
                    "<ul>",
                        "<li>Procedure for programming synthetic benchmark:",
                            "<ul>",
                                "<li>take statistics of all types of operations from many application programs</li>",
                                "<li>get proportion of each operation</li>",
                                "<li>write program based on the proportion above</li>",
                            "</ul>",
                        "</li>",
                        "<li>Types of Synthetic Benchmark are:",
                            "<ul>",
                                "<li>Whetstone</li>",
                                "<li>Dhrystone</li>",
                            "</ul>",
                        "</li>",
                        "<li>These were the first general purpose industry standard computer benchmarks. They do not necessarily obtain high scores on modern pipelined computers.</li>",
                    "</ul>",
                "</li>",
                "<li>I/O benchmarks</li>",
                "<li>Database benchmarks: to measure the throughput and response times of database management systems (DBMS')</li>",
                "<li>Parallel benchmarks: used on machines with multiple cores, processors or systems consisting of multiple machines</li>",
            "</ol>",
            "<h2>Common benchmarks</h2>",
            "<ul>",
                "<li>Business Applications Performance Corporation (BAPCo)</li>",
                "<li>Embedded Microprocessor Benchmark Consortium (EEMBC)</li>",
                "<li>Standard Performance Evaluation Corporation (SPEC), in particular their SPECint and SPECfp</li>",
                "<li>Transaction Processing Performance Council (TPC)</li>",
                "<li>Coremark: Embedded computing benchmark</li>",
            "</ul>",
            "<h3>Open source benchmarks</h3>",
            "<ul>",
                "<li>AIM Multiuser Benchmark: composed of a list of tests that could be mixed to create a 'load mix' that would simulate a specific computer function on any UNIX-type OS.</li>",
                "<li>Bonnie++: filesystem and hard drive benchmark</li>",
                "<li>BRL-CAD: cross-platform architecture-agnostic benchmark suite based on multithreaded ray tracing performance; baselined against a VAX-11/780; and used since 1984 for evaluating relative CPU performance, compiler differences, optimization levels, coherency, architecture differences, and operating system differences.</li>",
            "</ul>",
        "</div>"
    ],

    parseTextItem: function(textItem)
    {
        var parseResult = {};
        parseResult.tagStart = textItem.match(/<(.*?)>/g)[0];
        var spaceIndex = parseResult.tagStart.indexOf(" ");
        parseResult.nodeName = parseResult.tagStart.substring(1, spaceIndex != -1 ? spaceIndex : parseResult.tagStart.length - 1);
        parseResult.args = spaceIndex != -1 ? Utilities.parseArguments(parseResult.tagStart.substring(spaceIndex + 1, parseResult.tagStart.length - 1)) : {};
        var tagEnd = "</" + parseResult.nodeName + ">";
        parseResult.tagEnd = textItem.endsWith(tagEnd) ? tagEnd : "";
        return parseResult;
    },

    isOpeningTextItem: function(textItem)
    {
        return !LayeringTextStage.parseTextItem(textItem).tagEnd.length;
    },

    isClosingTextItem: function(textItem)
    {
        return textItem.indexOf("/") == +1;
    },

    isColorableTextItem: function(textItemFlags)
    {
        return !(textItemFlags.isOpening || textItemFlags.isClosing);
    },

    isInsertableTextItem: function(textItemFlags)
    {
        return !textItemFlags.isClosing;
    },

    colorIndexToTextElementIndex: function(colorIndex)
    {
        var textElementIndex = 0;
        var index = 0;

        for (var textItemIndex = 0; textItemIndex < LayeringTextStage.textItemsFlags.length; ++textItemIndex) {
            if (LayeringTextStage.isColorableTextItem(LayeringTextStage.textItemsFlags[textItemIndex])) {
                if (++index > colorIndex)
                    break;
            }
            if (LayeringTextStage.isInsertableTextItem(LayeringTextStage.textItemsFlags[textItemIndex]))
                ++textElementIndex;
        }

        return textElementIndex;
    }
});

Utilities.extendObject(LayeringTextStage, {
    textItemsFlags: LayeringTextStage.textItems.map(function(textItem)
    {
       var textItemFlags = {};
       textItemFlags.isOpening = LayeringTextStage.isOpeningTextItem(textItem);
       textItemFlags.isClosing = LayeringTextStage.isClosingTextItem(textItem);
       return textItemFlags;
    })
});

Utilities.extendObject(LayeringTextStage, {
    colorableTextItems: LayeringTextStage.textItemsFlags.filter(function(textItemFlags)
    {
        return LayeringTextStage.isColorableTextItem(textItemFlags);
    }).length,

    insertableTextItems: LayeringTextStage.textItemsFlags.filter(function(textItemFlags)
    {
        return LayeringTextStage.isInsertableTextItem(textItemFlags);
    }).length
});

LayeringTextBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new LayeringTextStage(), options);
    }
);

window.benchmarkClass = LayeringTextBenchmark;

})();

