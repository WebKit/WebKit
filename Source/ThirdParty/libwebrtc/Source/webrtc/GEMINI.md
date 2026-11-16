# General workflow

@agents/prompts/common.md @agents/README.md

**Important:** `depot_tools` must be in your `PATH` for working in this checkout
(e.g., for `git new-branch` to work).

# Searching Downstream

When auditing symbol usage or finding references in downstream projects, always
prefer high-precision CodeSearch filters over raw text searches:

-   **Semantic References:** Use `usage:webrtc::SymbolName` to find actual
    call-sites.
-   **Virtual Overrides:** Use `func:MethodName` to find overrides in downstream
    implementations.
-   **Exclude WebRTC Mirrors:** Always append `-file:stable/webrtc` to focus on
    true downstream consumers.

# Codebase knowledge

@./g3doc/abseil-in-webrtc.md

@./g3doc/how_to_write_documentation.md

@./g3doc/implementation_basics.md

@./g3doc/style-guide.md

@./g3doc/testing.md
