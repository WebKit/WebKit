/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/egl_dmabuf.h"

#include <asm/ioctl.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <linux/types.h>
#include <spa/param/video/format-utils.h>
#include <unistd.h>
#include <xf86drm.h>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/sanitizer.h"
#include "rtc_base/string_encode.h"

namespace webrtc {

// EGL
typedef EGLBoolean (*eglBindAPI_func)(EGLenum api);
typedef EGLContext (*eglCreateContext_func)(EGLDisplay dpy,
                                            EGLConfig config,
                                            EGLContext share_context,
                                            const EGLint* attrib_list);
typedef EGLImageKHR (*eglCreateImageKHR_func)(EGLDisplay dpy,
                                              EGLContext ctx,
                                              EGLenum target,
                                              EGLClientBuffer buffer,
                                              const EGLint* attrib_list);
typedef EGLBoolean (*eglDestroyImageKHR_func)(EGLDisplay dpy,
                                              EGLImageKHR image);
typedef EGLint (*eglGetError_func)(void);
typedef void* (*eglGetProcAddress_func)(const char*);
typedef EGLDisplay (*eglGetPlatformDisplayEXT_func)(EGLenum platform,
                                                    void* native_display,
                                                    const EGLint* attrib_list);
typedef EGLBoolean (*eglInitialize_func)(EGLDisplay dpy,
                                         EGLint* major,
                                         EGLint* minor);
typedef EGLBoolean (*eglMakeCurrent_func)(EGLDisplay dpy,
                                          EGLSurface draw,
                                          EGLSurface read,
                                          EGLContext ctx);
typedef EGLBoolean (*eglQueryDmaBufFormatsEXT_func)(EGLDisplay dpy,
                                                    EGLint max_formats,
                                                    EGLint* formats,
                                                    EGLint* num_formats);
typedef EGLBoolean (*eglQueryDmaBufModifiersEXT_func)(EGLDisplay dpy,
                                                      EGLint format,
                                                      EGLint max_modifiers,
                                                      EGLuint64KHR* modifiers,
                                                      EGLBoolean* external_only,
                                                      EGLint* num_modifiers);
typedef const char* (*eglQueryString_func)(EGLDisplay dpy, EGLint name);
typedef void (*glEGLImageTargetTexture2DOES_func)(GLenum target,
                                                  GLeglImageOES image);

// This doesn't follow naming conventions in WebRTC, where the naming
// should look like e.g. egl_bind_api instead of EglBindAPI, however
// we named them according to the exported functions they map to for
// consistency.
eglBindAPI_func EglBindAPI = nullptr;
eglCreateContext_func EglCreateContext = nullptr;
eglCreateImageKHR_func EglCreateImageKHR = nullptr;
eglDestroyImageKHR_func EglDestroyImageKHR = nullptr;
eglGetError_func EglGetError = nullptr;
eglGetProcAddress_func EglGetProcAddress = nullptr;
eglGetPlatformDisplayEXT_func EglGetPlatformDisplayEXT = nullptr;
eglInitialize_func EglInitialize = nullptr;
eglMakeCurrent_func EglMakeCurrent = nullptr;
eglQueryDmaBufFormatsEXT_func EglQueryDmaBufFormatsEXT = nullptr;
eglQueryDmaBufModifiersEXT_func EglQueryDmaBufModifiersEXT = nullptr;
eglQueryString_func EglQueryString = nullptr;
glEGLImageTargetTexture2DOES_func GlEGLImageTargetTexture2DOES = nullptr;

// GL
typedef void (*glBindTexture_func)(GLenum target, GLuint texture);
typedef void (*glDeleteTextures_func)(GLsizei n, const GLuint* textures);
typedef void (*glGenTextures_func)(GLsizei n, GLuint* textures);
typedef GLenum (*glGetError_func)(void);
typedef const GLubyte* (*glGetString_func)(GLenum name);
typedef void (*glGetTexImage_func)(GLenum target,
                                   GLint level,
                                   GLenum format,
                                   GLenum type,
                                   void* pixels);
typedef void (*glTexParameteri_func)(GLenum target, GLenum pname, GLint param);
typedef void* (*glXGetProcAddressARB_func)(const char*);

// This doesn't follow naming conventions in WebRTC, where the naming
// should look like e.g. egl_bind_api instead of EglBindAPI, however
// we named them according to the exported functions they map to for
// consistency.
glBindTexture_func GlBindTexture = nullptr;
glDeleteTextures_func GlDeleteTextures = nullptr;
glGenTextures_func GlGenTextures = nullptr;
glGetError_func GlGetError = nullptr;
glGetString_func GlGetString = nullptr;
glGetTexImage_func GlGetTexImage = nullptr;
glTexParameteri_func GlTexParameteri = nullptr;
glXGetProcAddressARB_func GlXGetProcAddressARB = nullptr;

static const std::string FormatGLError(GLenum err) {
  switch (err) {
    case GL_NO_ERROR:
      return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
      return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION";
    case GL_STACK_OVERFLOW:
      return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW:
      return "GL_STACK_UNDERFLOW";
    case GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY";
    default:
      return std::string("0x") + std::to_string(err);
  }
}

static uint32_t SpaPixelFormatToDrmFormat(uint32_t spa_format) {
  switch (spa_format) {
    case SPA_VIDEO_FORMAT_RGBA:
      return DRM_FORMAT_ABGR8888;
    case SPA_VIDEO_FORMAT_RGBx:
      return DRM_FORMAT_XBGR8888;
    case SPA_VIDEO_FORMAT_BGRA:
      return DRM_FORMAT_ARGB8888;
    case SPA_VIDEO_FORMAT_BGRx:
      return DRM_FORMAT_XRGB8888;
    default:
      return DRM_FORMAT_INVALID;
  }
}

static void CloseLibrary(void* library) {
  if (library) {
    dlclose(library);
    library = nullptr;
  }
}

static void* g_lib_egl = nullptr;

RTC_NO_SANITIZE("cfi-icall")
static bool OpenEGL() {
  g_lib_egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
  if (g_lib_egl) {
    EglGetProcAddress =
        (eglGetProcAddress_func)dlsym(g_lib_egl, "eglGetProcAddress");
    return EglGetProcAddress;
  }

  return false;
}

RTC_NO_SANITIZE("cfi-icall")
static bool LoadEGL() {
  if (OpenEGL()) {
    EglBindAPI = (eglBindAPI_func)EglGetProcAddress("eglBindAPI");
    EglCreateContext =
        (eglCreateContext_func)EglGetProcAddress("eglCreateContext");
    EglCreateImageKHR =
        (eglCreateImageKHR_func)EglGetProcAddress("eglCreateImageKHR");
    EglDestroyImageKHR =
        (eglDestroyImageKHR_func)EglGetProcAddress("eglDestroyImageKHR");
    EglGetError = (eglGetError_func)EglGetProcAddress("eglGetError");
    EglGetPlatformDisplayEXT = (eglGetPlatformDisplayEXT_func)EglGetProcAddress(
        "eglGetPlatformDisplayEXT");
    EglInitialize = (eglInitialize_func)EglGetProcAddress("eglInitialize");
    EglMakeCurrent = (eglMakeCurrent_func)EglGetProcAddress("eglMakeCurrent");
    EglQueryString = (eglQueryString_func)EglGetProcAddress("eglQueryString");
    GlEGLImageTargetTexture2DOES =
        (glEGLImageTargetTexture2DOES_func)EglGetProcAddress(
            "glEGLImageTargetTexture2DOES");

    return EglBindAPI && EglCreateContext && EglCreateImageKHR &&
           EglDestroyImageKHR && EglGetError && EglGetPlatformDisplayEXT &&
           EglInitialize && EglMakeCurrent && EglQueryString &&
           GlEGLImageTargetTexture2DOES;
  }

  return false;
}

static void* g_lib_gl = nullptr;

RTC_NO_SANITIZE("cfi-icall")
static bool OpenGL() {
  std::vector<std::string> names = {"libGL.so.1", "libGL.so"};
  for (const std::string& name : names) {
    g_lib_gl = dlopen(name.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (g_lib_gl) {
      GlXGetProcAddressARB =
          (glXGetProcAddressARB_func)dlsym(g_lib_gl, "glXGetProcAddressARB");
      return GlXGetProcAddressARB;
    }
  }

  return false;
}

RTC_NO_SANITIZE("cfi-icall")
static bool LoadGL() {
  if (OpenGL()) {
    GlGetString = (glGetString_func)GlXGetProcAddressARB("glGetString");
    if (!GlGetString) {
      return false;
    }

    GlBindTexture = (glBindTexture_func)GlXGetProcAddressARB("glBindTexture");
    GlDeleteTextures =
        (glDeleteTextures_func)GlXGetProcAddressARB("glDeleteTextures");
    GlGenTextures = (glGenTextures_func)GlXGetProcAddressARB("glGenTextures");
    GlGetError = (glGetError_func)GlXGetProcAddressARB("glGetError");
    GlGetTexImage = (glGetTexImage_func)GlXGetProcAddressARB("glGetTexImage");
    GlTexParameteri =
        (glTexParameteri_func)GlXGetProcAddressARB("glTexParameteri");

    return GlBindTexture && GlDeleteTextures && GlGenTextures && GlGetError &&
           GlGetTexImage && GlTexParameteri;
  }

  return false;
}

RTC_NO_SANITIZE("cfi-icall")
EglDmaBuf::EglDmaBuf() {
  absl::optional<std::string> render_node = GetRenderNode();
  if (!render_node) {
    return;
  }

  drm_fd_ = open(render_node->c_str(), O_RDWR);

  if (drm_fd_ < 0) {
    RTC_LOG(LS_ERROR) << "Failed to open drm render node: " << strerror(errno);
    return;
  }

  gbm_device_ = gbm_create_device(drm_fd_);

  if (!gbm_device_) {
    RTC_LOG(LS_ERROR) << "Cannot create GBM device: " << strerror(errno);
    return;
  }

  if (!LoadEGL()) {
    RTC_LOG(LS_ERROR) << "Unable to load EGL entry functions.";
    return;
  }

  if (!LoadGL()) {
    RTC_LOG(LS_ERROR) << "Failed to load OpenGL entry functions.";
    return;
  }

  // Get the list of client extensions
  const char* client_extensions_cstring_no_display =
      EglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  std::string client_extensions_string = client_extensions_cstring_no_display;
  if (!client_extensions_cstring_no_display) {
    // If eglQueryString() returned NULL, the implementation doesn't support
    // EGL_EXT_client_extensions. Expect an EGL_BAD_DISPLAY error.
    RTC_LOG(LS_ERROR) << "No client extensions defined! "
                      << FormatGLError(EglGetError());
    return;
  }

  std::vector<std::string> client_extensions_no_display;
  rtc::split(client_extensions_cstring_no_display, ' ',
             &client_extensions_no_display);
  for (const auto& extension : client_extensions_no_display) {
    egl_.extensions.push_back(std::string(extension));
  }

  bool has_platform_base_ext = false;
  bool has_platform_gbm_ext = false;

  for (const auto& extension : egl_.extensions) {
    if (extension == "EGL_EXT_platform_base") {
      has_platform_base_ext = true;
      continue;
    } else if (extension == "EGL_MESA_platform_gbm") {
      has_platform_gbm_ext = true;
      continue;
    }
  }

  if (!has_platform_base_ext || !has_platform_gbm_ext) {
    RTC_LOG(LS_ERROR) << "One of required EGL extensions is missing";
    return;
  }

  // Use eglGetPlatformDisplayEXT() to get the display pointer
  // if the implementation supports it.
  egl_.display =
      EglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, gbm_device_, nullptr);

  if (egl_.display == EGL_NO_DISPLAY) {
    RTC_LOG(LS_ERROR) << "Error during obtaining EGL display: "
                      << FormatGLError(EglGetError());
    return;
  }

  EGLint major, minor;
  if (EglInitialize(egl_.display, &major, &minor) == EGL_FALSE) {
    RTC_LOG(LS_ERROR) << "Error during eglInitialize: "
                      << FormatGLError(EglGetError());
    return;
  }

  if (EglBindAPI(EGL_OPENGL_API) == EGL_FALSE) {
    RTC_LOG(LS_ERROR) << "bind OpenGL API failed";
    return;
  }

  egl_.context =
      EglCreateContext(egl_.display, nullptr, EGL_NO_CONTEXT, nullptr);

  if (egl_.context == EGL_NO_CONTEXT) {
    RTC_LOG(LS_ERROR) << "Couldn't create EGL context: "
                      << FormatGLError(EglGetError());
    return;
  }

  const char* client_extensions_cstring_display =
      EglQueryString(egl_.display, EGL_EXTENSIONS);
  client_extensions_string = client_extensions_cstring_display;

  std::vector<std::string> client_extensions;
  rtc::split(client_extensions_string, ' ', &client_extensions);
  for (const auto& extension : client_extensions) {
    egl_.extensions.push_back(std::string(extension));
  }

  bool has_image_dma_buf_import_ext = false;
  bool has_image_dma_buf_import_modifiers_ext = false;

  for (const auto& extension : egl_.extensions) {
    if (extension == "EGL_EXT_image_dma_buf_import") {
      has_image_dma_buf_import_ext = true;
      continue;
    } else if (extension == "EGL_EXT_image_dma_buf_import_modifiers") {
      has_image_dma_buf_import_modifiers_ext = true;
      continue;
    }
  }

  if (has_image_dma_buf_import_ext && has_image_dma_buf_import_modifiers_ext) {
    EglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXT_func)EglGetProcAddress(
        "eglQueryDmaBufFormatsEXT");
    EglQueryDmaBufModifiersEXT =
        (eglQueryDmaBufModifiersEXT_func)EglGetProcAddress(
            "eglQueryDmaBufModifiersEXT");
  }

  RTC_LOG(LS_INFO) << "Egl initialization succeeded";
  egl_initialized_ = true;
}

EglDmaBuf::~EglDmaBuf() {
  if (gbm_device_) {
    gbm_device_destroy(gbm_device_);
  }

  CloseLibrary(g_lib_egl);
  CloseLibrary(g_lib_gl);
}

RTC_NO_SANITIZE("cfi-icall")
std::unique_ptr<uint8_t[]> EglDmaBuf::ImageFromDmaBuf(const DesktopSize& size,
                                                      uint32_t format,
                                                      uint32_t n_planes,
                                                      const int32_t* fds,
                                                      const uint32_t* strides,
                                                      const uint32_t* offsets,
                                                      uint64_t modifier) {
  std::unique_ptr<uint8_t[]> src;

  if (!egl_initialized_) {
    return src;
  }

  if (n_planes <= 0) {
    RTC_LOG(LS_ERROR) << "Failed to process buffer: invalid number of planes";
    return src;
  }

  gbm_bo* imported;
  if (modifier == DRM_FORMAT_MOD_INVALID) {
    gbm_import_fd_data import_info = {fds[0],
                                      static_cast<uint32_t>(size.width()),
                                      static_cast<uint32_t>(size.height()),
                                      strides[0], GBM_BO_FORMAT_ARGB8888};

    imported = gbm_bo_import(gbm_device_, GBM_BO_IMPORT_FD, &import_info, 0);
  } else {
    gbm_import_fd_modifier_data import_info = {};
    import_info.format = GBM_BO_FORMAT_ARGB8888;
    import_info.width = static_cast<uint32_t>(size.width());
    import_info.height = static_cast<uint32_t>(size.height());
    import_info.num_fds = n_planes;
    import_info.modifier = modifier;
    for (uint32_t i = 0; i < n_planes; i++) {
      import_info.fds[i] = fds[i];
      import_info.offsets[i] = offsets[i];
      import_info.strides[i] = strides[i];
    }

    imported =
        gbm_bo_import(gbm_device_, GBM_BO_IMPORT_FD_MODIFIER, &import_info, 0);
  }

  if (!imported) {
    RTC_LOG(LS_ERROR)
        << "Failed to process buffer: Cannot import passed GBM fd - "
        << strerror(errno);
    return src;
  }

  // bind context to render thread
  EglMakeCurrent(egl_.display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_.context);

  // create EGL image from imported BO
  EGLImageKHR image = EglCreateImageKHR(
      egl_.display, nullptr, EGL_NATIVE_PIXMAP_KHR, imported, nullptr);

  if (image == EGL_NO_IMAGE_KHR) {
    RTC_LOG(LS_ERROR) << "Failed to record frame: Error creating EGLImageKHR - "
                      << FormatGLError(GlGetError());
    gbm_bo_destroy(imported);
    return src;
  }

  // create GL 2D texture for framebuffer
  GLuint texture;
  GlGenTextures(1, &texture);
  GlTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  GlTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  GlTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  GlTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  GlBindTexture(GL_TEXTURE_2D, texture);
  GlEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

  src = std::make_unique<uint8_t[]>(strides[0] * size.height());

  GLenum gl_format = GL_BGRA;
  switch (format) {
    case SPA_VIDEO_FORMAT_RGBx:
      gl_format = GL_RGBA;
      break;
    case SPA_VIDEO_FORMAT_RGBA:
      gl_format = GL_RGBA;
      break;
    case SPA_VIDEO_FORMAT_BGRx:
      gl_format = GL_BGRA;
      break;
    case SPA_VIDEO_FORMAT_RGB:
      gl_format = GL_RGB;
      break;
    case SPA_VIDEO_FORMAT_BGR:
      gl_format = GL_BGR;
      break;
    default:
      gl_format = GL_BGRA;
      break;
  }
  GlGetTexImage(GL_TEXTURE_2D, 0, gl_format, GL_UNSIGNED_BYTE, src.get());

  if (GlGetError()) {
    RTC_LOG(LS_ERROR) << "Failed to get image from DMA buffer.";
    gbm_bo_destroy(imported);
    return src;
  }

  GlDeleteTextures(1, &texture);
  EglDestroyImageKHR(egl_.display, image);

  gbm_bo_destroy(imported);

  return src;
}

RTC_NO_SANITIZE("cfi-icall")
std::vector<uint64_t> EglDmaBuf::QueryDmaBufModifiers(uint32_t format) {
  if (!egl_initialized_) {
    return {};
  }

  // Modifiers not supported, return just DRM_FORMAT_MOD_INVALID as we can still
  // use modifier-less DMA-BUFs
  if (EglQueryDmaBufFormatsEXT == nullptr ||
      EglQueryDmaBufModifiersEXT == nullptr) {
    return {DRM_FORMAT_MOD_INVALID};
  }

  uint32_t drm_format = SpaPixelFormatToDrmFormat(format);
  if (drm_format == DRM_FORMAT_INVALID) {
    RTC_LOG(LS_ERROR) << "Failed to find matching DRM format.";
    return {DRM_FORMAT_MOD_INVALID};
  }

  EGLint count = 0;
  EGLBoolean success =
      EglQueryDmaBufFormatsEXT(egl_.display, 0, nullptr, &count);

  if (!success || !count) {
    RTC_LOG(LS_ERROR) << "Failed to query DMA-BUF formats.";
    return {DRM_FORMAT_MOD_INVALID};
  }

  std::vector<uint32_t> formats(count);
  if (!EglQueryDmaBufFormatsEXT(egl_.display, count,
                                reinterpret_cast<EGLint*>(formats.data()),
                                &count)) {
    RTC_LOG(LS_ERROR) << "Failed to query DMA-BUF formats.";
    return {DRM_FORMAT_MOD_INVALID};
  }

  if (std::find(formats.begin(), formats.end(), drm_format) == formats.end()) {
    RTC_LOG(LS_ERROR) << "Format " << drm_format
                      << " not supported for modifiers.";
    return {DRM_FORMAT_MOD_INVALID};
  }

  success = EglQueryDmaBufModifiersEXT(egl_.display, drm_format, 0, nullptr,
                                       nullptr, &count);

  if (!success || !count) {
    RTC_LOG(LS_ERROR) << "Failed to query DMA-BUF modifiers.";
    return {DRM_FORMAT_MOD_INVALID};
  }

  std::vector<uint64_t> modifiers(count);
  if (!EglQueryDmaBufModifiersEXT(egl_.display, drm_format, count,
                                  modifiers.data(), nullptr, &count)) {
    RTC_LOG(LS_ERROR) << "Failed to query DMA-BUF modifiers.";
  }

  // Support modifier-less buffers
  modifiers.push_back(DRM_FORMAT_MOD_INVALID);
  return modifiers;
}

absl::optional<std::string> EglDmaBuf::GetRenderNode() {
  int max_devices = drmGetDevices2(0, nullptr, 0);
  if (max_devices <= 0) {
    RTC_LOG(LS_ERROR) << "drmGetDevices2() has not found any devices (errno="
                      << -max_devices << ")";
    return absl::nullopt;
  }

  std::vector<drmDevicePtr> devices(max_devices);
  int ret = drmGetDevices2(0, devices.data(), max_devices);
  if (ret < 0) {
    RTC_LOG(LS_ERROR) << "drmGetDevices2() returned an error " << ret;
    return absl::nullopt;
  }

  std::string render_node;

  for (const drmDevicePtr& device : devices) {
    if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
      render_node = device->nodes[DRM_NODE_RENDER];
      break;
    }
  }

  drmFreeDevices(devices.data(), ret);
  return render_node;
}

}  // namespace webrtc
