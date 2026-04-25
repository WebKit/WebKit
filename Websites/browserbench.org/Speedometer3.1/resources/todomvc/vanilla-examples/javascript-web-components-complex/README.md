# Speedometer 3.0: TodoMVC: Web Components with Complex DOM

## Description

This application embeds the Web Components implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [JavaScript Web Components README.md](../javascript-web-components/README.md) for more information on the Web Components TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone JavaScript Web Components TodoMVC need to be built before building the JavaScript Web Components Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../javascript-web-components && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:7006
```
