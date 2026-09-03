# API Compat Reviewer Checklist

Run this checklist on every public header touched by the CL. A header
counts as public if it is under `api/`, has permissive `visibility` in
`BUILD.gn`, or is included directly by downstream projects (e.g.
`pc/session_description.h`).

## 1. Scope

- [ ] Does the CL touch any public header? If no, skip the rest.
- [ ] Are downstream consumers (Chromium, internal Google) likely to
      include the changed declarations? When unsure, treat it as yes.

## 2. Breaking-change detection

For each modified public header, diff at the symbol level and check:

- [ ] **Renamed type / method / enum / enum value**: old + new
      identifier appear together in the diff. Always breaking.
- [ ] **Removed symbol**: public method, type, enum, enum value,
      constant, or member field deleted outright. Always breaking.
- [ ] **Changed function signature**: return type, parameter types,
      parameter order, or arity differs from the old version.
- [ ] **`enum` → `enum class`** (or vice versa): breaks callers that
      relied on implicit-int conversion.
- [ ] **Reordered or inserted enum value**: shifts underlying ints.
      Critical for UMA / persisted enums.
- [ ] **New `= 0;` virtual on an `api/` base class** without a default
      body, which breaks every downstream subclass.
- [ ] **Changed default argument value** on a public method, when
      downstream callers rely on the old default.
- [ ] **Changed include path** of a public header (file moved or
      renamed): downstream `#include` lines break.

## 3. Required mitigations

If any breaking-change box is ticked, the CL must include all of:

- [ ] **Polyfill present**: the old symbol still exists in the header,
      either in this CL or in a paired follow-up commit on the same
      branch.
- [ ] **Polyfill delegates**: the deprecated shim calls into the new
      implementation; it does not duplicate logic.
- [ ] **`[[deprecated("...")]]` message** names the replacement so
      downstream maintainers know what to migrate to.
- [ ] **`TODO: bugs.webrtc.org/NNNNN - description `** above the
      polyfill, tracking removal.
- [ ] **Default body** for new pure-virtual methods (e.g.
      `{ RTC_CHECK_NOTREACHED(); }`) if it is intended to eventually
      become pure virtual.

## 4. Commit message

- [ ] **Bug line** referencing the migration tracking bug
      (`Bug: webrtc:NNNNN`).
- [ ] **Downstream migration block** with explicit before/after
      mapping (old symbol → new symbol, old include → new include).
- [ ] **Reland justification** if this is a reland: spell out what
      changed since the revert (e.g. "added polyfill in the same
      branch").

## 5. Follow-up tracking

- [ ] Tracking bug exists for the eventual removal of the polyfill.
- [ ] Removal is gated on confirmation that all known downstream
      consumers have migrated, not on a calendar date alone.
