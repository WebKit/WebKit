---
name: api-compat-reviewer
description: Automated code review for backward-incompatible changes in WebRTC public headers. Use this skill to catch renames/removals/signature changes in api/ (and other downstream-consumed headers like pc/session_description.h) that will break Chromium and internal builds, and to verify a deprecation polyfill is in place.
---

# API Compat Reviewer Skill

libWebRTC is consumed by downstream projects (Chromium, and internal,
closed-source Google code) that build out-of-tree against a moving
WebRTC checkout. Any rename, removal, or signature change to a public
symbol breaks the next downstream import and triggers a revert. The
upstream reviewer cannot see the closed-source consumers, so the burden
is on the CL author to keep the old API in place until those consumers
have migrated.

The required workflow is always three steps:

1. **Land the new API alongside the old one.** The old symbol stays as
   a thin wrapper / translation layer that delegates to the new
   implementation, marked `[[deprecated]]`.
2. **Migrate downstream** to the new API, following the instructions
   the CL author put in the commit message (see "Commit message
   guidance" below).
3. **Remove the old API** in a follow-up CL once all known consumers
   have moved.

This skill catches CLs that skip step 1 and verifies the migration
path is in place.

The canonical incident: a CL renamed
`MediaContentDescription::ExtmapAllowMixed` → `AttributeLevel` (and the
matching getters/setters) in `pc/session_description.h`. It was reverted
within hours with the message:

> "Breaks downstream projects. The definitions in session_description.h
> must be kept in parallel until downstream projects are updated."

The reland kept the new names but added a "polyfill" commit that
restored the old enum and methods as `[[deprecated]]` shims delegating
to the new ones.

## What counts as a "public" header / symbol

- **Always public**: every `.h` file under `api/`.
- **Effectively public**: headers outside `api/` that downstream
  projects include directly. `pc/session_description.h` is the proven
  example. When unsure, check `BUILD.gn` for permissive `visibility`
  (e.g. `[ "*" ]`) or grep Chromium for `#include` of the header.
- **Internal**: headers under `pc/`, `media/`, `modules/`, etc. with
  restricted `visibility` and no known downstream includes.

If the diff only touches internal headers, this skill has nothing to
do. Say so and stop.

## Breaking-change patterns to flag

For each modified public header, look for:

1. **Renamed symbols**: a removed identifier + an added identifier with
   a similar name in the same class/namespace. Common shapes:
   - `enum X` → `enum class Y`, value renames (`kNo` → `kNone`).
   - `set_foo_enum()` → `set_foo_level()`.
   - Type rename in a parameter or return type.
2. **Removed symbols**: public method, free function, type, enum,
   enum value, constant, or member field deleted outright.
3. **Changed signatures**: return type changed, parameter type changed,
   parameter added without a default, parameter order changed.
4. **New pure virtual methods**: `virtual ... = 0;` added to an
   abstract base class in `api/` without a default implementation. This
   breaks every downstream subclass.
5. **Tighter enums**: switching an `enum` to `enum class` is a breaking
   change for any caller that relied on implicit-int conversion.
6. **Changed enum value numbers**: reordering or inserting values
   shifts the underlying integers. Particularly bad for UMA/histogram
   enums where values are persisted.
7. **Default value changes** on public methods or struct members, when
   downstream relies on the old default.

## Required mitigations

When a public header has any of the above, the CL must include a
polyfill so downstream keeps compiling. The pattern is:

```cpp
// New API
enum class AttributeLevel { kNone, kSession, kMedia };
void set_extmap_allow_mixed_level(AttributeLevel level);
AttributeLevel extmap_allow_mixed_level() const;

// TODO(bugs.webrtc.org/NNNNN): Remove once downstream has migrated.
enum [[deprecated("Use AttributeLevel")]] ExtmapAllowMixed {
  kNo, kSession, kMedia
};
[[deprecated("Use set_extmap_allow_mixed_level")]]
void set_extmap_allow_mixed_enum(ExtmapAllowMixed v) { /* delegate */ }
[[deprecated("Use extmap_allow_mixed_level")]]
ExtmapAllowMixed extmap_allow_mixed_enum() const { /* delegate */ }
```

Verify that:

- Both old and new names compile and link.
- The deprecated shim **delegates** to the new implementation; it does
  not duplicate logic.
- Each shim has a `[[deprecated("...")]]` message naming the
  replacement.
- A `TODO: bugs.webrtc.org/NNNNN - description` references the
  tracking bug for removal.
- For pure-virtual additions, a default implementation is present
  (e.g. `{ RTC_CHECK_NOTREACHED(); }` or a sensible no-op).

## Commit message guidance

The CL that introduces the new API must give downstream maintainers
explicit, copy-pasteable migration instructions. Suggest a message
shaped like:

```
api: introduce <NewName> alongside <OldName>

Renames <OldName> to <NewName>. The old symbol is preserved as a
[[deprecated]] wrapper that delegates to the new one, so this CL is
safe to roll into downstream projects without changes.

Downstream migration:
  - Replace <OldEnum>::<kOldValue> with <NewEnum>::<kNewValue>
  - Replace <old_method>() with <new_method>()
  - Replace #include "<old/path.h>" with #include "<new/path.h>"

After downstream has migrated, the deprecated symbols will be removed
in a follow-up CL tracked by bugs.webrtc.org/NNNNN.

Bug: webrtc:NNNNN
```

If the CL has no such migration block, flag it: closed-source
downstream maintainers cannot migrate from a diff they cannot read.

## Workflow

1. **Filter the diff** to public headers only. If none, stop.
2. **Diff each header** at the symbol level (use `git diff` on the
   header in the CL range). For each removed/renamed/signature-changed
   symbol, classify it against the patterns above.
3. **Check for polyfill** in the same CL or in a follow-up commit on
   the same branch. If absent, this is the headline finding.
4. **Check the commit message** mentions the migration plan and the
   tracking bug.
5. **Cross-reference** [checklist.md](references/checklist.md) for
   items, and [bad_patterns.md](references/bad_patterns.md) for
   common anti-patterns.
6. **Report** findings grouped by header. For each breaking change,
   state: the symbol, the kind of break, the required polyfill, and
   the bug to file/reference.

## Tone and Style

- **Direct**: "This rename will break downstream. Add a polyfill."
- **Concrete**: name the symbol, point at the line, show the shim.
- **Strict on hygiene**: no polyfill = revert risk. Say so plainly.
- **Quiet on the all-clear**: if nothing public moved, one line is
  enough.
