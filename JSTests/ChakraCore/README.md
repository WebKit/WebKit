# ChakraCore

[![Join the chat at https://gitter.im/Microsoft/ChakraCore](https://badges.gitter.im/Microsoft/ChakraCore.svg)](https://gitter.im/Microsoft/ChakraCore?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

ChakraCore is the core part of Chakra, the high-performance JavaScript engine that powers Microsoft Edge and Windows applications written in HTML/CSS/JS.  ChakraCore supports Just-in-time (JIT) compilation of JavaScript for x86/x64/ARM, garbage collection, and a wide range of the latest JavaScript features.  ChakraCore also supports the [JavaScript Runtime (JSRT) APIs](https://github.com/Microsoft/ChakraCore/wiki/JavaScript-Runtime-%28JSRT%29-Overview), which allows you to easily embed ChakraCore in your applications.

You can stay up-to-date on progress by following the [MSEdge developer blog](https://blogs.windows.com/msedgedev/).

## [Build Status](https://github.com/Microsoft/ChakraCore/wiki/Build-Status)

|                               | __Debug__ | __Test__ | __Release__ |
|:-----------------------------:|:---------:|:--------:|:-----------:|
| __Windows (x64)__             | [![x64debug][x64dbgicon]][x64dbglink] | [![x64test][x64testicon]][x64testlink] | [![x64release][x64relicon]][x64rellink] |
| __Windows (x86)__             | [![x86debug][x86dbgicon]][x86dbglink] | [![x86test][x86testicon]][x86testlink] | [![x86release][x86relicon]][x86rellink] |
| __Windows (ARM)__             | [![armdebug][armdbgicon]][armdbglink] | [![armtest][armtesticon]][armtestlink] | [![armrelease][armrelicon]][armrellink] |
| __Ubuntu 16.04 (x64)__        | [![linuxdebug][linuxdbgicon]][linuxdbglink] | [![linuxtest][linuxtesticon]][linuxtestlink] | [![linuxrelease][linuxrelicon]][linuxrellink] |
| __Ubuntu 16.04 (x64 static)__ | [![linuxsdebug][linuxsdbgicon]][linuxsdbglink] | [![linuxstest][linuxstesticon]][linuxstestlink] | [![linuxsrelease][linuxsrelicon]][linuxsrellink] |
| __OS X 10.9 (x64 static)__    | [![osxsdebug][osxsdbgicon]][osxsdbglink] | [![osxstest][osxstesticon]][osxstestlink] | [![osxsrelease][osxsrelicon]][osxsrellink] |

*If you see badges reading "Build: Unknown" it is likely because a build was skipped due to changes being only in files known not to affect the health of the build.*

[x64dbgicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x64_debug/badge/icon
[x64dbglink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x64_debug/
[x64testicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x64_test/badge/icon
[x64testlink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x64_test/
[x64relicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x64_release/badge/icon
[x64rellink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x64_release/

[x86dbgicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x86_debug/badge/icon
[x86dbglink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x86_debug/
[x86testicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x86_test/badge/icon
[x86testlink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x86_test/
[x86relicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x86_release/badge/icon
[x86rellink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/x86_release/

[armdbgicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/arm_debug/badge/icon
[armdbglink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/arm_debug/
[armtesticon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/arm_test/badge/icon
[armtestlink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/arm_test/
[armrelicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/arm_release/badge/icon
[armrellink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/arm_release/

[linuxdbgicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_debug/badge/icon
[linuxdbglink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_debug/
[linuxtesticon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_test/badge/icon
[linuxtestlink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_test/
[linuxrelicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_release/badge/icon
[linuxrellink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_release/

[linuxsdbgicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_debug_static/badge/icon
[linuxsdbglink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_debug_static/
[linuxstesticon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_test_static/badge/icon
[linuxstestlink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_test_static/
[linuxsrelicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_release_static/badge/icon
[linuxsrellink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/ubuntu_linux_release_static/

[osxsdbgicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/osx_osx_debug_static/badge/icon
[osxsdbglink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/osx_osx_debug_static/
[osxstesticon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/osx_osx_test_static/badge/icon
[osxstestlink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/osx_osx_test_static/
[osxsrelicon]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/osx_osx_release_static/badge/icon
[osxsrellink]: http://dotnet-ci.cloudapp.net/job/Microsoft_ChakraCore/job/master/job/osx_osx_release_static/

Above is a table of our rolling build status. We run additional builds on a daily basis. See [Build Status](https://github.com/Microsoft/ChakraCore/wiki/Build-Status) for the status of all builds.

## Security

If you believe you have found a security issue in ChakraCore, please share it with us privately following the guidance at the Microsoft [Security TechCenter](https://technet.microsoft.com/en-us/security/ff852094). Reporting it via this channel helps minimize risk to projects built with ChakraCore.

## Documentation

* [ChakraCore Architecture](https://github.com/Microsoft/ChakraCore/wiki/Architecture-Overview)
* [Quickstart Embedding ChakraCore](https://github.com/Microsoft/ChakraCore/wiki/Embedding-ChakraCore)
* [JSRT Reference](https://github.com/Microsoft/ChakraCore/wiki/JavaScript-Runtime-%28JSRT%29-Reference)
* [Contribution guidelines](CONTRIBUTING.md)
* [Blogs, talks and other resources](https://github.com/Microsoft/ChakraCore/wiki/Resources)

## Building ChakraCore

You can build ChakraCore on Windows 7 SP1 or above, and Windows Server 2008 R2 or above, with either Visual Studio 2013 or 2015 with C++ support installed.  Once you have Visual Studio installed:

* Clone ChakraCore through ```git clone https://github.com/Microsoft/ChakraCore.git```
* Open `Build\Chakra.Core.sln` in Visual Studio
* Build Solution

More details in [Building ChakraCore](https://github.com/Microsoft/ChakraCore/wiki/Building-ChakraCore).

Alternatively, you can get pre-built ChakraCore binaries from our [NuGet Packages](https://github.com/Microsoft/ChakraCore/wiki/NuGet-Packages).

## Using ChakraCore

Once built, you have a few options for how you can use ChakraCore:

* The most basic is to test the engine is running correctly with the *ch.exe* binary.  This app is a lightweight hosting of JSRT that you can use to run small applications.  After building, you can find this binary in:
  * `Build\VcBuild\bin\${platform}_${configuration}`
  * (e.g. `Build\VcBuild\bin\x64_debug`)
* You can [embed ChakraCore](https://github.com/Microsoft/ChakraCore/wiki/Embedding-ChakraCore) in your applications - see [documentation](https://github.com/Microsoft/ChakraCore/wiki/Embedding-ChakraCore) and [samples](http://aka.ms/chakracoresamples).
* Finally, you can also use ChakraCore as the JavaScript engine in Node.  You can learn more by reading how to use [Chakra as Node's JS engine](https://github.com/Microsoft/node)

_A note about using ChakraCore_: ChakraCore is the foundational JavaScript engine, but it does not include the external APIs that make up the modern JavaScript development experience.  For example, DOM APIs like ```document.write()``` are additional APIs that are not available by default and would need to be provided.  For debugging, you may instead want to use ```print()```.

## Contribute

Contributions to ChakraCore are welcome.  Here is how you can contribute to ChakraCore:

* [Submit bugs](https://github.com/Microsoft/ChakraCore/issues) and help us verify fixes
* [Submit pull requests](https://github.com/Microsoft/ChakraCore/pulls) for bug fixes and features and discuss existing proposals
* Chat about [@ChakraCore](https://twitter.com/ChakraCore) on Twitter

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

Please refer to [Contribution guidelines](CONTRIBUTING.md) for more details.

## License

Code licensed under the [MIT License](https://github.com/Microsoft/ChakraCore/blob/master/LICENSE.txt).

## Roadmap
For details on our planned features and future direction please refer to our [roadmap](https://github.com/Microsoft/ChakraCore/wiki/Roadmap).

## Contact us
For questions about ChakraCore, you can reach us on [Gitter](https://gitter.im/Microsoft/ChakraCore) or open an [issue](https://github.com/Microsoft/ChakraCore/issues/new) and prefix the issue title with [Question]. See [Question](https://github.com/Microsoft/ChakraCore/issues?q=label%3AQuestion) tag for already-opened questions.
