#ifndef _EGLUNATIVEDISPLAY_HPP
#define _EGLUNATIVEDISPLAY_HPP
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
 * \brief EGL native display abstraction
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuFactoryRegistry.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "eglwDefs.hpp"

#include <string>

namespace eglw
{
class Library;
}

namespace eglu
{

class NativeDisplay
{
public:
	enum Capability
	{
		CAPABILITY_GET_DISPLAY_LEGACY		= (1<<0),	//!< Query EGL display using eglGetDisplay()
		CAPABILITY_GET_DISPLAY_PLATFORM		= (1<<1),	//!< Query EGL display using eglGetPlatformDisplay()
		CAPABILITY_GET_DISPLAY_PLATFORM_EXT = (1<<2)	//!< Query EGL display using eglGetPlatformDisplayEXT()
	};

	virtual								~NativeDisplay				(void);

	virtual const eglw::Library&		getLibrary					(void) const = 0;

	Capability							getCapabilities				(void) const { return m_capabilities; }
	eglw::EGLenum						getPlatformType				(void) const { return m_platformType; }
	const char*							getPlatformExtensionName	(void) const { return (m_platformExtension.empty() ? DE_NULL : m_platformExtension.c_str()); }

	//! Get EGLNativeDisplayType that can be used with eglGetDisplay(). Default implementation throws tcu::NotSupportedError().
	virtual eglw::EGLNativeDisplayType	getLegacyNative				(void);

	//! Return display pointer that can be used with eglGetPlatformDisplay(). Default implementations throw tcu::NotSupportedError()
	virtual void*						getPlatformNative			(void);

	//! Attributes to pass to eglGetPlatformDisplay(EXT)
	virtual const eglw::EGLAttrib*		getPlatformAttributes		(void) const;

protected:
										NativeDisplay				(Capability capabilities, eglw::EGLenum platformType, const char* platformExtension);
										NativeDisplay				(Capability capabilities);

private:
										NativeDisplay				(const NativeDisplay&);
	NativeDisplay&						operator=					(const NativeDisplay&);

	const Capability					m_capabilities;
	const eglw::EGLenum					m_platformType;				//!< EGL platform type, or EGL_NONE if not supported.
	const std::string					m_platformExtension;
};

class NativeDisplayFactory : public tcu::FactoryBase
{
public:
	virtual								~NativeDisplayFactory		(void);

	virtual NativeDisplay*				createDisplay				(const eglw::EGLAttrib* attribList = DE_NULL) const = 0;

	NativeDisplay::Capability			getCapabilities				(void) const { return m_capabilities; }
	eglw::EGLenum						getPlatformType				(void) const { return m_platformType; }
	const char*							getPlatformExtensionName	(void) const { return (m_platformExtension.empty() ? DE_NULL : m_platformExtension.c_str()); }

	const NativeWindowFactoryRegistry&	getNativeWindowRegistry		(void) const { return m_nativeWindowRegistry; }
	const NativePixmapFactoryRegistry&	getNativePixmapRegistry		(void) const { return m_nativePixmapRegistry; }

protected:
										NativeDisplayFactory		(const std::string& name, const std::string& description, NativeDisplay::Capability capabilities);
										NativeDisplayFactory		(const std::string& name, const std::string& description, NativeDisplay::Capability capabilities, eglw::EGLenum platformType, const char* platformExtension);

	NativeWindowFactoryRegistry			m_nativeWindowRegistry;
	NativePixmapFactoryRegistry			m_nativePixmapRegistry;

private:
										NativeDisplayFactory		(const NativeDisplayFactory&);
	NativeDisplayFactory&				operator=					(const NativeDisplayFactory&);

	const NativeDisplay::Capability		m_capabilities;
	const eglw::EGLenum					m_platformType;
	const std::string					m_platformExtension;
};

typedef tcu::FactoryRegistry<NativeDisplayFactory> NativeDisplayFactoryRegistry;

} // eglu

#endif // _EGLUNATIVEDISPLAY_HPP
