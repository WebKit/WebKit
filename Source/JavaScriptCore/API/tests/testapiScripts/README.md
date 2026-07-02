**IMPORTANT**: when a script file required by tests is added to this directory or its subdirectory,
it needs to be manually added to the corresponding `Copy Files` build phase in the `testapi` target
in `Source/JavaScriptCore/JavaScriptCore.xcodeproj`.

1. Open `Source/JavaScriptCore/JavaScriptCore.xcodeproj` in Xcode.
2. Select `JavaScriptCore` (top project item) in the Project Navigator.
3. Select the `testapi` target.
4. Select the `Build Phases` tab.
5. Expand the `Copy Files` build phase for `testapiScripts` or `testapiScripts/dependencyListTests`
   and add the new file(s) to the list.