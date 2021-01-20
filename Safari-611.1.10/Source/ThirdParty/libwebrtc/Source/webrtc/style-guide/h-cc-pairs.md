# `.h` and `.cc` files come in pairs

This is an overflow page for [this](../style-guide.md#h-cc-pairs)
style rule.

## Example violations

Example violations, which should be avoided in new code:

* Declarations in `path/to/include/foo.h`, definitions in
  `path/to/source/foo.cc`. **Fix:** The `.h` and `.cc` files should be
  in the same directory.
* Declarations in `foo.h`, definitions in both `foo_bar.cc` and
  `foo_baz.cc`. **Fix:** The `.h` and `.cc` files should come in
  pairs, so either split `foo.h` into `foo_bar.h` and `foo_baz.h`, or
  merge `foo_bar.cc` and `foo_baz.cc` into `foo.cc`.

## Exception for platform-specific code

If the functions in a header file need different implementations for
different platforms, we allow the following arrangement:

* Declarations in `foo.h`.
* A complete set of matching definitions in `foo_win.cc`, another
  complete set of matching definitions in `foo_mac.cc`, and so on.
* As per the main rule, these files should all be in the same
  directory and in the same build target. The build target should use
  platform conditionals to ensure that exactly one of the `.cc` files
  are included.
