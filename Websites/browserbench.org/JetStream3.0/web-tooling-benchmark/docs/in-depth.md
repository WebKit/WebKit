# In-depth Analysis

The Web Tooling Benchmark contains interesting JavaScript workloads from a variety of
commonly used web developer tools. It executes these core workloads a couple of times
reports a single score at the end that balances them using geometric mean.

Note that scores of different versions of the Web Tooling Benchmark are not comparable
to each other.

## acorn

[Acorn](https://github.com/ternjs/acorn) is a tiny, fast JavaScript parser, written
completely in JavaScript. Acorn is the basis for many other JavaScript compiler
and code analysis tools, i.e. [webpack](https://webpack.js.org) uses acorn and
the [Babylon](https://github.com/babel/babylon) parser used by
[Babel](http://babeljs.io) today is also based on acorn.

This benchmarks runs the Acorn tokenizer, the parser and the full AST (Abstract
Syntax Tree) walker on several popular inputs, including
- the [Preact](https://github.com/developit/preact) 8.2.5 bundle,
- the [lodash](https://lodash.com) 4.17.4 bundle,
- the [underscore](http://underscorejs.org/) 1.8.3 bundle,
- an ES2015 module containing the untranspiled [Vue](https://github.com/vuejs/vue) bundle,
- the [jQuery](http://jquery.com) 3.2.1 distribution,
- the minified [Redux](https://redux.js.org) 3.7.2 distribution,
- the [Backbone.js](http://backbonejs.org) 1.1.0 bundle,
- and the (concatenated) JavaScript source for the Vanilla ES2015 test in the [Speedometer](https://browserbench.org/Speedometer).

## babel

[Babel](https://github.com/babel/babel) is a transpiler that compiles modern JavaScipt
(i.e. ES2015 and later) to an older JavaScript dialect (i.e. ES3 or ES5) that is understood by a broad set of browsers. It's probably the most popular transpiler at
this point in time and used in the vast majority of web projects nowadays.

This benchmark runs the Babel transformation logic using the `es2015` preset on a 196KiB
ES2015 module containing the untranspiled [Vue](https://github.com/vuejs/vue) bundle.
Note that this explicitly excludes the [Babylon](https://github.com/babel/babylon) parser
and only measures the throughput of the actual transformations. The parser is tested
separately by the `babylon` benchmark below.

## babel-minify

[Babel Minify](https://github.com/babel/minify) is an ES2015+ aware minifier based on the Babel toolchain. It is written as a set of Babel plugins.

This benchmark stresses the babel minifier on the (concatenated) JavaScript source for the ES2015 test in the [Speedometer](https://browserbench.org/Speedometer) 2.0 benchmark.

## babylon

[Babylon](https://github.com/babel/babylon) is the frontend for the Babel transpiler and
deals with transforming the JavaScript input into a so-called AST (Abstract Syntax Tree)
that can be consumed by the core transformation logic in Babel. It is built on top of
[Acorn](https://github.com/ternjs/acorn) nowadays.

This benchmark runs the Babylon parser on different popular inputs, including the
[Preact](https://github.com/developit/preact) 8.2.5 bundle, the [lodash](https://lodash.com)
4.17.4 bundle, the JSX files from the React TodoMVC example app, the
[underscore](http://underscorejs.org/) 1.8.3 bundle, an ES2015 module containing the
untranspiled [Vue](https://github.com/vuejs/vue) bundle, the [jQuery](http://jquery.com)
3.2.1 distribution, the minified [Redux](https://redux.js.org) 3.7.2 distribution, and
the (concatenated) JavaScript source for the Vanilla ES2015 test in the
[Speedometer](https://browserbench.org/Speedometer).

## buble

A test to stress the [Buble](https://github.com/Rich-Harris/buble)
ES2015 compiler, both the parser and the actual transformation
logic, on the same 196K ES2015 module containing the untranspiled
[Vue](https://github.com/vuejs/vue) source code that is also used
to stress by the `babel` and `babylon` tests.

## chai

[Chai](http://chaijs.com) is a popular BDD / TDD assertion library for
[Node](https://www.nodejs.org) and the browser that can be delightfully
paired with any test driver framework for JavaScript. It is commonly used
to write unit and integration tests. As such this test is not just a good
benchmark for web tooling on the [Node](https://www.nodejs.org) side, but
is also very relevant to the browser as tests often need to be run in the
browser too.

This benchmark is based on the test suite that comes with Chai and
essentially uses the Chai BDD style interface to test the Chai library
itself.

## coffeescript

[CoffeeScript](http://coffeescript.org/) is a little language that attempts to expose
the good parts of JavaScript in a simple way. At some point CoffeeScript was very
popular.

This benchmark runs the CoffeeScript compiler on the [`lexer.coffee`](https://github.com/v8/web-tooling-benchmark/blob/third_party/coffeescript-lexer-2.0.1.coffee)
file from the CoffeeScript 2.0.1 distribution.

## espree

[Espree](https://github.com/eslint/espree) started out as a fork of Esprima v1.2.2,
the last stable published released of Esprima before work on ECMAScript 6 began.
Espree is now built on top of Acorn, which has a modular architecture that allows
extension of core functionality. The goal of Espree is to produce output that is
similar to Esprima with a similar API so that it can be used in place of Esprima.
The popular analysis framework [ESLint](https://eslint.org) is built on Espree
today.

This benchmark runs Espree on a several common scripts, including
- the [Backbone.js](http://backbonejs.org) 1.1.0 bundle,
- the [jQuery](http://jquery.com) 3.2.1 distribution,
- the [MooTools](https://mootools.net) 1.6.0 bundle,
- and the [underscore](http://underscorejs.org/) 1.8.3 bundle.

## esprima

[Esprima](http://esprima.org) is a high performance, standard-compliant ECMAScript parser
written in ECMAScript, which can be used to perform [lexical analysis](https://en.wikipedia.org/wiki/Lexical_analysis)
(tokenization) or [syntactic analysis](https://en.wikipedia.org/wiki/Parsing) (parsing)
of an ECMAScript program. Esprima was initially used as the basis for several tools like
ESLint and Babel, and is still in use by a lot of analysis tools.

This benchmark runs both the tokenizer and the parser on a variety of common scripts,
including
- the [Backbone.js](http://backbonejs.org) 1.1.0 bundle,
- the [jQuery](http://jquery.com) 3.2.1 distribution,
- the [MooTools](https://mootools.net) 1.6.0 bundle,
- and the [underscore](http://underscorejs.org/) 1.8.3 bundle.

## jshint

[JSHint](http://jshint.com) is a program that flags suspicious usage in programs written
in JavaScript. At some point it was very popular, but nowadays it seems that many projects
have switch or are switching to [ESLint](https://eslint.org).

This benchmark runs JSHint on
- the [lodash](https://lodash.com) 4.17.4 bundle,
- the [Preact](http://preactjs.com) 8.2.5 bundle,
- and the [underscore](http://underscorejs.org) 1.8.3 distribution.

## lebab

[Lebab](https://github.com/lebab/lebab) transpiles your ES5 code to ES6/ES7, thus
performing the opposite of what [Babel](https://github.com/babel/babel) does.

This benchmark runs the Lebal ES5 to ES6/ES7 transpiler on the
[Preact](http://preactjs.com) 8.2.5 bundle.

## postcss

[PostCSS](https://github.com/postcss/postcss) is a tool for transforming styles with JS plugins.
These plugins can lint your CSS, support variables and mixins, transpile future CSS syntax,
inline images, and more.

This benchmark runs the PostCSS processor with [Autoprefixer](https://github.com/postcss/autoprefixer)
and [postcss-nested](https://github.com/postcss/postcss-nested) plugins on
- the [Bootstrap](https://getbootstrap.com/) 4.0.0 bundle.
- the [Foundation](https://foundation.zurb.com/) 6.4.2 bundle.
- the [Angular Material](https://material.angularjs.org) 1.1.8 bundle.

## prepack

[Prepack](https://prepack.io) is a tool that optimizes JavaScript source code:
Computations that can be done at compile-time instead of run-time get eliminated.
Prepack replaces the global code of a JavaScript bundle with equivalent code that
is a simple sequence of assignments. This gets rid of most intermediate computations
and object allocations. It currently focuses on React Native use cases.

This benchmark runs prepack on both
- the [Preact](https://github.com/developit/preact) 8.2.5 bundle (unminified) as well as
- the [Redux](https://github.com/reactjs/redux) 3.7.2 bundle (minified).

## prettier

[Prettier](https://github.com/prettier/prettier) is an opinionated code formatter
which removes all the original styling and ensures that all outputted code conforms
to a consistent style. It is often used in web projects today to automatically format
JavaScript, HTML, CSS and other files.

This benchmark runs prettier on different inputs, including
- the [lodash](https://lodash.com) 4.17.4 bundle,
- the [Preact](http://preactjs.com) 8.2.5 bundle, and
- the JSX files from the React [todomvc](https://github.com/tastejs/todomvc) example app.

## source-map

[Source Map](https://github.com/mozilla/source-map) is a library developed by Mozilla
to generate and consume source maps, which in turn are used by browsers to display
proper JavaScript when debugging a minified application.

This benchmark stresses the source-map tool on both parsing and serializing a
variety of different source maps, including the [Preact](http://preactjs.com)
8.2.5 source map.

## terser

[terser](https://github.com/fabiosantoscode/terser) is a fork of `uglify-es` that retains API and CLI compatibility with `uglify-es` and `uglify-js@3`.

This benchmark stresses the new ES2015-and-beyond minifier on the (concatenated) JavaScript source for the ES2015 test in the [Speedometer](https://browserbench.org/Speedometer) 2.0 benchmark.

## typescript

[TypeScript](https://github.com/Microsoft/TypeScript) is a language for
application-scale JavaScript. It adds optional types, classes, and modules
to JavaScript, and compiles to readable, standards-based JavaScript.

This benchmark stresses the TypeScript compiler by running it on the
[`typescript-angular`](https://github.com/tastejs/todomvc/tree/master/examples/typescript-angular)
example from [todomvc](https://github.com/tastejs/todomvc).

## uglify-js

[UglifyJS](https://github.com/mishoo/UglifyJS2) is a JavaScript parser, minifier,
compressor and beautifier toolkit, which is commonly used to minimize JavaScript
bundles.

This benchmark runs the UglifyJS minifier on the (concatenated) JavaScript source for
the ES2015 test in the [Lodash](https://lodash.com) module.
