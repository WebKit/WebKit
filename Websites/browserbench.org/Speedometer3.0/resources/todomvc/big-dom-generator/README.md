# TodoMVC embedded in a complex static UI shell

The complex DOM workloads embed the different TodoMVC implementations in a static UI shell that mimics a complex web page or application. This UI shell is made up of a large DOM and complex CSS. The purpose is to capture the performance impact on executing seemingly isolated actions (e.g. adding/deleting todo items) in the context of a complex website.

CSS complexity is represented by rules composed by complex selectors and combinators. These CSS rules are further split into **matching** and **non-matching** rules. The matching selectors fully match an element added by the TodoMVC benchmark, but not elements in the UI shell. The non-matching selectors partially match elements added by the TodoMVC benchmark (at least the right most selectors do), but entirely match the elements in the UI shell. This ensures that the performance impact of both the matching and non-matching CSS rules is captured within the measured time.

## Complex DOM workload description

The static UI shell has the following characteristics:

-   The DOM has around 6000 elements.
-   The static UI shell is styled using the @spectrum-css Adobe library, which relies on css variables for uniform styling.
-   The @spectrum-css rules of the static UI shell are post processed using postcss and purgecss.
-   Additional styling is applied through ~50 non-matching CSS rules.
    -   E.g. `.backlog-group li > div > :checked ~ label`.

All TodoMVC implementations now have a `show-priority` class name in the `<ul>` element and a `data-priority` attribute in the `li` elements. These are used by the matching CSS rules.

-   There are ~14 added matching CSS rules.
    -   E.g. `.todo-area .show-priority li[data-priority="2"].completed`.
    -   In the case of Javascript-web-components and lit TodoMVC, the `show-priority` class is added to the `<todo-list>` custom element and the `data-priority` attribute to the `<todo-item>` custom element.

### Web Components based workloads

The Shadow DOM isolation prevents global CSS rules to be applied to its children, hence, having matching or non-matching rules does not have the same impact as it has on the other architectures. Instead, the Complex DOM Web components and Lit workloads stress the use of CSS variables inherited though the shadow DOM boundary:

-   Some default variables are defined in the `root` element.
-   The `add-todo-list-extra-css.js` file defines a constructable stylesheet that will declare the priority colors only if the `show-priority` class is present in the host.
-   The `add-todo-item-extra-css.js` file defines the rule to style the todo items based on their priority level.

## Static UI shell

<p align = "center">
<img src="complex-dom-workload.jpeg" alt="workload" width="800"/>
</p>

## The generator

The big DOM is produced by the generator in a nodejs script that uses `renderToStaticMarkup` to generate the static html. It uses a random seedable library to synthesize a folder-like structure embedded in the sidebar. The generator takes the following parameters:

-   `MAX_DEPTH` - Maximum depth of the generated DOM.
-   `TARGET_SIZE` - Number of elements in the generated DOM.
-   `CHILD_PROB` - Probability of each element of the sidebar to have children.
-   `MAX_BREADTH` - Maximum number of children of each element of the sidebar.

## Structure of the folder

-   _src_ Code to generate the big static DOM
-   _dist_ - Output folder for the big static DOM generator.
-   utils - Folder for the functions to generate the complex DOM versions of each TodoMVC implementation as well as add additional css.

## Requirements

The only requirement is an installation of Node, to be able to install dependencies and run scripts to serve a local server.

-   Node (min version: 18.13.0)
-   NPM (min version: 8.19.3)

## How to run

`npm install` - Installs the dependencies.

`npm run build` - Generates the static html and corresponding css.

`npm run serve` - Serves the dist folder in port 7002.

## Install using local path

In the project where you want to use the big-dom-generator package, e.g. `Speedometer/resources/todomvc/architecture-examples/react-complex` run:

```bash
npm install ../../big-dom-generator --save-dev
```

The flag `--save-dev` will create an entry in the package.json if one doesn't already exist. Now you can use the package in the project as if it was installed from npm.

## Developer notes

-   The non-matching selectors for the static UI shell were written so that elements within the todoMVC matched more than the right-most selector, for example, the `label` element in the todo items matches the selector `backlog-group li > div > :checked ~ label` up to the `li` simple selector. If any changes are made to the DOM structure within the todoMVC, these selectors will need to be reevaluated and potentially rewritten.
-   Angular uses custom elements, so the todoMVC elements might match a smaller part of a selector as compared to other architectures.
