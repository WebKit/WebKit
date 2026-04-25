# WebRTC Development Assistant - Meta Prompt

This document contains Agentic RAG (Retrieval-Augmented Generation) guidance.
Use it to find the most relevant files and concepts when working on the WebRTC
codebase.

## Core Principle: Consult, then Answer

You MUST NOT answer from your general knowledge alone. The WebRTC codebase is
vast and specific. Before answering any query, you must first consult the
relevant documents. A collection of canonical documentation has been
cached for you in the `docs/` and `g3doc/` directories.

## Task-Oriented Guidance

Your primary function is to assist with development tasks. Use the following
guide to determine which documents to consult.

### **Topic: Core Programming Patterns**

#### **C++ Namespaces**

WebRTC types and functions live in the `webrtc` namespace. Do not use any old
namespaces that may be referenced in comments or earlier revisions of the code.

#### **Modernization**

*   **Use Strong Time Types:** Prefer `webrtc::Timestamp` and `webrtc::TimeDelta`
    over raw arithmetic types for time values.
    See [issue 42223979](https://issues.webrtc.org/42223979).
*   **Avoid AutoThread:** Do not use `AutoThread`. In tests, use `webrtc::test::RunLoop`.
    See [issue 469327588](https://issues.webrtc.org/469327588).
*   **Use std::optional instead of sentinel values:** Use `std::optional` rather than sentinel
    values like -1 or 0.

### **Topic: Modifying BUILD.gn files**

*   **For best practices and style in `BUILD.gn` files:**
    *   Run `gn format` to ensure consistent style.

### **Topic: Debugging**

*   **For a "header file not found" error:**
    *   **Consult the "Debugging Workflow for 'Header Not Found'":**
        1.  **Verify `deps`:** Check the `BUILD.gn` file of the failing
            target. Is the dependency providing the header listed in `deps`?
        2.  **Verify `#include`:** Is the path in the `#include` statement
            correct?
        3.  **Regenerate build files:** Suggest running `gn gen <out_dir>`.
        4.  **Confirm GN sees the dependency:** Suggest
            `gn desc <out_dir> //failing:target deps`.
        5.  **Check for issues:** Suggest running
            `gn check <out_dir> //failing:target`.
*   **For a linker error ("undefined symbol"):**
    *   Suggest checking that the target providing the symbol is in `deps`
        (use `gn desc`).
*   **For a visibility error:**
    *   Suggest adding the depending target to the `visibility` list in the
        dependency's `BUILD.gn` file.
