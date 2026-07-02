/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos EGL API description (egl.xml) revision 682c662d48fbae076c5ed89a1bd5b2aa7e2e4449.
 */

eglw::EGLBoolean CallLogWrapper::eglBindAPI (eglw::EGLenum api)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglBindAPI(" << getAPIStr(api) << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.bindAPI(api);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglBindTexImage (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglBindTexImage(" << dpy << ", " << toHex(surface) << ", " << buffer << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.bindTexImage(dpy, surface, buffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglChooseConfig (eglw::EGLDisplay dpy, const eglw::EGLint *attrib_list, eglw::EGLConfig *configs, eglw::EGLint config_size, eglw::EGLint *num_config)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglChooseConfig(" << dpy << ", " << getConfigAttribListStr(attrib_list) << ", " << configs << ", " << config_size << ", " << num_config << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.chooseConfig(dpy, attrib_list, configs, config_size, num_config);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// configs = " << getPointerStr(configs, (num_config && returnValue) ? deMin32(config_size, *num_config) : 0) << TestLog::EndMessage;
		m_log << TestLog::Message << "// num_config = " << (num_config ? de::toString(*num_config) : "NULL") << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLint CallLogWrapper::eglClientWaitSync (eglw::EGLDisplay dpy, eglw::EGLSync sync, eglw::EGLint flags, eglw::EGLTime timeout)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglClientWaitSync(" << dpy << ", " << sync << ", " << flags << ", " << timeout << ");" << TestLog::EndMessage;
	eglw::EGLint returnValue = m_egl.clientWaitSync(dpy, sync, flags, timeout);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLint CallLogWrapper::eglClientWaitSyncKHR (eglw::EGLDisplay dpy, eglw::EGLSyncKHR sync, eglw::EGLint flags, eglw::EGLTimeKHR timeout)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglClientWaitSyncKHR(" << dpy << ", " << sync << ", " << flags << ", " << timeout << ");" << TestLog::EndMessage;
	eglw::EGLint returnValue = m_egl.clientWaitSyncKHR(dpy, sync, flags, timeout);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglCopyBuffers (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLNativePixmapType target)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCopyBuffers(" << dpy << ", " << toHex(surface) << ", " << toHex(target) << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.copyBuffers(dpy, surface, target);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLContext CallLogWrapper::eglCreateContext (eglw::EGLDisplay dpy, eglw::EGLConfig config, eglw::EGLContext share_context, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateContext(" << dpy << ", " << toHex(config) << ", " << share_context << ", " << getContextAttribListStr(attrib_list) << ");" << TestLog::EndMessage;
	eglw::EGLContext returnValue = m_egl.createContext(dpy, config, share_context, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLImage CallLogWrapper::eglCreateImage (eglw::EGLDisplay dpy, eglw::EGLContext ctx, eglw::EGLenum target, eglw::EGLClientBuffer buffer, const eglw::EGLAttrib *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateImage(" << dpy << ", " << ctx << ", " << toHex(target) << ", " << toHex(buffer) << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLImage returnValue = m_egl.createImage(dpy, ctx, target, buffer, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLImageKHR CallLogWrapper::eglCreateImageKHR (eglw::EGLDisplay dpy, eglw::EGLContext ctx, eglw::EGLenum target, eglw::EGLClientBuffer buffer, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateImageKHR(" << dpy << ", " << ctx << ", " << toHex(target) << ", " << toHex(buffer) << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLImageKHR returnValue = m_egl.createImageKHR(dpy, ctx, target, buffer, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglCreatePbufferFromClientBuffer (eglw::EGLDisplay dpy, eglw::EGLenum buftype, eglw::EGLClientBuffer buffer, eglw::EGLConfig config, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePbufferFromClientBuffer(" << dpy << ", " << toHex(buftype) << ", " << toHex(buffer) << ", " << toHex(config) << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.createPbufferFromClientBuffer(dpy, buftype, buffer, config, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglCreatePbufferSurface (eglw::EGLDisplay dpy, eglw::EGLConfig config, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePbufferSurface(" << dpy << ", " << toHex(config) << ", " << getSurfaceAttribListStr(attrib_list) << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.createPbufferSurface(dpy, config, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglCreatePixmapSurface (eglw::EGLDisplay dpy, eglw::EGLConfig config, eglw::EGLNativePixmapType pixmap, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePixmapSurface(" << dpy << ", " << toHex(config) << ", " << toHex(pixmap) << ", " << getSurfaceAttribListStr(attrib_list) << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.createPixmapSurface(dpy, config, pixmap, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglCreatePlatformPixmapSurface (eglw::EGLDisplay dpy, eglw::EGLConfig config, void *native_pixmap, const eglw::EGLAttrib *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePlatformPixmapSurface(" << dpy << ", " << toHex(config) << ", " << native_pixmap << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.createPlatformPixmapSurface(dpy, config, native_pixmap, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglCreatePlatformPixmapSurfaceEXT (eglw::EGLDisplay dpy, eglw::EGLConfig config, void *native_pixmap, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePlatformPixmapSurfaceEXT(" << dpy << ", " << toHex(config) << ", " << native_pixmap << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.createPlatformPixmapSurfaceEXT(dpy, config, native_pixmap, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglCreatePlatformWindowSurface (eglw::EGLDisplay dpy, eglw::EGLConfig config, void *native_window, const eglw::EGLAttrib *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePlatformWindowSurface(" << dpy << ", " << toHex(config) << ", " << native_window << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.createPlatformWindowSurface(dpy, config, native_window, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglCreatePlatformWindowSurfaceEXT (eglw::EGLDisplay dpy, eglw::EGLConfig config, void *native_window, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreatePlatformWindowSurfaceEXT(" << dpy << ", " << toHex(config) << ", " << native_window << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.createPlatformWindowSurfaceEXT(dpy, config, native_window, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSync CallLogWrapper::eglCreateSync (eglw::EGLDisplay dpy, eglw::EGLenum type, const eglw::EGLAttrib *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateSync(" << dpy << ", " << toHex(type) << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLSync returnValue = m_egl.createSync(dpy, type, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSyncKHR CallLogWrapper::eglCreateSyncKHR (eglw::EGLDisplay dpy, eglw::EGLenum type, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateSyncKHR(" << dpy << ", " << toHex(type) << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLSyncKHR returnValue = m_egl.createSyncKHR(dpy, type, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglCreateWindowSurface (eglw::EGLDisplay dpy, eglw::EGLConfig config, eglw::EGLNativeWindowType win, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglCreateWindowSurface(" << dpy << ", " << toHex(config) << ", " << toHex(win) << ", " << getSurfaceAttribListStr(attrib_list) << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.createWindowSurface(dpy, config, win, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglDestroyContext (eglw::EGLDisplay dpy, eglw::EGLContext ctx)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroyContext(" << dpy << ", " << ctx << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.destroyContext(dpy, ctx);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglDestroyImage (eglw::EGLDisplay dpy, eglw::EGLImage image)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroyImage(" << dpy << ", " << image << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.destroyImage(dpy, image);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglDestroyImageKHR (eglw::EGLDisplay dpy, eglw::EGLImageKHR image)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroyImageKHR(" << dpy << ", " << image << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.destroyImageKHR(dpy, image);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglDestroySurface (eglw::EGLDisplay dpy, eglw::EGLSurface surface)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroySurface(" << dpy << ", " << toHex(surface) << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.destroySurface(dpy, surface);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglDestroySync (eglw::EGLDisplay dpy, eglw::EGLSync sync)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroySync(" << dpy << ", " << sync << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.destroySync(dpy, sync);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglDestroySyncKHR (eglw::EGLDisplay dpy, eglw::EGLSyncKHR sync)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglDestroySyncKHR(" << dpy << ", " << sync << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.destroySyncKHR(dpy, sync);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglGetConfigAttrib (eglw::EGLDisplay dpy, eglw::EGLConfig config, eglw::EGLint attribute, eglw::EGLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetConfigAttrib(" << dpy << ", " << toHex(config) << ", " << getConfigAttribStr(attribute) << ", " << value << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.getConfigAttrib(dpy, config, attribute, value);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// value = " << getConfigAttribValuePointerStr(attribute, value) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglGetConfigs (eglw::EGLDisplay dpy, eglw::EGLConfig *configs, eglw::EGLint config_size, eglw::EGLint *num_config)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetConfigs(" << dpy << ", " << configs << ", " << config_size << ", " << num_config << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.getConfigs(dpy, configs, config_size, num_config);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLContext CallLogWrapper::eglGetCurrentContext ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentContext(" << ");" << TestLog::EndMessage;
	eglw::EGLContext returnValue = m_egl.getCurrentContext();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLDisplay CallLogWrapper::eglGetCurrentDisplay ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentDisplay(" << ");" << TestLog::EndMessage;
	eglw::EGLDisplay returnValue = m_egl.getCurrentDisplay();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLSurface CallLogWrapper::eglGetCurrentSurface (eglw::EGLint readdraw)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetCurrentSurface(" << getSurfaceTargetStr(readdraw) << ");" << TestLog::EndMessage;
	eglw::EGLSurface returnValue = m_egl.getCurrentSurface(readdraw);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLDisplay CallLogWrapper::eglGetDisplay (eglw::EGLNativeDisplayType display_id)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetDisplay(" << toHex(display_id) << ");" << TestLog::EndMessage;
	eglw::EGLDisplay returnValue = m_egl.getDisplay(display_id);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLint CallLogWrapper::eglGetError ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetError(" << ");" << TestLog::EndMessage;
	eglw::EGLint returnValue = m_egl.getError();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getErrorStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLDisplay CallLogWrapper::eglGetPlatformDisplay (eglw::EGLenum platform, void *native_display, const eglw::EGLAttrib *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetPlatformDisplay(" << toHex(platform) << ", " << native_display << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLDisplay returnValue = m_egl.getPlatformDisplay(platform, native_display, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLDisplay CallLogWrapper::eglGetPlatformDisplayEXT (eglw::EGLenum platform, void *native_display, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetPlatformDisplayEXT(" << toHex(platform) << ", " << native_display << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLDisplay returnValue = m_egl.getPlatformDisplayEXT(platform, native_display, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::__eglMustCastToProperFunctionPointerType CallLogWrapper::eglGetProcAddress (const char *procname)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetProcAddress(" << getStringStr(procname) << ");" << TestLog::EndMessage;
	eglw::__eglMustCastToProperFunctionPointerType returnValue = m_egl.getProcAddress(procname);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << tcu::toHex(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglGetSyncAttrib (eglw::EGLDisplay dpy, eglw::EGLSync sync, eglw::EGLint attribute, eglw::EGLAttrib *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetSyncAttrib(" << dpy << ", " << sync << ", " << attribute << ", " << value << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.getSyncAttrib(dpy, sync, attribute, value);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglGetSyncAttribKHR (eglw::EGLDisplay dpy, eglw::EGLSyncKHR sync, eglw::EGLint attribute, eglw::EGLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglGetSyncAttribKHR(" << dpy << ", " << sync << ", " << attribute << ", " << value << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.getSyncAttribKHR(dpy, sync, attribute, value);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglInitialize (eglw::EGLDisplay dpy, eglw::EGLint *major, eglw::EGLint *minor)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglInitialize(" << dpy << ", " << major << ", " << minor << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.initialize(dpy, major, minor);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglLockSurfaceKHR (eglw::EGLDisplay dpy, eglw::EGLSurface surface, const eglw::EGLint *attrib_list)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglLockSurfaceKHR(" << dpy << ", " << toHex(surface) << ", " << attrib_list << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.lockSurfaceKHR(dpy, surface, attrib_list);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglMakeCurrent (eglw::EGLDisplay dpy, eglw::EGLSurface draw, eglw::EGLSurface read, eglw::EGLContext ctx)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglMakeCurrent(" << dpy << ", " << toHex(draw) << ", " << toHex(read) << ", " << ctx << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.makeCurrent(dpy, draw, read, ctx);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLenum CallLogWrapper::eglQueryAPI ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQueryAPI(" << ");" << TestLog::EndMessage;
	eglw::EGLenum returnValue = m_egl.queryAPI();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getAPIStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglQueryContext (eglw::EGLDisplay dpy, eglw::EGLContext ctx, eglw::EGLint attribute, eglw::EGLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQueryContext(" << dpy << ", " << ctx << ", " << getContextAttribStr(attribute) << ", " << value << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.queryContext(dpy, ctx, attribute, value);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// value = " << getContextAttribValuePointerStr(attribute, value) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

const char * CallLogWrapper::eglQueryString (eglw::EGLDisplay dpy, eglw::EGLint name)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQueryString(" << dpy << ", " << name << ");" << TestLog::EndMessage;
	const char * returnValue = m_egl.queryString(dpy, name);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getStringStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglQuerySurface (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLint attribute, eglw::EGLint *value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglQuerySurface(" << dpy << ", " << toHex(surface) << ", " << getSurfaceAttribStr(attribute) << ", " << value << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.querySurface(dpy, surface, attribute, value);
	if (m_enableLog)
	{
		m_log << TestLog::Message << "// value = " << getSurfaceAttribValuePointerStr(attribute, value) << TestLog::EndMessage;
	}
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglReleaseTexImage (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLint buffer)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglReleaseTexImage(" << dpy << ", " << toHex(surface) << ", " << buffer << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.releaseTexImage(dpy, surface, buffer);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglReleaseThread ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglReleaseThread(" << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.releaseThread();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglSetDamageRegionKHR (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLint *rects, eglw::EGLint n_rects)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSetDamageRegionKHR(" << dpy << ", " << toHex(surface) << ", " << rects << ", " << n_rects << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.setDamageRegionKHR(dpy, surface, rects, n_rects);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglSignalSyncKHR (eglw::EGLDisplay dpy, eglw::EGLSyncKHR sync, eglw::EGLenum mode)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSignalSyncKHR(" << dpy << ", " << sync << ", " << toHex(mode) << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.signalSyncKHR(dpy, sync, mode);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglSurfaceAttrib (eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLint attribute, eglw::EGLint value)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSurfaceAttrib(" << dpy << ", " << toHex(surface) << ", " << getSurfaceAttribStr(attribute) << ", " << getSurfaceAttribValueStr(attribute, value) << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.surfaceAttrib(dpy, surface, attribute, value);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglSwapBuffers (eglw::EGLDisplay dpy, eglw::EGLSurface surface)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSwapBuffers(" << dpy << ", " << toHex(surface) << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.swapBuffers(dpy, surface);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglSwapBuffersWithDamageKHR (eglw::EGLDisplay dpy, eglw::EGLSurface surface, const eglw::EGLint *rects, eglw::EGLint n_rects)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSwapBuffersWithDamageKHR(" << dpy << ", " << toHex(surface) << ", " << rects << ", " << n_rects << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.swapBuffersWithDamageKHR(dpy, surface, rects, n_rects);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglSwapInterval (eglw::EGLDisplay dpy, eglw::EGLint interval)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglSwapInterval(" << dpy << ", " << interval << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.swapInterval(dpy, interval);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglTerminate (eglw::EGLDisplay dpy)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglTerminate(" << dpy << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.terminate(dpy);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglUnlockSurfaceKHR (eglw::EGLDisplay dpy, eglw::EGLSurface surface)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglUnlockSurfaceKHR(" << dpy << ", " << toHex(surface) << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.unlockSurfaceKHR(dpy, surface);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglWaitClient ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitClient(" << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.waitClient();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglWaitGL ()
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitGL(" << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.waitGL();
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglWaitNative (eglw::EGLint engine)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitNative(" << engine << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.waitNative(engine);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLBoolean CallLogWrapper::eglWaitSync (eglw::EGLDisplay dpy, eglw::EGLSync sync, eglw::EGLint flags)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitSync(" << dpy << ", " << sync << ", " << flags << ");" << TestLog::EndMessage;
	eglw::EGLBoolean returnValue = m_egl.waitSync(dpy, sync, flags);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << getBooleanStr(returnValue) << " returned" << TestLog::EndMessage;
	return returnValue;
}

eglw::EGLint CallLogWrapper::eglWaitSyncKHR (eglw::EGLDisplay dpy, eglw::EGLSyncKHR sync, eglw::EGLint flags)
{
	if (m_enableLog)
		m_log << TestLog::Message << "eglWaitSyncKHR(" << dpy << ", " << sync << ", " << flags << ");" << TestLog::EndMessage;
	eglw::EGLint returnValue = m_egl.waitSyncKHR(dpy, sync, flags);
	if (m_enableLog)
		m_log << TestLog::Message << "// " << returnValue << " returned" << TestLog::EndMessage;
	return returnValue;
}
