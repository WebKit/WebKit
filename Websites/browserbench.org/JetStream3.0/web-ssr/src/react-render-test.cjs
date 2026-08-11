const React = require('react');
const { renderToString } = require('react-dom/server');
const { WineList } = require('./components.cjs');
const { WINE_DATA } = require("./wine-data.cjs");

function renderTest() {
  const start = performance.now();
  const html = renderToString(<WineList wines={WINE_DATA} />);
  const end = performance.now(start);

  const duration = end - start;
  console.log(`renderToString took ${duration.toFixed(2)}ms`);

  return {
    duration,
    html: html,
  };
}

module.exports = {
  renderTest
};
