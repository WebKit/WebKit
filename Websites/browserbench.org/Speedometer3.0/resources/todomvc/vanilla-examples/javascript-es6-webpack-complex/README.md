# Speedometer 3.0: TodoMVC: JavaScript Es6 with Webpack Complex DOM

## Description

This application embeds the JavaScript Es6 with Webpack implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [JavaScript Es6 with Webpack README.md](../javascript-es6-webpack/README.md) for more information on the JavaScript Es6 with Webpack TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone JavaScript Es6 with Webpack TodoMVC need to be built before building the JavaScript Es6 with Webpack Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../javascript-es6-webpack && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:7002
```
