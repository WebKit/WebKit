## Description

This benchmark content was created out of [WebKit's performance dashboard](https://perf.webkit.org/).

## What are we testing

 - Basic DOM
 - Web Components
 - ES2005+
 - Canvas Drawing
 - Inline SVG icons

## How are we testing

The test simulates a real-world user flow by loading "Charts" page with a few charts, selecting a few data points, and selecting a time range.

## Developer Documentation

The app was ported from https://github.com/WebKit/WebKit/tree/main/Websites/perf.webkit.org after removing non-essential files
and scraping JSON files off of https://perf.webkit.org/ and substituting RemoteAPI.sendHttpRequest in mockAPIs.

In order to update files, simply copy over files from the source directory.

Speedometer specific content is in index.html and main.js between "// BEGIN - Speedometer Specific Code" and "// END - Speedometer Specific Code".

./tools/bundle-v3-scripts.py is used to generate public/v3/bundled-scripts.js, which is a minified script file.

The tested URL was constructed by adding StyleBench charts for mac-ventura results on https://perf.webkit.org/: https://perf.webkit.org/v3/#/charts?since=1678991819934&paneList=((55-1649-53731881-null-(5-2.5-500))-(55-1407-null-null-(5-2.5-500))-(55-1648-null-null-(5-2.5-500))-(55-1974-null-null-(5-2.5-500)))
