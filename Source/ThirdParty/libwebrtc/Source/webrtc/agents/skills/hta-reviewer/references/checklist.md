# HTA Reviewer Checklist

Use this checklist to identify specific issues based on the contributor's
affiliation.

## 1. Process & Formalities (Mandatory for Externals)

- [ ] **CLA**: Has the contributor signed the CLA?
- [ ] **AUTHORS**: Is the contributor (or their organization) listed in the
  \`AUTHORS\` file?
- [ ] **Bug Line**: Does the commit message have a \`Bug:\` line? (e.g.,
  \`webrtc:XXXX\` or \`chromium:XXXX\`).
- [ ] **Freshness**: Is the CL rebased against the tip-of-tree?

## 2. Standards & Specs (All Contributors)

- [ ] **W3C/IETF Compliance**: Does the logic match the relevant specification
  (e.g., WebRTC-PC, SCTP, RTP)? Flag implementations that use arbitrary error
  codes or behaviors that deviate from the spec.
- [ ] **C++ Style Guide**: Ensure strict adherence to the Google C++ Style Guide
  (e.g., \`nullptr\` vs \`NULL\`).

## 3. Technical Review (All Contributors)

### Thread Safety & Concurrency

- [ ] Are member variables guarded? Use \`RTC_GUARDED_BY(checker\_)\`.
- [ ] Are methods asserting their thread context? Use
  \`RTC_DCHECK_RUN_ON(&checker\_)\`.
- [ ] Is \`SequenceChecker\` initialized in the constructor?
- [ ] Avoid \`BlockingCall\` unless absolutely necessary (and justified).

### Modern C++ Standards

- [ ] **No \`rtc::\` namespace**: The \`rtc\` namespace is obsolete. Types
  should be in the \`webrtc\` namespace.
- [ ] **Pointers**: Use \`nullptr\`, never \`NULL\`.
- [ ] **Smart Pointers**: Prefer \`std::unique_ptr\` and \`absl::WrapUnique\`
  for local socket/resource management to ensure cleanup on all return paths.
- [ ] **String Views**: Prefer \`absl::string_view\` over \`std::string_view\`.
- [ ] **Span**: Use \`std::span\` for array views (migrated from \`ArrayView\`).
- [ ] **Time**: Use \`webrtc::Timestamp\` or \`webrtc::TimeDelta\` instead of
  raw \`int\` or \`int64_t\` for time values.
- [ ] **Logging**: Use \`AbslStringify\` in \`RTC_LOG\` statements. Avoid
  explicit \`.ToString()\` or \`TypeToString()\` calls.

### API & Architectural Design

- [ ] **New Virtual Methods (api/ only)**: If adding a virtual method to a base
  class in the \`api/\` directory, provide a default implementation (e.g., \`{
  RTC_DCHECK_NOTREACHED(); }\`) to avoid breaking downstream implementations.

- [ ] **Visibility**: Is the \`visibility\` in \`BUILD.gn\` as restrictive as
  possible?

- [ ] **Field Trials**: Ensure field trials are passed from the \`Environment\`.
  Are new trials registered in \`g3doc/field-trials.md\`?

### Testing

- [ ] **Assertions**: Use \`ASSERT_EQ\` (or similar) instead of \`EXPECT_EQ\` if
  the condition is a prerequisite for the test's validity.
- [ ] **Boilerplate**: Avoid excessive setup code in integration tests; leverage
  existing fixtures or helper classes.

## 4. Code Hygiene (All Contributors)

- [ ] **No Commented-out Code**: If it's not needed, delete it.
- [ ] **No Trailing Spaces**: Clean up whitespace.
- [ ] **TO​DO markers**: Follow the format \`// TO​DO: webrtc:XXXX -
  Description\`.
- [ ] **Naming**: Avoid overly shortened names (e.g., use \`InvalidParameters\`
  instead of \`InvParam\`).
