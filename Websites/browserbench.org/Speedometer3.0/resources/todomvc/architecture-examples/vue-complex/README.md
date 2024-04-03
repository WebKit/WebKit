# Speedometer 3.0: TodoMVC: Vue Complex DOM

## Description

This application embeds the Vue implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [Vue README.md](../vue/README.md) for more information on the Vue TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone Vue TodoMVC need to be built before building the Vue Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../vue && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:7002
```
