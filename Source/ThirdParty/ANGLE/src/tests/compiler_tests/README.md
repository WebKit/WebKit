# Translator Unit Tests

This directory contains historical unit tests for the ANGLE translator.  Barring a few cases, they
should be considered deprecated.  **Do not add new tests to this directory.**

The existing translator tests are severely limited in what they test.  They often verify translation
by asserting that a substring exists in the translated source or a node is found in the AST.
However, neither is sufficient to ensure the transformations are done correctly.  Often issues are
discovered only when the shaders are used in a draw call and the translated source is given to the
native driver.

Furthermore, with the built-in assumptions about the way the AST works, these tests are incompatible
with the upcoming IR.

Instead, add end-to-end tests:

* They can be easily tested on every backend
* Functional testing with a draw call ensures the transformations are correct
* Using the `GL_ANGLE_translated_shader_source` extension, we can still verify the output is as
  expected.  This is a maintenance burden however and best avoided unless there is no _functional_
  way to verify the transformation is done correctly, e.g. if the transformation works around
  undefined behavior which can't be reliably tested.

To test translator validation, add new tests to `GLSLValidationTest.cpp`, which not only verifies
that compilation fails, but also has facilities to ensure the failure reason is as expected (and
not, for example, a typo in the test)

To test transformations, add new tests to `GLSLTest.cpp` that would fail if the transformation in
question is not done.  By using ANGLE features to control the translator flag in question, the
transformation can be force-enabled in a test suite, e.g. by instantiating with
`ES3_OPENGL().enable(Feature::ScalarizeVecAndMatConstructorArgs)`

To test that the output includes a specific text, if there is no better form of verification, add
new tests to `GLSLOutputTest.cpp`.
