# API Compat Reviewer: Bad Patterns

Anti-patterns that will break closed-source downstream consumers and
trigger a revert. Each entry names the pattern, why it's bad, and the
fix.

## 1. Rename without polyfill

**Pattern**: A public type, method, or enum value is renamed in a
single CL with no shim left behind for the old name.

**Why**: Downstream code referencing the old name fails to compile at
the next roll. Reviewers cannot see the closed-source consumers, so
"nobody references it" is not verifiable.

**Fix**: Add the new name first; keep the old name as a
`[[deprecated]]` wrapper that delegates to the new implementation.
Remove the old name only after downstream has migrated.

**Reference incident**: the `ExtmapAllowMixed` → `AttributeLevel`
rename in `pc/session_description.h` was reverted within hours of
landing. The reland kept the new names but added a polyfill commit
restoring the old enum and methods as deprecated shims.

## 2. New pure-virtual on an `api/` base class

**Pattern**: `virtual Foo() = 0;` added to an existing abstract class
in `api/`, with no default implementation.

**Why**: Every downstream subclass must now implement `Foo()` or it
won't compile. There is no way to add this incrementally.

**Fix**: Provide a default body (either a no-op, a sensible default
return, or `{ RTC_CHECK_NOTREACHED(); }`) so existing subclasses
keep compiling. Once downstream has overridden the method, you can
make it pure-virtual in a follow-up.

## 3. Reordering or inserting enum values

**Pattern**: Inserting a new value in the middle of an existing enum,
or reordering existing values, in a public enum.

**Why**: Underlying integer values shift. For UMA / histogram /
persisted enums this corrupts data. For ABI consumers, behavior
silently changes at the next roll.

**Fix**: Append new values at the end. Never reorder. For UMA enums,
explicitly assign integers to every value to make the ordering
load-bearing in code, not in declaration order.

## 4. `enum` → `enum class` (or vice versa)

**Pattern**: Tightening a plain `enum` into an `enum class` (or
loosening the other way) in a public header.

**Why**: Callers that relied on implicit-int conversion or unscoped
value names stop compiling.

**Fix**: If the goal is to scope the values, introduce a new
`enum class` with new value names and keep the old plain `enum` as a
deprecated alias. Migrate callers, then remove.

## 5. Reland with no polyfill, no explanation

**Pattern**: Relanding a CL that was reverted for downstream breakage,
without explaining what changed and without adding the missing
polyfill.

**Why**: Same revert, same day, same reason. Wastes everyone's time.

**Fix**: A reland of a downstream-breakage revert must (a) include the
polyfill that was missing the first time, and (b) say so plainly in
the commit message. "Reason for reland: added polyfill in the same
branch" is the minimum.

## 6. Migration message missing from the commit

**Pattern**: A breaking-change CL with a polyfill but no
copy-pasteable downstream migration instructions in the commit
message.

**Why**: Closed-source downstream maintainers can't read the diff;
they read the commit message. Without an explicit
`old_name → new_name` mapping, the migration stalls and the
deprecated shim never gets cleaned up.

**Fix**: Include a "Downstream migration:" block listing every old
symbol and its replacement, plus any moved include paths.

## 7. Removing a deprecated symbol before downstream migrated

**Pattern**: Cleaning up a `[[deprecated]]` shim because "it's been
deprecated for a while now."

**Why**: Without confirmation that all closed-source consumers have
migrated, removal is a guess. The revert will follow.

**Fix**: Gate removal on tracking-bug confirmation that downstream is
clean, not on time elapsed.
