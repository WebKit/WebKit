#  ANGLE EGL Headers

The EGL headers ANGLE uses are generated using the Khronos tools but modified to include function pointer types and function prototype guards.

### Regenerating EGL.h

1. Install **Python 3** (not 2) with the **lxml** addon. You can do this using `pip install lxml` from your Python's Scripts folder.
1. Clone [https://github.com/KhronosGroup/EGL-Registry.git](https://github.com/KhronosGroup/EGL-Registry.git).
1. Edit `EGL-Registry/api/genheaders.py`:

   1. Look for the section titled `# EGL API - EGL/egl.h (no function pointers, yet @@@)`
   1. Change `genFuncPointers   = False,` to `genFuncPointers   = True,`
   1. Change `protectProto      = False,` to `protectProto      = 'nonzero',`
   1. Change `protectProtoStr   = 'EGL_EGLEXT_PROTOTYPES',` to `protectProtoStr   = 'EGL_EGL_PROTOTYPES',`

1. Set your working directory to `EGL-Registry/api/`.
1. Run `python genheaders.py -registry egl.xml EGL/egl.h`
1. The generated header will now be in `EGL-Registry/api/EGL/egl.h`. You can copy the header over to this folder.
1. Also update `scripts/egl.xml` with the latest version from `EGL-Registry/api/`.
