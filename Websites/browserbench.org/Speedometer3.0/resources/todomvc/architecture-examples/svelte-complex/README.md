# Speedometer 3.0: TodoMVC: Svelte Complex DOM

## Description

This application embeds the Svelte implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [Svelte README.md](../svelte/README.md) for more information on the Svelte TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone Svelte TodoMVC need to be built before building the Svelte Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../svelte && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:7002
```
