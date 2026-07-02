/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision 7d0cb181dace7461bc4f26650fec71efeacc18a5.
 */

if (de::contains(extSet, "GL_KHR_blend_equation_advanced"))
{
	gl->blendBarrier	= (glBlendBarrierFunc)	loader->get("glBlendBarrierKHR");
}

if (de::contains(extSet, "GL_KHR_debug"))
{
	gl->debugMessageCallback	= (glDebugMessageCallbackFunc)	loader->get("glDebugMessageCallbackKHR");
	gl->debugMessageControl		= (glDebugMessageControlFunc)	loader->get("glDebugMessageControlKHR");
	gl->debugMessageInsert		= (glDebugMessageInsertFunc)	loader->get("glDebugMessageInsertKHR");
	gl->getDebugMessageLog		= (glGetDebugMessageLogFunc)	loader->get("glGetDebugMessageLogKHR");
	gl->getObjectLabel			= (glGetObjectLabelFunc)		loader->get("glGetObjectLabelKHR");
	gl->getObjectPtrLabel		= (glGetObjectPtrLabelFunc)		loader->get("glGetObjectPtrLabelKHR");
	gl->getPointerv				= (glGetPointervFunc)			loader->get("glGetPointervKHR");
	gl->objectLabel				= (glObjectLabelFunc)			loader->get("glObjectLabelKHR");
	gl->objectPtrLabel			= (glObjectPtrLabelFunc)		loader->get("glObjectPtrLabelKHR");
	gl->popDebugGroup			= (glPopDebugGroupFunc)			loader->get("glPopDebugGroupKHR");
	gl->pushDebugGroup			= (glPushDebugGroupFunc)		loader->get("glPushDebugGroupKHR");
}

if (de::contains(extSet, "GL_EXT_robustness"))
{
	gl->getGraphicsResetStatus	= (glGetGraphicsResetStatusFunc)	loader->get("glGetGraphicsResetStatusEXT");
	gl->getnUniformfv			= (glGetnUniformfvFunc)				loader->get("glGetnUniformfvEXT");
	gl->getnUniformiv			= (glGetnUniformivFunc)				loader->get("glGetnUniformivEXT");
	gl->readnPixels				= (glReadnPixelsFunc)				loader->get("glReadnPixelsEXT");
}

if (de::contains(extSet, "GL_KHR_robustness"))
{
	gl->getGraphicsResetStatus	= (glGetGraphicsResetStatusFunc)	loader->get("glGetGraphicsResetStatusKHR");
	gl->getnUniformfv			= (glGetnUniformfvFunc)				loader->get("glGetnUniformfvKHR");
	gl->getnUniformiv			= (glGetnUniformivFunc)				loader->get("glGetnUniformivKHR");
	gl->getnUniformuiv			= (glGetnUniformuivFunc)			loader->get("glGetnUniformuivKHR");
	gl->readnPixels				= (glReadnPixelsFunc)				loader->get("glReadnPixelsKHR");
}

if (de::contains(extSet, "GL_KHR_parallel_shader_compile"))
{
	gl->maxShaderCompilerThreadsKHR	= (glMaxShaderCompilerThreadsKHRFunc)	loader->get("glMaxShaderCompilerThreadsKHR");
}

if (de::contains(extSet, "GL_EXT_tessellation_shader"))
{
	gl->patchParameteri	= (glPatchParameteriFunc)	loader->get("glPatchParameteriEXT");
}

if (de::contains(extSet, "GL_EXT_geometry_shader"))
{
	gl->framebufferTexture	= (glFramebufferTextureFunc)	loader->get("glFramebufferTextureEXT");
}

if (de::contains(extSet, "GL_EXT_texture_buffer"))
{
	gl->texBuffer		= (glTexBufferFunc)			loader->get("glTexBufferEXT");
	gl->texBufferRange	= (glTexBufferRangeFunc)	loader->get("glTexBufferRangeEXT");
}

if (de::contains(extSet, "GL_EXT_primitive_bounding_box"))
{
	gl->primitiveBoundingBox	= (glPrimitiveBoundingBoxFunc)	loader->get("glPrimitiveBoundingBoxEXT");
}

if (de::contains(extSet, "GL_EXT_clip_control"))
{
	gl->clipControl	= (glClipControlFunc)	loader->get("glClipControlEXT");
}

if (de::contains(extSet, "GL_EXT_copy_image"))
{
	gl->copyImageSubData	= (glCopyImageSubDataFunc)	loader->get("glCopyImageSubDataEXT");
}

if (de::contains(extSet, "GL_EXT_draw_buffers_indexed"))
{
	gl->blendEquationSeparatei	= (glBlendEquationSeparateiFunc)	loader->get("glBlendEquationSeparateiEXT");
	gl->blendEquationi			= (glBlendEquationiFunc)			loader->get("glBlendEquationiEXT");
	gl->blendFuncSeparatei		= (glBlendFuncSeparateiFunc)		loader->get("glBlendFuncSeparateiEXT");
	gl->blendFunci				= (glBlendFunciFunc)				loader->get("glBlendFunciEXT");
	gl->colorMaski				= (glColorMaskiFunc)				loader->get("glColorMaskiEXT");
	gl->disablei				= (glDisableiFunc)					loader->get("glDisableiEXT");
	gl->enablei					= (glEnableiFunc)					loader->get("glEnableiEXT");
	gl->isEnabledi				= (glIsEnablediFunc)				loader->get("glIsEnablediEXT");
}

if (de::contains(extSet, "GL_EXT_draw_elements_base_vertex"))
{
	gl->drawElementsBaseVertex			= (glDrawElementsBaseVertexFunc)			loader->get("glDrawElementsBaseVertexEXT");
	gl->drawElementsInstancedBaseVertex	= (glDrawElementsInstancedBaseVertexFunc)	loader->get("glDrawElementsInstancedBaseVertexEXT");
	gl->drawRangeElementsBaseVertex		= (glDrawRangeElementsBaseVertexFunc)		loader->get("glDrawRangeElementsBaseVertexEXT");
	gl->multiDrawElementsBaseVertex		= (glMultiDrawElementsBaseVertexFunc)		loader->get("glMultiDrawElementsBaseVertexEXT");
}

if (de::contains(extSet, "GL_EXT_texture_storage"))
{
	gl->texStorage1D		= (glTexStorage1DFunc)			loader->get("glTexStorage1DEXT");
	gl->texStorage2D		= (glTexStorage2DFunc)			loader->get("glTexStorage2DEXT");
	gl->texStorage3D		= (glTexStorage3DFunc)			loader->get("glTexStorage3DEXT");
	gl->textureStorage1DEXT	= (glTextureStorage1DEXTFunc)	loader->get("glTextureStorage1DEXT");
	gl->textureStorage2DEXT	= (glTextureStorage2DEXTFunc)	loader->get("glTextureStorage2DEXT");
	gl->textureStorage3DEXT	= (glTextureStorage3DEXTFunc)	loader->get("glTextureStorage3DEXT");
}

if (de::contains(extSet, "GL_EXT_texture_border_clamp"))
{
	gl->getSamplerParameterIiv	= (glGetSamplerParameterIivFunc)	loader->get("glGetSamplerParameterIivEXT");
	gl->getSamplerParameterIuiv	= (glGetSamplerParameterIuivFunc)	loader->get("glGetSamplerParameterIuivEXT");
	gl->getTexParameterIiv		= (glGetTexParameterIivFunc)		loader->get("glGetTexParameterIivEXT");
	gl->getTexParameterIuiv		= (glGetTexParameterIuivFunc)		loader->get("glGetTexParameterIuivEXT");
	gl->samplerParameterIiv		= (glSamplerParameterIivFunc)		loader->get("glSamplerParameterIivEXT");
	gl->samplerParameterIuiv	= (glSamplerParameterIuivFunc)		loader->get("glSamplerParameterIuivEXT");
	gl->texParameterIiv			= (glTexParameterIivFunc)			loader->get("glTexParameterIivEXT");
	gl->texParameterIuiv		= (glTexParameterIuivFunc)			loader->get("glTexParameterIuivEXT");
}

if (de::contains(extSet, "GL_EXT_multisampled_render_to_texture"))
{
	gl->framebufferTexture2DMultisampleEXT	= (glFramebufferTexture2DMultisampleEXTFunc)	loader->get("glFramebufferTexture2DMultisampleEXT");
	gl->renderbufferStorageMultisampleEXT	= (glRenderbufferStorageMultisampleEXTFunc)		loader->get("glRenderbufferStorageMultisampleEXT");
}

if (de::contains(extSet, "GL_EXT_debug_marker"))
{
	gl->insertEventMarkerEXT	= (glInsertEventMarkerEXTFunc)	loader->get("glInsertEventMarkerEXT");
	gl->popGroupMarkerEXT		= (glPopGroupMarkerEXTFunc)		loader->get("glPopGroupMarkerEXT");
	gl->pushGroupMarkerEXT		= (glPushGroupMarkerEXTFunc)	loader->get("glPushGroupMarkerEXT");
}

if (de::contains(extSet, "GL_EXT_polygon_offset_clamp"))
{
	gl->polygonOffsetClamp	= (glPolygonOffsetClampFunc)	loader->get("glPolygonOffsetClampEXT");
}

if (de::contains(extSet, "GL_EXT_fragment_shading_rate"))
{
	gl->framebufferShadingRateEXT	= (glFramebufferShadingRateEXTFunc)		loader->get("glFramebufferShadingRateEXT");
	gl->getFragmentShadingRatesEXT	= (glGetFragmentShadingRatesEXTFunc)	loader->get("glGetFragmentShadingRatesEXT");
	gl->shadingRateEXT				= (glShadingRateEXTFunc)				loader->get("glShadingRateEXT");
	gl->shadingRateCombinerOpsEXT	= (glShadingRateCombinerOpsEXTFunc)		loader->get("glShadingRateCombinerOpsEXT");
}

if (de::contains(extSet, "GL_OES_EGL_image"))
{
	gl->eglImageTargetRenderbufferStorageOES	= (glEGLImageTargetRenderbufferStorageOESFunc)	loader->get("glEGLImageTargetRenderbufferStorageOES");
	gl->eglImageTargetTexture2DOES				= (glEGLImageTargetTexture2DOESFunc)			loader->get("glEGLImageTargetTexture2DOES");
}

if (de::contains(extSet, "GL_OES_EGL_image_external"))
{
	gl->eglImageTargetTexture2DOES	= (glEGLImageTargetTexture2DOESFunc)	loader->get("glEGLImageTargetTexture2DOES");
}

if (de::contains(extSet, "GL_OES_texture_3D"))
{
	gl->compressedTexImage3DOES		= (glCompressedTexImage3DOESFunc)		loader->get("glCompressedTexImage3DOES");
	gl->compressedTexSubImage3DOES	= (glCompressedTexSubImage3DOESFunc)	loader->get("glCompressedTexSubImage3DOES");
	gl->copyTexSubImage3DOES		= (glCopyTexSubImage3DOESFunc)			loader->get("glCopyTexSubImage3DOES");
	gl->framebufferTexture3DOES		= (glFramebufferTexture3DOESFunc)		loader->get("glFramebufferTexture3DOES");
	gl->texImage3DOES				= (glTexImage3DOESFunc)					loader->get("glTexImage3DOES");
	gl->texSubImage3DOES			= (glTexSubImage3DOESFunc)				loader->get("glTexSubImage3DOES");
}

if (de::contains(extSet, "GL_OES_texture_storage_multisample_2d_array"))
{
	gl->texStorage3DMultisample	= (glTexStorage3DMultisampleFunc)	loader->get("glTexStorage3DMultisampleOES");
}

if (de::contains(extSet, "GL_OES_sample_shading"))
{
	gl->minSampleShading	= (glMinSampleShadingFunc)	loader->get("glMinSampleShadingOES");
}

if (de::contains(extSet, "GL_OES_mapbuffer"))
{
	gl->getBufferPointerv	= (glGetBufferPointervFunc)	loader->get("glGetBufferPointervOES");
	gl->mapBuffer			= (glMapBufferFunc)			loader->get("glMapBufferOES");
	gl->unmapBuffer			= (glUnmapBufferFunc)		loader->get("glUnmapBufferOES");
}

if (de::contains(extSet, "GL_OES_vertex_array_object"))
{
	gl->bindVertexArray		= (glBindVertexArrayFunc)		loader->get("glBindVertexArrayOES");
	gl->deleteVertexArrays	= (glDeleteVertexArraysFunc)	loader->get("glDeleteVertexArraysOES");
	gl->genVertexArrays		= (glGenVertexArraysFunc)		loader->get("glGenVertexArraysOES");
	gl->isVertexArray		= (glIsVertexArrayFunc)			loader->get("glIsVertexArrayOES");
}

if (de::contains(extSet, "GL_OES_viewport_array"))
{
	gl->depthRangeArrayfvOES	= (glDepthRangeArrayfvOESFunc)	loader->get("glDepthRangeArrayfvOES");
	gl->depthRangeIndexedfOES	= (glDepthRangeIndexedfOESFunc)	loader->get("glDepthRangeIndexedfOES");
	gl->disablei				= (glDisableiFunc)				loader->get("glDisableiOES");
	gl->enablei					= (glEnableiFunc)				loader->get("glEnableiOES");
	gl->getFloati_v				= (glGetFloati_vFunc)			loader->get("glGetFloati_vOES");
	gl->isEnabledi				= (glIsEnablediFunc)			loader->get("glIsEnablediOES");
	gl->scissorArrayv			= (glScissorArrayvFunc)			loader->get("glScissorArrayvOES");
	gl->scissorIndexed			= (glScissorIndexedFunc)		loader->get("glScissorIndexedOES");
	gl->scissorIndexedv			= (glScissorIndexedvFunc)		loader->get("glScissorIndexedvOES");
	gl->viewportArrayv			= (glViewportArrayvFunc)		loader->get("glViewportArrayvOES");
	gl->viewportIndexedf		= (glViewportIndexedfFunc)		loader->get("glViewportIndexedfOES");
	gl->viewportIndexedfv		= (glViewportIndexedfvFunc)		loader->get("glViewportIndexedfvOES");
}

if (de::contains(extSet, "GL_NV_internalformat_sample_query"))
{
	gl->getInternalformatSampleivNV	= (glGetInternalformatSampleivNVFunc)	loader->get("glGetInternalformatSampleivNV");
}

if (de::contains(extSet, "GL_OES_draw_elements_base_vertex"))
{
	gl->drawElementsBaseVertex			= (glDrawElementsBaseVertexFunc)			loader->get("glDrawElementsBaseVertexOES");
	gl->drawElementsInstancedBaseVertex	= (glDrawElementsInstancedBaseVertexFunc)	loader->get("glDrawElementsInstancedBaseVertexOES");
	gl->drawRangeElementsBaseVertex		= (glDrawRangeElementsBaseVertexFunc)		loader->get("glDrawRangeElementsBaseVertexOES");
	gl->multiDrawElementsBaseVertex		= (glMultiDrawElementsBaseVertexFunc)		loader->get("glMultiDrawElementsBaseVertexEXT");
}

if (de::contains(extSet, "GL_OVR_multiview"))
{
	gl->framebufferTextureMultiviewOVR		= (glFramebufferTextureMultiviewOVRFunc)		loader->get("glFramebufferTextureMultiviewOVR");
	gl->namedFramebufferTextureMultiviewOVR	= (glNamedFramebufferTextureMultiviewOVRFunc)	loader->get("glNamedFramebufferTextureMultiviewOVR");
}

if (de::contains(extSet, "GL_OVR_multiview_multisampled_render_to_texture"))
{
	gl->framebufferTextureMultisampleMultiviewOVR	= (glFramebufferTextureMultisampleMultiviewOVRFunc)	loader->get("glFramebufferTextureMultisampleMultiviewOVR");
}
