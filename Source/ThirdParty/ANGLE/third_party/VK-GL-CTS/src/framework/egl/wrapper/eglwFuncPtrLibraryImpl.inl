/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos EGL API description (egl.xml) revision 682c662d48fbae076c5ed89a1bd5b2aa7e2e4449.
 */

EGLBoolean FuncPtrLibrary::bindAPI (EGLenum api) const
{
    return m_egl.bindAPI(api);
}

EGLBoolean FuncPtrLibrary::bindTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer) const
{
    return m_egl.bindTexImage(dpy, surface, buffer);
}

EGLBoolean FuncPtrLibrary::chooseConfig (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config) const
{
    return m_egl.chooseConfig(dpy, attrib_list, configs, config_size, num_config);
}

EGLint FuncPtrLibrary::clientWaitSync (EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout) const
{
    return m_egl.clientWaitSync(dpy, sync, flags, timeout);
}

EGLint FuncPtrLibrary::clientWaitSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout) const
{
    return m_egl.clientWaitSyncKHR(dpy, sync, flags, timeout);
}

EGLBoolean FuncPtrLibrary::copyBuffers (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) const
{
    return m_egl.copyBuffers(dpy, surface, target);
}

EGLContext FuncPtrLibrary::createContext (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list) const
{
    return m_egl.createContext(dpy, config, share_context, attrib_list);
}

EGLImage FuncPtrLibrary::createImage (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list) const
{
    return m_egl.createImage(dpy, ctx, target, buffer, attrib_list);
}

EGLImageKHR FuncPtrLibrary::createImageKHR (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list) const
{
    return m_egl.createImageKHR(dpy, ctx, target, buffer, attrib_list);
}

EGLSurface FuncPtrLibrary::createPbufferFromClientBuffer (EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list) const
{
    return m_egl.createPbufferFromClientBuffer(dpy, buftype, buffer, config, attrib_list);
}

EGLSurface FuncPtrLibrary::createPbufferSurface (EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) const
{
    return m_egl.createPbufferSurface(dpy, config, attrib_list);
}

EGLSurface FuncPtrLibrary::createPixmapSurface (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list) const
{
    return m_egl.createPixmapSurface(dpy, config, pixmap, attrib_list);
}

EGLSurface FuncPtrLibrary::createPlatformPixmapSurface (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list) const
{
    return m_egl.createPlatformPixmapSurface(dpy, config, native_pixmap, attrib_list);
}

EGLSurface FuncPtrLibrary::createPlatformPixmapSurfaceEXT (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLint *attrib_list) const
{
    return m_egl.createPlatformPixmapSurfaceEXT(dpy, config, native_pixmap, attrib_list);
}

EGLSurface FuncPtrLibrary::createPlatformWindowSurface (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list) const
{
    return m_egl.createPlatformWindowSurface(dpy, config, native_window, attrib_list);
}

EGLSurface FuncPtrLibrary::createPlatformWindowSurfaceEXT (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list) const
{
    return m_egl.createPlatformWindowSurfaceEXT(dpy, config, native_window, attrib_list);
}

EGLSync FuncPtrLibrary::createSync (EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list) const
{
    return m_egl.createSync(dpy, type, attrib_list);
}

EGLSyncKHR FuncPtrLibrary::createSyncKHR (EGLDisplay dpy, EGLenum type, const EGLint *attrib_list) const
{
    return m_egl.createSyncKHR(dpy, type, attrib_list);
}

EGLSurface FuncPtrLibrary::createWindowSurface (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) const
{
    return m_egl.createWindowSurface(dpy, config, win, attrib_list);
}

EGLBoolean FuncPtrLibrary::destroyContext (EGLDisplay dpy, EGLContext ctx) const
{
    return m_egl.destroyContext(dpy, ctx);
}

EGLBoolean FuncPtrLibrary::destroyImage (EGLDisplay dpy, EGLImage image) const
{
    return m_egl.destroyImage(dpy, image);
}

EGLBoolean FuncPtrLibrary::destroyImageKHR (EGLDisplay dpy, EGLImageKHR image) const
{
    return m_egl.destroyImageKHR(dpy, image);
}

EGLBoolean FuncPtrLibrary::destroySurface (EGLDisplay dpy, EGLSurface surface) const
{
    return m_egl.destroySurface(dpy, surface);
}

EGLBoolean FuncPtrLibrary::destroySync (EGLDisplay dpy, EGLSync sync) const
{
    return m_egl.destroySync(dpy, sync);
}

EGLBoolean FuncPtrLibrary::destroySyncKHR (EGLDisplay dpy, EGLSyncKHR sync) const
{
    return m_egl.destroySyncKHR(dpy, sync);
}

EGLBoolean FuncPtrLibrary::getConfigAttrib (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value) const
{
    return m_egl.getConfigAttrib(dpy, config, attribute, value);
}

EGLBoolean FuncPtrLibrary::getConfigs (EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config) const
{
    return m_egl.getConfigs(dpy, configs, config_size, num_config);
}

EGLContext FuncPtrLibrary::getCurrentContext (void) const
{
    return m_egl.getCurrentContext();
}

EGLDisplay FuncPtrLibrary::getCurrentDisplay (void) const
{
    return m_egl.getCurrentDisplay();
}

EGLSurface FuncPtrLibrary::getCurrentSurface (EGLint readdraw) const
{
    return m_egl.getCurrentSurface(readdraw);
}

EGLDisplay FuncPtrLibrary::getDisplay (EGLNativeDisplayType display_id) const
{
    return m_egl.getDisplay(display_id);
}

EGLint FuncPtrLibrary::getError (void) const
{
    return m_egl.getError();
}

EGLDisplay FuncPtrLibrary::getPlatformDisplay (EGLenum platform, void *native_display, const EGLAttrib *attrib_list) const
{
    return m_egl.getPlatformDisplay(platform, native_display, attrib_list);
}

EGLDisplay FuncPtrLibrary::getPlatformDisplayEXT (EGLenum platform, void *native_display, const EGLint *attrib_list) const
{
    return m_egl.getPlatformDisplayEXT(platform, native_display, attrib_list);
}

__eglMustCastToProperFunctionPointerType FuncPtrLibrary::getProcAddress (const char *procname) const
{
    return m_egl.getProcAddress(procname);
}

EGLBoolean FuncPtrLibrary::getSyncAttrib (EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value) const
{
    return m_egl.getSyncAttrib(dpy, sync, attribute, value);
}

EGLBoolean FuncPtrLibrary::getSyncAttribKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value) const
{
    return m_egl.getSyncAttribKHR(dpy, sync, attribute, value);
}

EGLBoolean FuncPtrLibrary::initialize (EGLDisplay dpy, EGLint *major, EGLint *minor) const
{
    return m_egl.initialize(dpy, major, minor);
}

EGLBoolean FuncPtrLibrary::lockSurfaceKHR (EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list) const
{
    return m_egl.lockSurfaceKHR(dpy, surface, attrib_list);
}

EGLBoolean FuncPtrLibrary::makeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) const
{
    return m_egl.makeCurrent(dpy, draw, read, ctx);
}

EGLenum FuncPtrLibrary::queryAPI (void) const
{
    return m_egl.queryAPI();
}

EGLBoolean FuncPtrLibrary::queryContext (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value) const
{
    return m_egl.queryContext(dpy, ctx, attribute, value);
}

const char * FuncPtrLibrary::queryString (EGLDisplay dpy, EGLint name) const
{
    return m_egl.queryString(dpy, name);
}

EGLBoolean FuncPtrLibrary::querySurface (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value) const
{
    return m_egl.querySurface(dpy, surface, attribute, value);
}

EGLBoolean FuncPtrLibrary::releaseTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer) const
{
    return m_egl.releaseTexImage(dpy, surface, buffer);
}

EGLBoolean FuncPtrLibrary::releaseThread (void) const
{
    return m_egl.releaseThread();
}

EGLBoolean FuncPtrLibrary::setDamageRegionKHR (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects) const
{
    return m_egl.setDamageRegionKHR(dpy, surface, rects, n_rects);
}

EGLBoolean FuncPtrLibrary::signalSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode) const
{
    return m_egl.signalSyncKHR(dpy, sync, mode);
}

EGLBoolean FuncPtrLibrary::surfaceAttrib (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) const
{
    return m_egl.surfaceAttrib(dpy, surface, attribute, value);
}

EGLBoolean FuncPtrLibrary::swapBuffers (EGLDisplay dpy, EGLSurface surface) const
{
    return m_egl.swapBuffers(dpy, surface);
}

EGLBoolean FuncPtrLibrary::swapBuffersWithDamageKHR (EGLDisplay dpy, EGLSurface surface, const EGLint *rects, EGLint n_rects) const
{
    return m_egl.swapBuffersWithDamageKHR(dpy, surface, rects, n_rects);
}

EGLBoolean FuncPtrLibrary::swapInterval (EGLDisplay dpy, EGLint interval) const
{
    return m_egl.swapInterval(dpy, interval);
}

EGLBoolean FuncPtrLibrary::terminate (EGLDisplay dpy) const
{
    return m_egl.terminate(dpy);
}

EGLBoolean FuncPtrLibrary::unlockSurfaceKHR (EGLDisplay dpy, EGLSurface surface) const
{
    return m_egl.unlockSurfaceKHR(dpy, surface);
}

EGLBoolean FuncPtrLibrary::waitClient (void) const
{
    return m_egl.waitClient();
}

EGLBoolean FuncPtrLibrary::waitGL (void) const
{
    return m_egl.waitGL();
}

EGLBoolean FuncPtrLibrary::waitNative (EGLint engine) const
{
    return m_egl.waitNative(engine);
}

EGLBoolean FuncPtrLibrary::waitSync (EGLDisplay dpy, EGLSync sync, EGLint flags) const
{
    return m_egl.waitSync(dpy, sync, flags);
}

EGLint FuncPtrLibrary::waitSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags) const
{
    return m_egl.waitSyncKHR(dpy, sync, flags);
}
