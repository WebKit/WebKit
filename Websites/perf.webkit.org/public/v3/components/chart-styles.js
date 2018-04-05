
class ChartStyles {
    static resolveConfiguration(platformId, metricId)
    {
        var platform = Platform.findById(platformId);
        var metric = Metric.findById(metricId);
        if (!platform || !metric)
            return {error: `Invalid platform or metric: ${platformId} and ${metricId}`};

        var lastModified = platform.lastModified(metric);
        if (!lastModified)
            return {platform: platform, metric: metric, error: `No results on ${platform.name()}`};

        return {
            platform: platform,
            metric: metric,
        };
    }

    static createSourceList(platform, metric, disableSampling, includeOutlier, showPoint)
    {
        console.assert(platform instanceof Platform);
        console.assert(metric instanceof Metric);

        var lastModified = platform.lastModified(metric);
        console.assert(lastModified);

        var measurementSet = MeasurementSet.findSet(platform.id(), metric.id(), lastModified);
        return [
            this.baselineStyle(measurementSet, disableSampling, includeOutlier, showPoint),
            this.targetStyle(measurementSet, disableSampling, includeOutlier, showPoint),
            this.currentStyle(measurementSet, disableSampling, includeOutlier, showPoint),
        ];
    }

    static baselineStyle(measurementSet, disableSampling, includeOutlier, showPoint)
    {
        return {
            measurementSet: measurementSet,
            extendToFuture: true,
            sampleData: !disableSampling,
            includeOutliers: includeOutlier,
            type: 'baseline',
            pointStyle: '#f33',
            pointRadius: showPoint ? 2 : 0,
            lineStyle: showPoint ? '#f99' : '#f66',
            lineWidth: 1.5,
            intervalStyle: 'rgba(255, 153, 153, 0.25)',
            intervalWidth: 3,
            foregroundLineStyle: '#f33',
            foregroundPointRadius: 0,
            backgroundIntervalStyle: 'rgba(255, 153, 153, 0.1)',
            backgroundPointStyle: '#f99',
            backgroundLineStyle: '#fcc',
            interactive: true,
        };
    }

    static targetStyle(measurementSet, disableSampling, includeOutlier, showPoint)
    {
        return {
            measurementSet: measurementSet,
            extendToFuture: true,
            sampleData: !disableSampling,
            includeOutliers: includeOutlier,
            type: 'target',
            pointStyle: '#33f',
            pointRadius: showPoint ? 2 : 0,
            lineStyle: showPoint ? '#99f' : '#66f',
            lineWidth: 1.5,
            intervalStyle: 'rgba(153, 153, 255, 0.25)',
            intervalWidth: 3,
            foregroundLineStyle: '#33f',
            foregroundPointRadius: 0,
            backgroundIntervalStyle: 'rgba(153, 153, 255, 0.1)',
            backgroundPointStyle: '#99f',
            backgroundLineStyle: '#ccf',
        };
    }

    static currentStyle(measurementSet, disableSampling, includeOutlier, showPoint)
    {
        return {
            measurementSet: measurementSet,
            sampleData: !disableSampling,
            includeOutliers: includeOutlier,
            type: 'current',
            pointStyle: '#333',
            pointRadius: showPoint ? 2 : 0,
            lineStyle: showPoint ? '#999' : '#666',
            lineWidth: 1.5,
            intervalStyle: 'rgba(153, 153, 153, 0.25)',
            intervalWidth: 3,
            foregroundLineStyle: '#333',
            foregroundPointRadius: 0,
            backgroundIntervalStyle: 'rgba(153, 153, 153, 0.1)',
            backgroundPointStyle: '#999',
            backgroundLineStyle: '#ccc',
            interactive: true,
        };
    }

    static dashboardOptions(valueFormatter)
    {
        return {
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
            lineStyle: 'rgba(51, 204, 255, .5)',
            lineWidth: 2,
            fillStyle: 'rgba(51, 204, 255, .125)',
        }
        return options;
    }

    static mainChartOptions(valueFormatter)
    {
        var options = this.dashboardOptions(valueFormatter);
        options.axis.xAxisEndPadding = 5;
        options.axis.yAxisWidth = 5;
        options.zoomButton = true;
        options.selection = {
            lineStyle: '#3cf',
            lineWidth: 2,
            fillStyle: 'rgba(51, 204, 255, .125)',
        }
        options.indicator = {
            lineStyle: '#3cf',
            lineWidth: 2,
            pointRadius: 2,
        };
        options.lockedIndicator = {
            fillStyle: '#fff',
            lineStyle: '#36c',
            lineWidth: 2,
            pointRadius: 3,
        };
        options.annotations = {
            textStyle: '#000',
            textBackground: '#fff',
            minWidth: 3,
            barHeight: 7,
            barSpacing: 2
        };
        return options;
    }

    static annotationFillStyleForTask(task) {
        if (!task)
            return '#888';

        switch (task.changeType()) {
            case 'inconclusive':
                return '#fcc';
            case 'progression':
                return '#39f';
            case 'regression':
                return '#c60';
            case 'unchanged':
                return '#ccc';
        }
        return '#fc6';

    }
}
