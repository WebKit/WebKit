
import React from "react";
import PropTypes from "prop-types";

import { format } from "d3-format";
import { timeFormat } from "d3-time-format";

import { ChartCanvas, Chart } from "react-stockcharts";
import {
	BarSeries,
	StraightLine,
	CandlestickSeries,
	LineSeries,
	StochasticSeries,
} from "react-stockcharts/lib/series";
import { XAxis, YAxis } from "react-stockcharts/lib/axes";
import {
	CrossHairCursor,
	EdgeIndicator,
	CurrentCoordinate,
	MouseCoordinateX,
	MouseCoordinateY,
} from "react-stockcharts/lib/coordinates";
import {
	OHLCTooltip,
	MovingAverageTooltip,
	StochasticTooltip,
} from "react-stockcharts/lib/tooltip";

import { discontinuousTimeScaleProvider } from "react-stockcharts/lib/scale";
import { ema, stochasticOscillator } from "react-stockcharts/lib/indicator";
import { fitWidth } from "react-stockcharts/lib/helper";
import { last } from "react-stockcharts/lib/utils";

const stoAppearance = {
	stroke: Object.assign({},
		StochasticSeries.defaultProps.stroke)
};

class BenchmarkChart extends React.Component {
	render() {
		const overallHeight = 500;
		const numCharts = 5;
		const chartHeight = overallHeight / numCharts;
		const { type, data: initialData, width, ratio } = this.props;

		const ema10 = ema()
			.options({ windowSize: 10 })
			.merge((d, c) => {d.ema10 = c;})
			.accessor(d => d.ema10);

		const ema20 = ema()
			.options({ windowSize: 12 })
			.merge((d, c) => {d.ema20 = c;})
			.accessor(d => d.ema20);

		const slowSTO = stochasticOscillator()
			.options({ windowSize: 10, kWindowSize: 3 })
			.merge((d, c) => {d.slowSTO = c;})
			.accessor(d => d.slowSTO);
		const fastSTO = stochasticOscillator()
			.options({ windowSize: 10, kWindowSize: 1 })
			.merge((d, c) => {d.fastSTO = c;})
			.accessor(d => d.fastSTO);
		const fullSTO = stochasticOscillator()
			.options({ windowSize: 10, kWindowSize: 3, dWindowSize: 4 })
			.merge((d, c) => {d.fullSTO = c;})
			.accessor(d => d.fullSTO);

		const calculatedData = ema10(ema20(slowSTO(fastSTO(fullSTO(initialData)))));
		const xScaleProvider = discontinuousTimeScaleProvider
			.inputDateAccessor(d => d.date);
		const {
			data,
			xScale,
			xAccessor,
			displayXAccessor,
		} = xScaleProvider(calculatedData);

		const start = xAccessor(last(data));
		const end = xAccessor(data[Math.max(0, data.length - 150)]);
		const xExtents = [start, end];

		return (
			<ChartCanvas height={overallHeight}
				width={width}
				ratio={ratio}
				type={type}
				seriesName="COMPANY"
				data={data}
				xScale={xScale}
				xAccessor={xAccessor}
				displayXAccessor={displayXAccessor}
				xExtents={xExtents}
			>
				<Chart id={1} height={chartHeight}
					yExtents={d => [d.high, d.low]}
				>
					<YAxis axisAt="right" orient="right" ticks={5} />
					<XAxis axisAt="bottom" orient="bottom" showTicks={false} />

					<CandlestickSeries />

					<LineSeries yAccessor={ema10.accessor()} stroke={ema10.stroke()}/>
					<LineSeries yAccessor={ema20.accessor()} stroke={ema20.stroke()}/>

					<OHLCTooltip/>
				</Chart>
				<Chart id={2}
					yExtents={d => d.volume}
					height={chartHeight}
					origin={[0, chartHeight]}
				>
					<YAxis axisAt="left" orient="left" ticks={5} tickFormat={format(".2s")}/>

					<BarSeries yAccessor={d => d.volume}/>
				</Chart>
				<Chart id={3}
					yExtents={[0, 100]}
					height={chartHeight}
					origin={[0, 2 * chartHeight]}
				>
					<XAxis axisAt="bottom" orient="bottom" showTicks={false} />
					<YAxis axisAt="right" orient="right"/>

					<StochasticSeries
						yAccessor={d => d.fastSTO}
						{...stoAppearance} />

					<StochasticTooltip
						yAccessor={slowSTO.accessor()}
						options={slowSTO.options()}
						appearance={stoAppearance}
						label="Slow STO" />

				</Chart>
				<Chart id={4}
					yExtents={[0, 100]}
					height={chartHeight}
					origin={[0, 3 * chartHeight]}
				>
					<XAxis axisAt="bottom" orient="bottom" showTicks={false} />
					<YAxis axisAt="right" orient="right"/>

					<StochasticSeries
						yAccessor={fastSTO.accessor()}
						{...stoAppearance} />

					<StochasticTooltip
						yAccessor={fastSTO.accessor()}
						options={fastSTO.options()}
						appearance={stoAppearance}
						label="Fast STO" />
				</Chart>
				<Chart id={5}
					yExtents={[0, 100]}
					height={chartHeight}
					origin={[0, 4 * chartHeight]}
				>
					<XAxis axisAt="bottom" orient="bottom" />
					<YAxis axisAt="right" orient="right"/>

					<MouseCoordinateX
						at="bottom"
						orient="bottom"
						displayFormat={timeFormat("%Y-%m-%d")} />
					<MouseCoordinateY
						at="right"
						orient="right"
						displayFormat={format(".2f")} />
					<StochasticSeries
						yAccessor={fullSTO.accessor()}
						{...stoAppearance} />

					<StochasticTooltip
						yAccessor={fullSTO.accessor()}
						options={fullSTO.options()}
						appearance={stoAppearance}
						label="Full STO" />
				</Chart>
			</ChartCanvas>
		);
	}
}
BenchmarkChart.propTypes = {
	data: PropTypes.array.isRequired,
	width: PropTypes.number.isRequired,
	ratio: PropTypes.number.isRequired,
	type: PropTypes.oneOf(["svg", "hybrid"]).isRequired,
};

BenchmarkChart.defaultProps = {
	type: "svg",
};

BenchmarkChart = fitWidth(BenchmarkChart);

export default BenchmarkChart;
