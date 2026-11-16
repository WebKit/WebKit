/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos EGL API description (egl.xml) revision 682c662d48fbae076c5ed89a1bd5b2aa7e2e4449.
 */

EGLBoolean eglwBindAPI (EGLenum api)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->bindAPI(api);
}

EGLBoolean eglwBindTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->bindTexImage(dpy, surface, buffer);
}

EGLBoolean eglwChooseConfig (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->chooseConfig(dpy, attrib_list, configs, config_size, num_config);
}

EGLint eglwClientWaitSync (EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLint)0;
    return egl->clientWaitSync(dpy, sync, flags, timeout);
}

EGLBoolean eglwCopyBuffers (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->copyBuffers(dpy, surface, (void*)target);
}

EGLContext eglwCreateContext (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLContext)0;
    return egl->createContext(dpy, config, share_context, attrib_list);
}

EGLImage eglwCreateImage (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLImage)0;
    return egl->createImage(dpy, ctx, target, buffer, attrib_list);
}

EGLSurface eglwCreatePbufferFromClientBuffer (EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->createPbufferFromClientBuffer(dpy, buftype, buffer, config, attrib_list);
}

EGLSurface eglwCreatePbufferSurface (EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->createPbufferSurface(dpy, config, attrib_list);
}

EGLSurface eglwCreatePixmapSurface (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->createPixmapSurface(dpy, config, (void*)pixmap, attrib_list);
}

EGLSurface eglwCreatePlatformPixmapSurface (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->createPlatformPixmapSurface(dpy, config, native_pixmap, attrib_list);
}

EGLSurface eglwCreatePlatformWindowSurface (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->createPlatformWindowSurface(dpy, config, native_window, attrib_list);
}

EGLSync eglwCreateSync (EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSync)0;
    return egl->createSync(dpy, type, attrib_list);
}

EGLSurface eglwCreateWindowSurface (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->createWindowSurface(dpy, config, (void*)win, attrib_list);
}

EGLBoolean eglwDestroyContext (EGLDisplay dpy, EGLContext ctx)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->destroyContext(dpy, ctx);
}

EGLBoolean eglwDestroyImage (EGLDisplay dpy, EGLImage image)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->destroyImage(dpy, image);
}

EGLBoolean eglwDestroySurface (EGLDisplay dpy, EGLSurface surface)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->destroySurface(dpy, surface);
}

EGLBoolean eglwDestroySync (EGLDisplay dpy, EGLSync sync)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->destroySync(dpy, sync);
}

EGLBoolean eglwGetConfigAttrib (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->getConfigAttrib(dpy, config, attribute, value);
}

EGLBoolean eglwGetConfigs (EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->getConfigs(dpy, configs, config_size, num_config);
}

EGLContext eglwGetCurrentContext (void)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLContext)0;
    return egl->getCurrentContext();
}

EGLDisplay eglwGetCurrentDisplay (void)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLDisplay)0;
    return egl->getCurrentDisplay();
}

EGLSurface eglwGetCurrentSurface (EGLint readdraw)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->getCurrentSurface(readdraw);
}

EGLDisplay eglwGetDisplay (EGLNativeDisplayType display_id)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLDisplay)0;
    return egl->getDisplay((void*)display_id);
}

EGLint eglwGetError (void)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLint)0;
    return egl->getError();
}

EGLDisplay eglwGetPlatformDisplay (EGLenum platform, void *native_display, const EGLAttrib *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLDisplay)0;
    return egl->getPlatformDisplay(platform, native_display, attrib_list);
}

__eglMustCastToProperFunctionPointerType eglwGetProcAddress (const char *procname)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (__eglMustCastToProperFunctionPointerType)0;
    return egl->getProcAddress(procname);
}

EGLBoolean eglwGetSyncAttrib (EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->getSyncAttrib(dpy, sync, attribute, value);
}

EGLBoolean eglwInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->initialize(dpy, major, minor);
}

EGLBoolean eglwMakeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->makeCurrent(dpy, draw, read, ctx);
}

EGLenum eglwQueryAPI (void)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLenum)0;
    return egl->queryAPI();
}

EGLBoolean eglwQueryContext (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->queryContext(dpy, ctx, attribute, value);
}

const char * eglwQueryString (EGLDisplay dpy, EGLint name)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (const char *)0;
    return egl->queryString(dpy, name);
}

EGLBoolean eglwQuerySurface (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->querySurface(dpy, surface, attribute, value);
}

EGLBoolean eglwReleaseTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->releaseTexImage(dpy, surface, buffer);
}

EGLBoolean eglwReleaseThread (void)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->releaseThread();
}

EGLBoolean eglwSurfaceAttrib (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->surfaceAttrib(dpy, surface, attribute, value);
}

EGLBoolean eglwSwapBuffers (EGLDisplay dpy, EGLSurface surface)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->swapBuffers(dpy, surface);
}

EGLBoolean eglwSwapInterval (EGLDisplay dpy, EGLint interval)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->swapInterval(dpy, interval);
}

EGLBoolean eglwTerminate (EGLDisplay dpy)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->terminate(dpy);
}

EGLBoolean eglwWaitClient (void)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->waitClient();
}

EGLBoolean eglwWaitGL (void)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->waitGL();
}

EGLBoolean eglwWaitNative (EGLint engine)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->waitNative(engine);
}

EGLBoolean eglwWaitSync (EGLDisplay dpy, EGLSync sync, EGLint flags)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->waitSync(dpy, sync, flags);
}
