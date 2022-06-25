# ANGLE - Almost Native Graphics Layer Engine

The goal of ANGLE is to allow users of multiple operating systems to seamlessly run WebGL and other
OpenGL ES content by translating OpenGL ES API calls to one of the hardware-supported APIs available
for that platform. ANGLE currently provides translation from OpenGL ES 2.0, 3.0 and 3.1 to Vulkan,
desktop OpenGL, OpenGL ES, Direct3D 9, and Direct3D 11. Future plans include ES 3.2, translation to
Metal and MacOS, Chrome OS, and Fuchsia support.

### Level of OpenGL ES support via backing renderers

|                |  Direct3D 9   |  Direct3D 11     |   Desktop GL   |    GL ES      |    Vulkan     |    Metal      |
|----------------|:-------------:|:----------------:|:--------------:|:-------------:|:-------------:|:-------------:|
| OpenGL ES 2.0  |    complete   |    complete      |    complete    |    complete   |    complete   |    complete   |
| OpenGL ES 3.0  |               |    complete      |    complete    |    complete   |    complete   |  in progress  |
| OpenGL ES 3.1  |               | [incomplete](doc/ES31StatusOnD3D11.md) |    complete    |    complete   |    complete   |               |
| OpenGL ES 3.2  |               |                  |  in progress   |  in progress  |  in progress  |               |

### Platform support via backing renderers

|              |    Direct3D 9  |   Direct3D 11  |   Desktop GL  |    GL ES    |   Vulkan    |    Metal    |
|-------------:|:--------------:|:--------------:|:-------------:|:-----------:|:-----------:|:-----------:|
| Windows      |    complete    |    complete    |   complete    |   complete  |   complete  |             |
| Linux        |                |                |   complete    |             |   complete  |             |
| Mac OS X     |                |                |   complete    |             |             | in progress |
| iOS          |                |                |               |             |             | in progress |
| Chrome OS    |                |                |               |   complete  |   planned   |             |
| Android      |                |                |               |   complete  |   complete  |             |
| GGP (Stadia) |                |                |               |             |   complete  |             |
| Fuchsia      |                |                |               |             |   complete  |             |

ANGLE v1.0.772 was certified compliant by passing the OpenGL ES 2.0.3 conformance tests in October 2011.

ANGLE has received the following certifications with the Vulkan backend:
* OpenGL ES 2.0: ANGLE 2.1.0.d46e2fb1e341 (Nov, 2019)
* OpenGL ES 3.0: ANGLE 2.1.0.f18ff947360d (Feb, 2020)
* OpenGL ES 3.1: ANGLE 2.1.0.f5dace0f1e57 (Jul, 2020)

ANGLE also provides an implementation of the EGL 1.5 specification.

ANGLE is used as the default WebGL backend for both Google Chrome and Mozilla Firefox on Windows
platforms. Chrome uses ANGLE for all graphics rendering on Windows, including the accelerated
Canvas2D implementation and the Native Client sandbox environment.

Portions of the ANGLE shader compiler are used as a shader validator and translator by WebGL
implementations across multiple platforms. It is used on Mac OS X, Linux, and in mobile variants of
the browsers. Having one shader validator helps to ensure that a consistent set of GLSL ES shaders
are accepted across browsers and platforms. The shader translator can be used to translate shaders
to other shading languages, and to optionally apply shader modifications to work around bugs or
quirks in the native graphics drivers. The translator targets Desktop GLSL, Vulkan GLSL, Direct3D
HLSL, and even ESSL for native GLES2 platforms.

## Sources

ANGLE repository is hosted by Chromium project and can be
[browsed online](https://chromium.googlesource.com/angle/angle) or cloned with

    git clone https://chromium.googlesource.com/angle/angle


## Building

View the [Dev setup instructions](doc/DevSetup.md).

## Contributing

* Join our [Google group](https://groups.google.com/group/angleproject) to keep up to date.
* Join us on [Slack](https://chromium.slack.com) in the #angle channel. You can
  follow the instructions on the [Chromium developer page](https://www.chromium.org/developers/slack)
  for the steps to join the Slack channel. For Googlers, please follow the
  instructions on this [document](https://docs.google.com/document/d/1wWmRm-heDDBIkNJnureDiRO7kqcRouY2lSXlO6N2z6M/edit?usp=sharing)
  to use your google or chromium email to join the Slack channel.
* [File bugs](http://anglebug.com/new) in the [issue tracker](https://bugs.chromium.org/p/angleproject/issues/list) (preferably with an isolated test-case).
* [Choose an ANGLE branch](doc/ChoosingANGLEBranch.md) to track in your own project.


* Read ANGLE development [documentation](doc).
* Look at [pending](https://chromium-review.googlesource.com/q/project:angle/angle+status:open)
  and [merged](https://chromium-review.googlesource.com/q/project:angle/angle+status:merged) changes.
* Become a [code contributor](doc/ContributingCode.md).
* Use ANGLE's [coding standard](doc/CodingStandard.md).
* Learn how to [build ANGLE for Chromium development](doc/BuildingAngleForChromiumDevelopment.md).
* Get help on [debugging ANGLE](doc/DebuggingTips.md).
* Go through [ANGLE's orientation](doc/Orientation.md) and sift through [starter projects](https://bugs.chromium.org/p/angleproject/issues/list?q=Hotlist%3DStarterBug). If you decide to take on any task, write a comment so you can get in touch with us, and more importantly, set yourself as the "owner" of the bug. This avoids having multiple people accidentally working on the same issue.


* Read about WebGL on the [Khronos WebGL Wiki](http://khronos.org/webgl/wiki/Main_Page).
* Learn about the initial ANGLE implementation details in the [OpenGL Insights chapter on ANGLE](http://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-ANGLE.pdf) (this is not the most up-to-date ANGLE implementation details, it is listed here for historical reference only) and this [ANGLE presentation](https://drive.google.com/file/d/0Bw29oYeC09QbbHoxNE5EUFh0RGs/view?usp=sharing&resourcekey=0-CNvGnQGgFSvbXgX--Y_Iyg).
* Learn about the past, present, and future of the ANGLE implementation in [this presentation](https://docs.google.com/presentation/d/1CucIsdGVDmdTWRUbg68IxLE5jXwCb2y1E9YVhQo0thg/pub?start=false&loop=false).
* Watch a [short presentation](https://youtu.be/QrIKdjmpmaA) on the Vulkan back-end.
* Track the [dEQP test conformance](doc/dEQP-Charts.md)
* Read design docs on the [Vulkan back-end](src/libANGLE/renderer/vulkan/README.md)
* Read about ANGLE's [testing infrastructure](infra/README.md)
* View information on ANGLE's [supported extensions](doc/ExtensionSupport.md)
* If you use ANGLE in your own project, we'd love to hear about it!
