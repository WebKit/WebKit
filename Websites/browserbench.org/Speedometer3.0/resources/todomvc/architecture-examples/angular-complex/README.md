# Speedometer 3.0: TodoMVC: Angular Complex DOM

## Description

This application embeds the Angular implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [Angular README.md](../angular/README.md) for more information on the Angular TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone Angular TodoMVC need to be built before building the Angular Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../angular && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:8080
```
