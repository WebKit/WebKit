# Metal Backend Shaders

ANGLE's Metal backend uses the general [shader dumping and
substitution](ShaderSubstitution.md) features for inspecting the
Metal Shading Language (MSL) produced by ANGLE's shader translator.

## Dumping the MSL

When the feature `dumpTranslatedShaders` is enabled, the Metal backend
writes two kinds of files to the shader dump directory:

- `<hash>.translated`: the MSL the shader translator produces for
  each shader, named by the hash of the ESSL source. This file can be
  edited and substituted with `enableTranslatedShaderSubstitution`.
- `<hash>.metal`: the MSL the program link passes to the Metal compiler,
  named by the hash of the file contents. The link rewrites the
  translated MSL of each shader (vertex attributes, transform feedback
  and fragment outputs). The file starts with the compile options and
  with the preprocessor macros of the program variant as `#define`s. One
  file is written per compiled program variant. These files are not read
  back.

The Metal backend's internal shaders are not dumped.

If `ANGLE_SHADER_DUMP_PATH` is not set, the files go to `TMPDIR`.

```
mkdir -p /tmp/angle_shaders
export ANGLE_SHADER_DUMP_PATH=/tmp/angle_shaders
export ANGLE_FEATURE_OVERRIDES_ENABLED=dumpTranslatedShaders
/Applications/Google\ Chrome\ Canary.app/Contents/MacOS/Google\ Chrome\ Canary --use-angle=metal --disable-gpu-sandbox
```

To pass this environment to WebKit / Safari's GPU process, prefix the
environment variables with `__XPC_`. The GPU process sandbox allows
writing only to the GPU process's own temporary and cache directories.
Its `TMPDIR` is the parent of those directories and is not writable, so
set the dump path to the temporary directory. It is
`com.apple.WebKit.GPU+<application bundle identifier>` inside
`getconf DARWIN_USER_TEMP_DIR`:

```
export __XPC_ANGLE_SHADER_DUMP_PATH="$(getconf DARWIN_USER_TEMP_DIR)com.apple.WebKit.GPU+com.apple.SafariTechnologyPreview"
export __XPC_ANGLE_FEATURE_OVERRIDES_ENABLED=dumpTranslatedShaders
/Applications/Safari\ Technology\ Preview.app/Contents/MacOS/Safari\ Technology\ Preview
```
