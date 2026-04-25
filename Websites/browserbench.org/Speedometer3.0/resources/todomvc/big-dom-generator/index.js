import React from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { App } from "./src/app";

const fs = require("fs");

const html = `<!DOCTYPE html>
<html lang="en" class="spectrum spectrum--medium spectrum--light">
  <head>
    <title>Big Todo App</title>
    <link rel="stylesheet" href="big-dom.css">
  </head>
  <body>
    ${renderToStaticMarkup(<App />)}
  </body>
</html>`;
fs.writeFileSync("dist/index.html", html);
