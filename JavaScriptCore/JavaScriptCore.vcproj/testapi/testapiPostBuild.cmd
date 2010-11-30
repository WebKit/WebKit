if exist "$(WebKitOutputDir)\buildfailed" del "$(WebKitOutputDir)\buildfailed"

xcopy /y /d "$(ProjectDir)\..\..\API\tests\testapi.js" "$(OutDir)"
