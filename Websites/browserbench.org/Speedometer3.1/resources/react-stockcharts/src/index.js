
import React from 'react';
import { render } from 'react-dom';
import Chart from './chart';
import { getData } from "./utils"

import { TypeChooser } from "react-stockcharts/lib/helper";

class ChartComponent extends React.Component {
    constructor() {
        super();
        this.state = { data: null, render: false };
        this.doRender = () => this.setState({ render: true });
    }
	componentDidMount() {
		getData().then(data => {
			this.setState({ data })
		})
	}
	render() {
        const { render, data } = this.state;
        if (!render) {
            return <button id="render" onClick={this.doRender}>render</button>;
        }
		if (data == null) {
			return <div>Loading...</div>
		}
        const type = new URLSearchParams(location.search).get('type') || 'hybrid';
		return (
			<TypeChooser type={type}>
				{type => <Chart type={type} data={data} />}
			</TypeChooser>
		)
	}
}

render(
	<ChartComponent />,
	document.getElementById("root")
);
