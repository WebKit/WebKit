/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos EGL API description (egl.xml) revision 682c662d48fbae076c5ed89a1bd5b2aa7e2e4449.
 */

EGLint eglwClientWaitSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLint)0;
    return egl->clientWaitSyncKHR(dpy, sync, flags, timeout);
}

EGLImageKHR eglwCreateImageKHR (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLImageKHR)0;
    return egl->createImageKHR(dpy, ctx, target, buffer, attrib_list);
}

EGLSurface eglwCreatePlatformPixmapSurfaceEXT (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->createPlatformPixmapSurfaceEXT(dpy, config, native_pixmap, attrib_list);
}

EGLSurface eglwCreatePlatformWindowSurfaceEXT (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSurface)0;
    return egl->createPlatformWindowSurfaceEXT(dpy, config, native_window, attrib_list);
}

EGLSyncKHR eglwCreateSyncKHR (EGLDisplay dpy, EGLenum type, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLSyncKHR)0;
    return egl->createSyncKHR(dpy, type, attrib_list);
}

EGLBoolean eglwDestroyImageKHR (EGLDisplay dpy, EGLImageKHR image)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->destroyImageKHR(dpy, image);
}

EGLBoolean eglwDestroySyncKHR (EGLDisplay dpy, EGLSyncKHR sync)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->destroySyncKHR(dpy, sync);
}

EGLDisplay eglwGetPlatformDisplayEXT (EGLenum platform, void *native_display, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLDisplay)0;
    return egl->getPlatformDisplayEXT(platform, native_display, attrib_list);
}

EGLBoolean eglwGetSyncAttribKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->getSyncAttribKHR(dpy, sync, attribute, value);
}

EGLBoolean eglwLockSurfaceKHR (EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->lockSurfaceKHR(dpy, surface, attrib_list);
}

EGLBoolean eglwSetDamageRegionKHR (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->setDamageRegionKHR(dpy, surface, rects, n_rects);
}

EGLBoolean eglwSignalSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->signalSyncKHR(dpy, sync, mode);
}

EGLBoolean eglwSwapBuffersWithDamageKHR (EGLDisplay dpy, EGLSurface surface, const EGLint *rects, EGLint n_rects)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->swapBuffersWithDamageKHR(dpy, surface, rects, n_rects);
}

EGLBoolean eglwUnlockSurfaceKHR (EGLDisplay dpy, EGLSurface surface)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLBoolean)0;
    return egl->unlockSurfaceKHR(dpy, surface);
}

EGLint eglwWaitSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags)
{
    const eglw::Library* egl = eglw::getCurrentThreadLibrary();
    if (!egl)
        return (EGLint)0;
    return egl->waitSyncKHR(dpy, sync, flags);
}
