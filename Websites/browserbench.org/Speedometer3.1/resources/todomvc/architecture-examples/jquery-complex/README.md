# Speedometer 3.0: TodoMVC: jQuery Complex DOM

## Description

This application embeds the jQuery implementation of the TodoMVC application in a static UI shell that mimics a complex web page.

Please refer to the [jQuery README.md](../jquery/README.md) for more information on the jQuery TodoMVC implementation.

Please refer to the [big-dom-generator README.md](../../big-dom-generator/README.md) for more information on the UI shell.

## Build steps

Big-dom-generator and standalone jQuery TodoMVC need to be built before building the jQuery Complex DOM TodoMVC.

```
terminal
1. pushd ../../big-dom-generator && npm install && npm run build && popd
2. pushd ../jquery && npm install && npm run build && popd
3. npm run build
```

## Local preview

```
terminal:
1. npm run serve
browser:
1. http://localhost:7002
```
