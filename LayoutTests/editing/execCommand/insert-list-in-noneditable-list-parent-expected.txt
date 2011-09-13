execCommand("InsertUnorderedList") for contenteditable root element was crashing. The test has passed if it does not crash.
PASS
| "
    "
| <dl>
|   <div>
|     contenteditable="true"
|     id="div1"
|     <ul>
|       <li>
|         <br>
| "
    "
| <ul>
|   <div>
|     contenteditable="true"
|     id="div2"
|     <ul>
|       <li>
|         <br>
| "
    "
| <ol>
|   <div>
|     contenteditable="true"
|     id="div3"
|     <ul>
|       <li>
|         <#selection-caret>
|         <br>
| "
"
