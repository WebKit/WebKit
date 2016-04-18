# Tests for the MathML in HTML5 implementation note

This repository contains tests for the
[MathML in HTML5 implementation note](http://www.mathml-association.org/MathMLinHTML5/).
We follow the formats and conventions of
[W3C's Test the Web Forward Project](http://testthewebforward.org/),
please read their documentation for details. The main goals are:
- helping implementers to verify conformance with the technical description
  given in the MathML in HTML5 note.
- provide a set of automatable tests to integrate into the testing framework of
  web browser developers.

## Installation

You must at least install [git](http://git-scm.com/) and
[python](http://python.org/). Clone the repository with

    $ git clone --recursive https://github.com/username/MathMLinHTML5-tests.git

If you cloned the repository without --recursive, you will likely have empty
`resources` and `tools` directories at the root of your cloned repo. You can
clone the submodules with these additional steps:

    $ cd MathMLinHTML5-tests
    $ git submodule update --init --recursive

Next, generate the MANIFEST.json file with the following command:

    $ python tools/manifest/update.py

To verify the tests in your browser, you also need to setup a web server whose
root points to the root of the cloned repo. For a quick setup, you can just run
the `serve.py` Python script at the root:

    $ python server.py

Finally, open `index.html` at the web server root to get an overview of all the
tests. Using W3C's wptserve has not been tried but the tests can easily be
imported into other automated testing framework anyway.

## Note on Web fonts

Many of the tests verify OpenType features and require specific Web fonts for
that purpose. WOFF fonts are generated using the Python API of
[fontforge](https://github.com/fontforge/fontforge/). A recent enough version
of FontForge is necessary so that it includes fixes for [WOFF checkSumAdjustment](https://github.com/fontforge/fontforge/issues/926), [USE_TYPO_METRICS flag](https://github.com/fontforge/fontforge/pull/2274) and various bugs in OpenType
MATH.
