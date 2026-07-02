/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos EGL API description (egl.xml) revision 682c662d48fbae076c5ed89a1bd5b2aa7e2e4449.
 */
#define										eglBindAPI							eglwBindAPI
#define										eglBindTexImage						eglwBindTexImage
#define										eglChooseConfig						eglwChooseConfig
#define										eglClientWaitSync					eglwClientWaitSync
#define										eglClientWaitSyncKHR				eglwClientWaitSyncKHR
#define										eglCopyBuffers						eglwCopyBuffers
#define										eglCreateContext					eglwCreateContext
#define										eglCreateImage						eglwCreateImage
#define										eglCreateImageKHR					eglwCreateImageKHR
#define										eglCreatePbufferFromClientBuffer	eglwCreatePbufferFromClientBuffer
#define										eglCreatePbufferSurface				eglwCreatePbufferSurface
#define										eglCreatePixmapSurface				eglwCreatePixmapSurface
#define										eglCreatePlatformPixmapSurface		eglwCreatePlatformPixmapSurface
#define										eglCreatePlatformPixmapSurfaceEXT	eglwCreatePlatformPixmapSurfaceEXT
#define										eglCreatePlatformWindowSurface		eglwCreatePlatformWindowSurface
#define										eglCreatePlatformWindowSurfaceEXT	eglwCreatePlatformWindowSurfaceEXT
#define										eglCreateSync						eglwCreateSync
#define										eglCreateSyncKHR					eglwCreateSyncKHR
#define										eglCreateWindowSurface				eglwCreateWindowSurface
#define										eglDestroyContext					eglwDestroyContext
#define										eglDestroyImage						eglwDestroyImage
#define										eglDestroyImageKHR					eglwDestroyImageKHR
#define										eglDestroySurface					eglwDestroySurface
#define										eglDestroySync						eglwDestroySync
#define										eglDestroySyncKHR					eglwDestroySyncKHR
#define										eglGetConfigAttrib					eglwGetConfigAttrib
#define										eglGetConfigs						eglwGetConfigs
#define										eglGetCurrentContext				eglwGetCurrentContext
#define										eglGetCurrentDisplay				eglwGetCurrentDisplay
#define										eglGetCurrentSurface				eglwGetCurrentSurface
#define										eglGetDisplay						eglwGetDisplay
#define										eglGetError							eglwGetError
#define										eglGetPlatformDisplay				eglwGetPlatformDisplay
#define										eglGetPlatformDisplayEXT			eglwGetPlatformDisplayEXT
#define										eglGetProcAddress					eglwGetProcAddress
#define										eglGetSyncAttrib					eglwGetSyncAttrib
#define										eglGetSyncAttribKHR					eglwGetSyncAttribKHR
#define										eglInitialize						eglwInitialize
#define										eglLockSurfaceKHR					eglwLockSurfaceKHR
#define										eglMakeCurrent						eglwMakeCurrent
#define										eglQueryAPI							eglwQueryAPI
#define										eglQueryContext						eglwQueryContext
#define										eglQueryString						eglwQueryString
#define										eglQuerySurface						eglwQuerySurface
#define										eglReleaseTexImage					eglwReleaseTexImage
#define										eglReleaseThread					eglwReleaseThread
#define										eglSetDamageRegionKHR				eglwSetDamageRegionKHR
#define										eglSignalSyncKHR					eglwSignalSyncKHR
#define										eglSurfaceAttrib					eglwSurfaceAttrib
#define										eglSwapBuffers						eglwSwapBuffers
#define										eglSwapBuffersWithDamageKHR			eglwSwapBuffersWithDamageKHR
#define										eglSwapInterval						eglwSwapInterval
#define										eglTerminate						eglwTerminate
#define										eglUnlockSurfaceKHR					eglwUnlockSurfaceKHR
#define										eglWaitClient						eglwWaitClient
#define										eglWaitGL							eglwWaitGL
#define										eglWaitNative						eglwWaitNative
#define										eglWaitSync							eglwWaitSync
#define										eglWaitSyncKHR						eglwWaitSyncKHR
EGLBoolean									eglwBindAPI							(EGLenum api);
EGLBoolean									eglwBindTexImage					(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
EGLBoolean									eglwChooseConfig					(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLint										eglwClientWaitSync					(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout);
EGLint										eglwClientWaitSyncKHR				(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);
EGLBoolean									eglwCopyBuffers						(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target);
EGLContext									eglwCreateContext					(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
EGLImage									eglwCreateImage						(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list);
EGLImageKHR									eglwCreateImageKHR					(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
EGLSurface									eglwCreatePbufferFromClientBuffer	(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list);
EGLSurface									eglwCreatePbufferSurface			(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
EGLSurface									eglwCreatePixmapSurface				(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list);
EGLSurface									eglwCreatePlatformPixmapSurface		(EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list);
EGLSurface									eglwCreatePlatformPixmapSurfaceEXT	(EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLint *attrib_list);
EGLSurface									eglwCreatePlatformWindowSurface		(EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list);
EGLSurface									eglwCreatePlatformWindowSurfaceEXT	(EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list);
EGLSync										eglwCreateSync						(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list);
EGLSyncKHR									eglwCreateSyncKHR					(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
EGLSurface									eglwCreateWindowSurface				(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
EGLBoolean									eglwDestroyContext					(EGLDisplay dpy, EGLContext ctx);
EGLBoolean									eglwDestroyImage					(EGLDisplay dpy, EGLImage image);
EGLBoolean									eglwDestroyImageKHR					(EGLDisplay dpy, EGLImageKHR image);
EGLBoolean									eglwDestroySurface					(EGLDisplay dpy, EGLSurface surface);
EGLBoolean									eglwDestroySync						(EGLDisplay dpy, EGLSync sync);
EGLBoolean									eglwDestroySyncKHR					(EGLDisplay dpy, EGLSyncKHR sync);
EGLBoolean									eglwGetConfigAttrib					(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
EGLBoolean									eglwGetConfigs						(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLContext									eglwGetCurrentContext				();
EGLDisplay									eglwGetCurrentDisplay				();
EGLSurface									eglwGetCurrentSurface				(EGLint readdraw);
EGLDisplay									eglwGetDisplay						(EGLNativeDisplayType display_id);
EGLint										eglwGetError						();
EGLDisplay									eglwGetPlatformDisplay				(EGLenum platform, void *native_display, const EGLAttrib *attrib_list);
EGLDisplay									eglwGetPlatformDisplayEXT			(EGLenum platform, void *native_display, const EGLint *attrib_list);
__eglMustCastToProperFunctionPointerType	eglwGetProcAddress					(const char *procname);
EGLBoolean									eglwGetSyncAttrib					(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value);
EGLBoolean									eglwGetSyncAttribKHR				(EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value);
EGLBoolean									eglwInitialize						(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLBoolean									eglwLockSurfaceKHR					(EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list);
EGLBoolean									eglwMakeCurrent						(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
EGLenum										eglwQueryAPI						();
EGLBoolean									eglwQueryContext					(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);
const char *								eglwQueryString						(EGLDisplay dpy, EGLint name);
EGLBoolean									eglwQuerySurface					(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);
EGLBoolean									eglwReleaseTexImage					(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
EGLBoolean									eglwReleaseThread					();
EGLBoolean									eglwSetDamageRegionKHR				(EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects);
EGLBoolean									eglwSignalSyncKHR					(EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode);
EGLBoolean									eglwSurfaceAttrib					(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
EGLBoolean									eglwSwapBuffers						(EGLDisplay dpy, EGLSurface surface);
EGLBoolean									eglwSwapBuffersWithDamageKHR		(EGLDisplay dpy, EGLSurface surface, const EGLint *rects, EGLint n_rects);
EGLBoolean									eglwSwapInterval					(EGLDisplay dpy, EGLint interval);
EGLBoolean									eglwTerminate						(EGLDisplay dpy);
EGLBoolean									eglwUnlockSurfaceKHR				(EGLDisplay dpy, EGLSurface surface);
EGLBoolean									eglwWaitClient						();
EGLBoolean									eglwWaitGL							();
EGLBoolean									eglwWaitNative						(EGLint engine);
EGLBoolean									eglwWaitSync						(EGLDisplay dpy, EGLSync sync, EGLint flags);
EGLint										eglwWaitSyncKHR						(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);
