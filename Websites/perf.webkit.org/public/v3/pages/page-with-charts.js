
class PageWithCharts extends PageWithHeading {
    constructor(name, toolbar)
    {
        super(name, toolbar);
        this._charts = [];
    }

    static createChartSourceList(platformId, metricId)
    {
        var platform = Platform.findById(platformId);
        var metric = Metric.findById(metricId);
        if (!platform || !metric)
            return {error: `Invalid platform or metric: ${platformId} and ${metricId}`};

        var lastModified = platform.lastModified(metric);
        if (!lastModified)
            return {platform: platform, metric: metric, error: `No results on ${platform.name()}`};

        var measurementSet = MeasurementSet.findSet(platform.id(), metric.id(), lastModified);
        var sourceList = [
            this.baselineStyle(measurementSet, 'baseline'),
            this.targetStyle(measurementSet, 'target'),
            this.currentStyle(measurementSet, 'current'),
        ];

        return {
            platform: platform,
            metric: metric,
            sourceList: sourceList,
        };
    }

    static baselineStyle(measurementSet)
    {
        return {
            measurementSet: measurementSet,
            extendToFuture: true,
            sampleData: true,
            type: 'baseline',
            pointStyle: '#f33',
            pointRadius: 2,
            lineStyle: '#f99',
            lineWidth: 1.5,
            intervalStyle: '#fdd',
            intervalWidth: 2,
        };
    }

    static targetStyle(measurementSet)
    {
        return {
            measurementSet: measurementSet,
            extendToFuture: true,
            sampleData: true,
            type: 'target',
            pointStyle: '#33f',
            pointRadius: 2,
            lineStyle: '#99f',
            lineWidth: 1.5,
            intervalStyle: '#ddf',
            intervalWidth: 2,
        };
    }

    static currentStyle(measurementSet)
    {
        return {
            measurementSet: measurementSet,
            sampleData: true,
            type: 'current',
            pointStyle: '#333',
            pointRadius: 2,
            lineStyle: '#999',
            lineWidth: 1.5,
            intervalStyle: '#ddd',
            intervalWidth: 2,
            interactive: true,
        };
    }

    static dashboardOptions(valueFormatter)
    {
        return {
            updateOnRequestAnimationFrame: true,
            axis: {
                yAxisWidth: 4, // rem
                xAxisHeight: 2, // rem
                gridStyle: '#ddd',
                fontSize: 0.8, // rem
                valueFormatter: valueFormatter,
            },
        };
    }

    static overviewChartOptions(valueFormatter)
    {
        var options = this.dashboardOptions(valueFormatter);
        options.axis.yAxisWidth = 0; // rem
        options.selection = {
            lineStyle: '#f93',
            lineWidth: 2,
            fillStyle: 'rgba(153, 204, 102, .125)',
        }
        return options;
    }

    static mainChartOptions(valueFormatter)
    {
        var options = this.dashboardOptions(valueFormatter);
        options.axis.xAxisEndPadding = 5;
        options.axis.yAxisWidth = 5;
        options.selection = {
            lineStyle: '#f93',
            lineWidth: 2,
            fillStyle: 'rgba(153, 204, 102, .125)',
        }
        options.indicator = {
            lineStyle: '#f93',
            lineWidth: 2,
            pointRadius: 3,
        };
        options.annotations = {
            textStyle: '#000',
            textBackground: '#fff',
            minWidth: 3,
            barHeight: 7,
            barSpacing: 2,
        };
        return options;
    }
}
