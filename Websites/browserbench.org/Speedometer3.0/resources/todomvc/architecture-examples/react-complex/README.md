# Speedometer 3.0: TodoMVC: React Complex DOM

## Description

This application embeds the React implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [React README.md](../react/README.md) for more information on the React TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone React TodoMVC need to be built before building the React Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../react && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:7002
```
