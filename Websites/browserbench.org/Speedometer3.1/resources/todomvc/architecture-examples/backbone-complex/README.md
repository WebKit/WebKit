# Speedometer 3.0: TodoMVC: Backbone Complex DOM

## Description

This application embeds the Backbone implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [Backbone README.md](../backbone/README.md) for more information on the Backbone TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone Backbone TodoMVC need to be built before building the Backbone Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../backbone && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:7002
```
