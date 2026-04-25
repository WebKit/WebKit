Uses [react-stockcharts](https://github.com/rrag/react-stockcharts/blob/master/LICENSE)

Benchmark page adapted from [github.com/rrag/react-stockcharts-examples2/examples/CandleStickChartWithFullStochasticsIndicator](https://github.com/rrag/react-stockcharts-examples2/tree/master/examples/CandleStickChartWithFullStochasticsIndicator)

License: [MIT](https://github.com/rrag/react-stockcharts/blob/master/LICENSE)


## Building react-stockcharts locally
It’s very difficult to change the react-stockcharts code in its current state.
Here is a way to do it:
1. remove node_modules at the project root
2. Run: `DISABLE_ESLINT_PLUGIN=true nvm exec v16 npm run start`
   That is: disable eslint, and run with node v16 (because v18 doesn’t work).

This command needs [nvm](https://github.com/nvm-sh/nvm) to manage your local
node installations.
