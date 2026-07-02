#ifndef _EGLUUTIL_HPP
#define _EGLUUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief EGL utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "egluDefs.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativeDisplay.hpp"
#include "eglwDefs.hpp"

#include <vector>
#include <map>
#include <string>

namespace tcu
{
class CommandLine;
}

namespace eglw
{
class Library;
}

namespace eglu
{

class NativePixmapFactory;
class NativePixmap;
class FilterList;

typedef std::map<eglw::EGLint, eglw::EGLint> AttribMap;

std::vector<eglw::EGLint> attribMapToList(const AttribMap &map);

Version getVersion(const eglw::Library &egl, eglw::EGLDisplay display);

std::vector<std::string> getClientExtensions(const eglw::Library &egl);
std::vector<std::string> getDisplayExtensions(const eglw::Library &egl, eglw::EGLDisplay display);
bool hasExtension(const eglw::Library &egl, eglw::EGLDisplay display, const std::string &extName);

std::vector<eglw::EGLConfig> getConfigs(const eglw::Library &egl, eglw::EGLDisplay display);
std::vector<eglw::EGLConfig> chooseConfigs(const eglw::Library &egl, eglw::EGLDisplay display,
                                           const AttribMap &attribs);
std::vector<eglw::EGLConfig> chooseConfigs(const eglw::Library &egl, eglw::EGLDisplay display,
                                           const FilterList &filters);
std::vector<eglw::EGLConfig> chooseConfigs(const eglw::Library &egl, eglw::EGLDisplay display,
                                           const eglw::EGLint *attribs);
eglw::EGLConfig chooseSingleConfig(const eglw::Library &egl, eglw::EGLDisplay display, const AttribMap &attribs);
eglw::EGLConfig chooseSingleConfig(const eglw::Library &egl, eglw::EGLDisplay display, const eglw::EGLint *attribs);
eglw::EGLConfig chooseSingleConfig(const eglw::Library &egl, eglw::EGLDisplay display, const FilterList &filters);
eglw::EGLConfig chooseConfigByID(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLint id);
eglw::EGLint getConfigAttribInt(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLConfig config,
                                eglw::EGLint attrib);
eglw::EGLint getConfigID(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLConfig config);

eglw::EGLint querySurfaceInt(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLSurface surface,
                             eglw::EGLint attrib);
tcu::IVec2 getSurfaceSize(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLSurface surface);
tcu::IVec2 getSurfaceResolution(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLSurface surface);

eglw::EGLDisplay getDisplay(NativeDisplay &nativeDisplay);
eglw::EGLDisplay getAndInitDisplay(NativeDisplay &nativeDisplay, Version *version = nullptr);
void terminateDisplay(const eglw::Library &egl, eglw::EGLDisplay display);
eglw::EGLSurface createWindowSurface(NativeDisplay &nativeDisplay, NativeWindow &window, eglw::EGLDisplay display,
                                     eglw::EGLConfig config, const eglw::EGLAttrib *attribList);
eglw::EGLSurface createPixmapSurface(NativeDisplay &nativeDisplay, NativePixmap &pixmap, eglw::EGLDisplay display,
                                     eglw::EGLConfig config, const eglw::EGLAttrib *attribList);

const NativeDisplayFactory &selectNativeDisplayFactory(const NativeDisplayFactoryRegistry &registry,
                                                       const tcu::CommandLine &cmdLine);
const NativeWindowFactory &selectNativeWindowFactory(const NativeDisplayFactory &factory,
                                                     const tcu::CommandLine &cmdLine);
const NativePixmapFactory &selectNativePixmapFactory(const NativeDisplayFactory &factory,
                                                     const tcu::CommandLine &cmdLine);

WindowParams::Visibility parseWindowVisibility(const tcu::CommandLine &commandLine);
eglw::EGLenum parseClientAPI(const std::string &api);
std::vector<eglw::EGLenum> parseClientAPIs(const std::string &apiList);
std::vector<eglw::EGLenum> getClientAPIs(const eglw::Library &egl, eglw::EGLDisplay display);

eglw::EGLint getRenderableAPIsMask(const eglw::Library &egl, eglw::EGLDisplay display);

std::vector<eglw::EGLint> toLegacyAttribList(const eglw::EGLAttrib *attribs);

} // namespace eglu

#endif // _EGLUUTIL_HPP
