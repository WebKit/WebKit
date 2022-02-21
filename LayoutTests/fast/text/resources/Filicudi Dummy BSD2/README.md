# Filicudi Color Dummy

This project is a subset of [Filicudi Color](https://fonts.adobe.com/fonts/tipoteca-series).

The principle of Filicudi Color is applying an even color pattern across a line of text, no matter how wide the individual letters may be.
To achieve this, contextual alternates are used. In the most simple case, two colors alternate with each other.

This subset includes the glyphs .notdef, space, H (each in two color alternates), to illustrate a particular issue encountered in Safari:

- contextual alternates work as expected when no CSS is invoked
- contextual alternate assignment breaks with the space glyph when CSS is used via `font-feature-settings: "calt"`


This project was prepared by Frank Grießhammer (fgriessh@adobe.com) and is provided under a BSD-2-Clause license.