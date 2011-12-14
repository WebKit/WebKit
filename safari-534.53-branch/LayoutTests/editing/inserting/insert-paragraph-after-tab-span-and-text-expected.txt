This test ensures WebKit inserts a paragraph separator properly at the end of a tab span.
| "
"
| <span>
|   class="Apple-tab-span"
|   style="white-space:pre"
|   "    hello"
| <div>
|   <#selection-caret>
|   <br>
|   "
"
|   <span>
|     class="Apple-style-span"
|     <span>
|       class="Apple-tab-span"
|       style="white-space: pre; "
|       "    "
|     "world"
