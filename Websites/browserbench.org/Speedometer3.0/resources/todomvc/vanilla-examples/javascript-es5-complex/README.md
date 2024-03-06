# Speedometer 3.0: TodoMVC: JavaScript Es5 Complex DOM

## Description

This application embeds the JavaScript Es5 implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [JavaScript Es5 README.md](../javascript-es5/README.md) for more information on the JavaScript Es5 TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone JavaScript Es5 TodoMVC need to be built before building the JavaScript Es5 Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../javascript-es5 && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:7001
```
