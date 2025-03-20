# Chart benchmarks

## How to run locally

This project uses `npm` to manage dependencies and run scripts. It has been
tested with node v18 but should work with other versions of node.

This uses [vite](https://vitejs.dev/) as builder but this should be fairly
transparent.

### Install dependencies

```
npm i
```

### Run the development server

```
npm run dev
```

### Build the app

```
npm run build
```

### Preview the production build

```
npm run preview
```

## Included benchmarks

All the benchmarks are included in iframes in the index page. But it may be more
convenient to open these benchmarks with their specific files.

### [Observable Plot](https://github.com/observablehq/plot)

You can load this benchmark with the `/observable-plot.html` page, for example
http://localhost:5173/observable-plot.html if you run it locally or
http://localhost:7000/resources/charts/dist/observable-plot.html in the
context of the speedometer.

Observable Plot is D3-based and outputs SVG.

When run in development mode, the page will automatically execute the 3
included graphs:

-   a stacked bar graph
-   a grouped bar graph
-   a graph using dots

In production mode nothing executes by default, the user needs to push the
buttons to run any code. That's how the benchmark exercizes this code.

To build these graphs, we use datasets representing flight information in the
US. You can consult them in the [datasets directory](./datasets).

### [ChartJS](https://github.com/chartjs/Chart.js)

You can load this benchmark with the `/chartjs.html` page, for example
http://localhost:5173/chartjs.html if you run it locally or
http://localhost:7000/resources/charts/dist/chartjs.html in the
context of speedometer.

ChartJS is canvas-based.

When run in development mode, the page will automatically execute the scatter
graph.

In production mode nothing executes by default, the user needs to push the
buttons to run any code. That's how the benchmark exercizes this code.

To build these graphs, we use datasets representing flight information in the
US. You can consult them in the [datasets directory](./datasets).
