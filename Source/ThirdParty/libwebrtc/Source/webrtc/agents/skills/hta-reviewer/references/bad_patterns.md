# HTA Reviewer: Bad Patterns & Anti-patterns

This document captures "Bad Ideas" and anti-patterns that should be flagged
during review.

## 1. "Test Mode" in Production

**Pattern**: Adding a boolean flag like `inside_test_` or `test_mode_` to a
production class to alter its behavior for unittests. **HTA's Take**: "I'm not
happy about adding flags that put the code into 'test mode'. It's a bad pattern

- as far as possible, we should be testing the actual code, and adaptations
  should be in the test fixture." **Fix**: Use dependency injection (e.g.,
  inject a factory or a mock handler) or specialize the test fixture.

## 2. Command-line Flags in Unittests

**Pattern**: Using `ABSL_FLAG` to control unittest behavior or to trigger
specific test cases. **HTA's Take**: "I *still* don't like the use of ABSL_FLAG
in unittest binaries... testing should be automatic and done in CQ." **Fix**: If
it's a performance test, move it to a dedicated benchmark binary. If it's an
integration test, use `INSTANTIATE_TEST_SUITE_P` to cover different
configurations automatically.

## 3. Mixing Input and Output Concepts

**Pattern**: A configuration struct or class where some members are inputs
(provided by the user) and others are internal outputs (calculated during
setup), but they are all mixed together. **HTA's Take**: "Here you are not
separating the concepts that are input to the configuration setup from the
concepts that are its output. Thus, this becomes unreadable." **Fix**: Separate
input configuration from the resulting state. Make the state class immutable
(`const`) after construction.

## 4. Opaque Logic / Magic Numbers

**Pattern**: Using hex values or bitwise operations without descriptive
constants or comments. **HTA's Take**: "what's 0x10 here? ... it's more obvious
to use !((udpOpts & PacketSocketFactory::OPT_DTLS) && (udpOopts &
PacketSocketFactory::OPT_DTLS_INSECURE)) instead." **Fix**: Replace magic
numbers with named constants or clear boolean expressions.

## 5. Weak Justification for Relands

**Pattern**: Relanding a reverted CL without clearly explaining why the previous
issue is resolved. **HTA's Take**: "Relands should always tell why it's now safe
to land them. ... When relanding, always make sure to say in the description why
you think it will pass this time." **Fix**: Add a detailed explanation in the
commit message regarding the fix for the original failure (e.g., "Reason for
reland: Moved problematic functions to .cc file").

## 6. "Impure" Default Flips

**Pattern**: Mixing logic changes or test adaptations with the flipping of a
default flag in the same CL. **HTA's Take**: "I'd be happier if those changes
were landed first, and the default-flip were in a pure CL that did only that."
**Fix**: Split the CL. Land the supporting changes (tests, internal fixes)
first, then flip the default in a dedicated, minimal CL.
