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
	gl->debugMessageCallback	= (glDebugMessageCallbackFunc)	loader->get("glDebugMessageCallback");
	gl->debugMessageControl		= (glDebugMessageControlFunc)	loader->get("glDebugMessageControl");
	gl->debugMessageInsert		= (glDebugMessageInsertFunc)	loader->get("glDebugMessageInsert");
	gl->getDebugMessageLog		= (glGetDebugMessageLogFunc)	loader->get("glGetDebugMessageLog");
	gl->getObjectLabel			= (glGetObjectLabelFunc)		loader->get("glGetObjectLabel");
	gl->getObjectPtrLabel		= (glGetObjectPtrLabelFunc)		loader->get("glGetObjectPtrLabel");
	gl->objectLabel				= (glObjectLabelFunc)			loader->get("glObjectLabel");
	gl->objectPtrLabel			= (glObjectPtrLabelFunc)		loader->get("glObjectPtrLabel");
	gl->popDebugGroup			= (glPopDebugGroupFunc)			loader->get("glPopDebugGroup");
	gl->pushDebugGroup			= (glPushDebugGroupFunc)		loader->get("glPushDebugGroup");
}

if (de::contains(extSet, "GL_KHR_robustness"))
{
	gl->getGraphicsResetStatus	= (glGetGraphicsResetStatusFunc)	loader->get("glGetGraphicsResetStatus");
	gl->getnUniformfv			= (glGetnUniformfvFunc)				loader->get("glGetnUniformfv");
	gl->getnUniformiv			= (glGetnUniformivFunc)				loader->get("glGetnUniformiv");
	gl->getnUniformuiv			= (glGetnUniformuivFunc)			loader->get("glGetnUniformuiv");
	gl->readnPixels				= (glReadnPixelsFunc)				loader->get("glReadnPixels");
}

if (de::contains(extSet, "GL_KHR_parallel_shader_compile"))
{
	gl->maxShaderCompilerThreadsKHR	= (glMaxShaderCompilerThreadsKHRFunc)	loader->get("glMaxShaderCompilerThreadsKHR");
}

if (de::contains(extSet, "GL_EXT_direct_state_access"))
{
	gl->bindMultiTextureEXT								= (glBindMultiTextureEXTFunc)								loader->get("glBindMultiTextureEXT");
	gl->checkNamedFramebufferStatusEXT					= (glCheckNamedFramebufferStatusEXTFunc)					loader->get("glCheckNamedFramebufferStatusEXT");
	gl->clearNamedBufferDataEXT							= (glClearNamedBufferDataEXTFunc)							loader->get("glClearNamedBufferDataEXT");
	gl->clearNamedBufferSubDataEXT						= (glClearNamedBufferSubDataEXTFunc)						loader->get("glClearNamedBufferSubDataEXT");
	gl->clientAttribDefaultEXT							= (glClientAttribDefaultEXTFunc)							loader->get("glClientAttribDefaultEXT");
	gl->compressedMultiTexImage1DEXT					= (glCompressedMultiTexImage1DEXTFunc)						loader->get("glCompressedMultiTexImage1DEXT");
	gl->compressedMultiTexImage2DEXT					= (glCompressedMultiTexImage2DEXTFunc)						loader->get("glCompressedMultiTexImage2DEXT");
	gl->compressedMultiTexImage3DEXT					= (glCompressedMultiTexImage3DEXTFunc)						loader->get("glCompressedMultiTexImage3DEXT");
	gl->compressedMultiTexSubImage1DEXT					= (glCompressedMultiTexSubImage1DEXTFunc)					loader->get("glCompressedMultiTexSubImage1DEXT");
	gl->compressedMultiTexSubImage2DEXT					= (glCompressedMultiTexSubImage2DEXTFunc)					loader->get("glCompressedMultiTexSubImage2DEXT");
	gl->compressedMultiTexSubImage3DEXT					= (glCompressedMultiTexSubImage3DEXTFunc)					loader->get("glCompressedMultiTexSubImage3DEXT");
	gl->compressedTextureImage1DEXT						= (glCompressedTextureImage1DEXTFunc)						loader->get("glCompressedTextureImage1DEXT");
	gl->compressedTextureImage2DEXT						= (glCompressedTextureImage2DEXTFunc)						loader->get("glCompressedTextureImage2DEXT");
	gl->compressedTextureImage3DEXT						= (glCompressedTextureImage3DEXTFunc)						loader->get("glCompressedTextureImage3DEXT");
	gl->compressedTextureSubImage1DEXT					= (glCompressedTextureSubImage1DEXTFunc)					loader->get("glCompressedTextureSubImage1DEXT");
	gl->compressedTextureSubImage2DEXT					= (glCompressedTextureSubImage2DEXTFunc)					loader->get("glCompressedTextureSubImage2DEXT");
	gl->compressedTextureSubImage3DEXT					= (glCompressedTextureSubImage3DEXTFunc)					loader->get("glCompressedTextureSubImage3DEXT");
	gl->copyMultiTexImage1DEXT							= (glCopyMultiTexImage1DEXTFunc)							loader->get("glCopyMultiTexImage1DEXT");
	gl->copyMultiTexImage2DEXT							= (glCopyMultiTexImage2DEXTFunc)							loader->get("glCopyMultiTexImage2DEXT");
	gl->copyMultiTexSubImage1DEXT						= (glCopyMultiTexSubImage1DEXTFunc)							loader->get("glCopyMultiTexSubImage1DEXT");
	gl->copyMultiTexSubImage2DEXT						= (glCopyMultiTexSubImage2DEXTFunc)							loader->get("glCopyMultiTexSubImage2DEXT");
	gl->copyMultiTexSubImage3DEXT						= (glCopyMultiTexSubImage3DEXTFunc)							loader->get("glCopyMultiTexSubImage3DEXT");
	gl->copyTextureImage1DEXT							= (glCopyTextureImage1DEXTFunc)								loader->get("glCopyTextureImage1DEXT");
	gl->copyTextureImage2DEXT							= (glCopyTextureImage2DEXTFunc)								loader->get("glCopyTextureImage2DEXT");
	gl->copyTextureSubImage1DEXT						= (glCopyTextureSubImage1DEXTFunc)							loader->get("glCopyTextureSubImage1DEXT");
	gl->copyTextureSubImage2DEXT						= (glCopyTextureSubImage2DEXTFunc)							loader->get("glCopyTextureSubImage2DEXT");
	gl->copyTextureSubImage3DEXT						= (glCopyTextureSubImage3DEXTFunc)							loader->get("glCopyTextureSubImage3DEXT");
	gl->disableClientStateIndexedEXT					= (glDisableClientStateIndexedEXTFunc)						loader->get("glDisableClientStateIndexedEXT");
	gl->disableClientStateiEXT							= (glDisableClientStateiEXTFunc)							loader->get("glDisableClientStateiEXT");
	gl->disablei										= (glDisableiFunc)											loader->get("glDisableIndexedEXT");
	gl->disableVertexArrayAttribEXT						= (glDisableVertexArrayAttribEXTFunc)						loader->get("glDisableVertexArrayAttribEXT");
	gl->disableVertexArrayEXT							= (glDisableVertexArrayEXTFunc)								loader->get("glDisableVertexArrayEXT");
	gl->enableClientStateIndexedEXT						= (glEnableClientStateIndexedEXTFunc)						loader->get("glEnableClientStateIndexedEXT");
	gl->enableClientStateiEXT							= (glEnableClientStateiEXTFunc)								loader->get("glEnableClientStateiEXT");
	gl->enablei											= (glEnableiFunc)											loader->get("glEnableIndexedEXT");
	gl->enableVertexArrayAttribEXT						= (glEnableVertexArrayAttribEXTFunc)						loader->get("glEnableVertexArrayAttribEXT");
	gl->enableVertexArrayEXT							= (glEnableVertexArrayEXTFunc)								loader->get("glEnableVertexArrayEXT");
	gl->flushMappedNamedBufferRangeEXT					= (glFlushMappedNamedBufferRangeEXTFunc)					loader->get("glFlushMappedNamedBufferRangeEXT");
	gl->framebufferDrawBufferEXT						= (glFramebufferDrawBufferEXTFunc)							loader->get("glFramebufferDrawBufferEXT");
	gl->framebufferDrawBuffersEXT						= (glFramebufferDrawBuffersEXTFunc)							loader->get("glFramebufferDrawBuffersEXT");
	gl->framebufferReadBufferEXT						= (glFramebufferReadBufferEXTFunc)							loader->get("glFramebufferReadBufferEXT");
	gl->generateMultiTexMipmapEXT						= (glGenerateMultiTexMipmapEXTFunc)							loader->get("glGenerateMultiTexMipmapEXT");
	gl->generateTextureMipmapEXT						= (glGenerateTextureMipmapEXTFunc)							loader->get("glGenerateTextureMipmapEXT");
	gl->getBooleani_v									= (glGetBooleani_vFunc)										loader->get("glGetBooleanIndexedvEXT");
	gl->getCompressedMultiTexImageEXT					= (glGetCompressedMultiTexImageEXTFunc)						loader->get("glGetCompressedMultiTexImageEXT");
	gl->getCompressedTextureImageEXT					= (glGetCompressedTextureImageEXTFunc)						loader->get("glGetCompressedTextureImageEXT");
	gl->getDoublei_v									= (glGetDoublei_vFunc)										loader->get("glGetDoubleIndexedvEXT");
	gl->getDoublei_v									= (glGetDoublei_vFunc)										loader->get("glGetDoublei_vEXT");
	gl->getFloati_v										= (glGetFloati_vFunc)										loader->get("glGetFloatIndexedvEXT");
	gl->getFloati_v										= (glGetFloati_vFunc)										loader->get("glGetFloati_vEXT");
	gl->getFramebufferParameterivEXT					= (glGetFramebufferParameterivEXTFunc)						loader->get("glGetFramebufferParameterivEXT");
	gl->getIntegeri_v									= (glGetIntegeri_vFunc)										loader->get("glGetIntegerIndexedvEXT");
	gl->getMultiTexEnvfvEXT								= (glGetMultiTexEnvfvEXTFunc)								loader->get("glGetMultiTexEnvfvEXT");
	gl->getMultiTexEnvivEXT								= (glGetMultiTexEnvivEXTFunc)								loader->get("glGetMultiTexEnvivEXT");
	gl->getMultiTexGendvEXT								= (glGetMultiTexGendvEXTFunc)								loader->get("glGetMultiTexGendvEXT");
	gl->getMultiTexGenfvEXT								= (glGetMultiTexGenfvEXTFunc)								loader->get("glGetMultiTexGenfvEXT");
	gl->getMultiTexGenivEXT								= (glGetMultiTexGenivEXTFunc)								loader->get("glGetMultiTexGenivEXT");
	gl->getMultiTexImageEXT								= (glGetMultiTexImageEXTFunc)								loader->get("glGetMultiTexImageEXT");
	gl->getMultiTexLevelParameterfvEXT					= (glGetMultiTexLevelParameterfvEXTFunc)					loader->get("glGetMultiTexLevelParameterfvEXT");
	gl->getMultiTexLevelParameterivEXT					= (glGetMultiTexLevelParameterivEXTFunc)					loader->get("glGetMultiTexLevelParameterivEXT");
	gl->getMultiTexParameterIivEXT						= (glGetMultiTexParameterIivEXTFunc)						loader->get("glGetMultiTexParameterIivEXT");
	gl->getMultiTexParameterIuivEXT						= (glGetMultiTexParameterIuivEXTFunc)						loader->get("glGetMultiTexParameterIuivEXT");
	gl->getMultiTexParameterfvEXT						= (glGetMultiTexParameterfvEXTFunc)							loader->get("glGetMultiTexParameterfvEXT");
	gl->getMultiTexParameterivEXT						= (glGetMultiTexParameterivEXTFunc)							loader->get("glGetMultiTexParameterivEXT");
	gl->getNamedBufferParameterivEXT					= (glGetNamedBufferParameterivEXTFunc)						loader->get("glGetNamedBufferParameterivEXT");
	gl->getNamedBufferPointervEXT						= (glGetNamedBufferPointervEXTFunc)							loader->get("glGetNamedBufferPointervEXT");
	gl->getNamedBufferSubDataEXT						= (glGetNamedBufferSubDataEXTFunc)							loader->get("glGetNamedBufferSubDataEXT");
	gl->getNamedFramebufferAttachmentParameterivEXT		= (glGetNamedFramebufferAttachmentParameterivEXTFunc)		loader->get("glGetNamedFramebufferAttachmentParameterivEXT");
	gl->getNamedFramebufferParameterivEXT				= (glGetNamedFramebufferParameterivEXTFunc)					loader->get("glGetNamedFramebufferParameterivEXT");
	gl->getNamedProgramLocalParameterIivEXT				= (glGetNamedProgramLocalParameterIivEXTFunc)				loader->get("glGetNamedProgramLocalParameterIivEXT");
	gl->getNamedProgramLocalParameterIuivEXT			= (glGetNamedProgramLocalParameterIuivEXTFunc)				loader->get("glGetNamedProgramLocalParameterIuivEXT");
	gl->getNamedProgramLocalParameterdvEXT				= (glGetNamedProgramLocalParameterdvEXTFunc)				loader->get("glGetNamedProgramLocalParameterdvEXT");
	gl->getNamedProgramLocalParameterfvEXT				= (glGetNamedProgramLocalParameterfvEXTFunc)				loader->get("glGetNamedProgramLocalParameterfvEXT");
	gl->getNamedProgramStringEXT						= (glGetNamedProgramStringEXTFunc)							loader->get("glGetNamedProgramStringEXT");
	gl->getNamedProgramivEXT							= (glGetNamedProgramivEXTFunc)								loader->get("glGetNamedProgramivEXT");
	gl->getNamedRenderbufferParameterivEXT				= (glGetNamedRenderbufferParameterivEXTFunc)				loader->get("glGetNamedRenderbufferParameterivEXT");
	gl->getPointerIndexedvEXT							= (glGetPointerIndexedvEXTFunc)								loader->get("glGetPointerIndexedvEXT");
	gl->getPointeri_vEXT								= (glGetPointeri_vEXTFunc)									loader->get("glGetPointeri_vEXT");
	gl->getTextureImageEXT								= (glGetTextureImageEXTFunc)								loader->get("glGetTextureImageEXT");
	gl->getTextureLevelParameterfvEXT					= (glGetTextureLevelParameterfvEXTFunc)						loader->get("glGetTextureLevelParameterfvEXT");
	gl->getTextureLevelParameterivEXT					= (glGetTextureLevelParameterivEXTFunc)						loader->get("glGetTextureLevelParameterivEXT");
	gl->getTextureParameterIivEXT						= (glGetTextureParameterIivEXTFunc)							loader->get("glGetTextureParameterIivEXT");
	gl->getTextureParameterIuivEXT						= (glGetTextureParameterIuivEXTFunc)						loader->get("glGetTextureParameterIuivEXT");
	gl->getTextureParameterfvEXT						= (glGetTextureParameterfvEXTFunc)							loader->get("glGetTextureParameterfvEXT");
	gl->getTextureParameterivEXT						= (glGetTextureParameterivEXTFunc)							loader->get("glGetTextureParameterivEXT");
	gl->getVertexArrayIntegeri_vEXT						= (glGetVertexArrayIntegeri_vEXTFunc)						loader->get("glGetVertexArrayIntegeri_vEXT");
	gl->getVertexArrayIntegervEXT						= (glGetVertexArrayIntegervEXTFunc)							loader->get("glGetVertexArrayIntegervEXT");
	gl->getVertexArrayPointeri_vEXT						= (glGetVertexArrayPointeri_vEXTFunc)						loader->get("glGetVertexArrayPointeri_vEXT");
	gl->getVertexArrayPointervEXT						= (glGetVertexArrayPointervEXTFunc)							loader->get("glGetVertexArrayPointervEXT");
	gl->isEnabledi										= (glIsEnablediFunc)										loader->get("glIsEnabledIndexedEXT");
	gl->mapNamedBufferEXT								= (glMapNamedBufferEXTFunc)									loader->get("glMapNamedBufferEXT");
	gl->mapNamedBufferRangeEXT							= (glMapNamedBufferRangeEXTFunc)							loader->get("glMapNamedBufferRangeEXT");
	gl->matrixFrustumEXT								= (glMatrixFrustumEXTFunc)									loader->get("glMatrixFrustumEXT");
	gl->matrixLoadIdentityEXT							= (glMatrixLoadIdentityEXTFunc)								loader->get("glMatrixLoadIdentityEXT");
	gl->matrixLoadTransposedEXT							= (glMatrixLoadTransposedEXTFunc)							loader->get("glMatrixLoadTransposedEXT");
	gl->matrixLoadTransposefEXT							= (glMatrixLoadTransposefEXTFunc)							loader->get("glMatrixLoadTransposefEXT");
	gl->matrixLoaddEXT									= (glMatrixLoaddEXTFunc)									loader->get("glMatrixLoaddEXT");
	gl->matrixLoadfEXT									= (glMatrixLoadfEXTFunc)									loader->get("glMatrixLoadfEXT");
	gl->matrixMultTransposedEXT							= (glMatrixMultTransposedEXTFunc)							loader->get("glMatrixMultTransposedEXT");
	gl->matrixMultTransposefEXT							= (glMatrixMultTransposefEXTFunc)							loader->get("glMatrixMultTransposefEXT");
	gl->matrixMultdEXT									= (glMatrixMultdEXTFunc)									loader->get("glMatrixMultdEXT");
	gl->matrixMultfEXT									= (glMatrixMultfEXTFunc)									loader->get("glMatrixMultfEXT");
	gl->matrixOrthoEXT									= (glMatrixOrthoEXTFunc)									loader->get("glMatrixOrthoEXT");
	gl->matrixPopEXT									= (glMatrixPopEXTFunc)										loader->get("glMatrixPopEXT");
	gl->matrixPushEXT									= (glMatrixPushEXTFunc)										loader->get("glMatrixPushEXT");
	gl->matrixRotatedEXT								= (glMatrixRotatedEXTFunc)									loader->get("glMatrixRotatedEXT");
	gl->matrixRotatefEXT								= (glMatrixRotatefEXTFunc)									loader->get("glMatrixRotatefEXT");
	gl->matrixScaledEXT									= (glMatrixScaledEXTFunc)									loader->get("glMatrixScaledEXT");
	gl->matrixScalefEXT									= (glMatrixScalefEXTFunc)									loader->get("glMatrixScalefEXT");
	gl->matrixTranslatedEXT								= (glMatrixTranslatedEXTFunc)								loader->get("glMatrixTranslatedEXT");
	gl->matrixTranslatefEXT								= (glMatrixTranslatefEXTFunc)								loader->get("glMatrixTranslatefEXT");
	gl->multiTexBufferEXT								= (glMultiTexBufferEXTFunc)									loader->get("glMultiTexBufferEXT");
	gl->multiTexCoordPointerEXT							= (glMultiTexCoordPointerEXTFunc)							loader->get("glMultiTexCoordPointerEXT");
	gl->multiTexEnvfEXT									= (glMultiTexEnvfEXTFunc)									loader->get("glMultiTexEnvfEXT");
	gl->multiTexEnvfvEXT								= (glMultiTexEnvfvEXTFunc)									loader->get("glMultiTexEnvfvEXT");
	gl->multiTexEnviEXT									= (glMultiTexEnviEXTFunc)									loader->get("glMultiTexEnviEXT");
	gl->multiTexEnvivEXT								= (glMultiTexEnvivEXTFunc)									loader->get("glMultiTexEnvivEXT");
	gl->multiTexGendEXT									= (glMultiTexGendEXTFunc)									loader->get("glMultiTexGendEXT");
	gl->multiTexGendvEXT								= (glMultiTexGendvEXTFunc)									loader->get("glMultiTexGendvEXT");
	gl->multiTexGenfEXT									= (glMultiTexGenfEXTFunc)									loader->get("glMultiTexGenfEXT");
	gl->multiTexGenfvEXT								= (glMultiTexGenfvEXTFunc)									loader->get("glMultiTexGenfvEXT");
	gl->multiTexGeniEXT									= (glMultiTexGeniEXTFunc)									loader->get("glMultiTexGeniEXT");
	gl->multiTexGenivEXT								= (glMultiTexGenivEXTFunc)									loader->get("glMultiTexGenivEXT");
	gl->multiTexImage1DEXT								= (glMultiTexImage1DEXTFunc)								loader->get("glMultiTexImage1DEXT");
	gl->multiTexImage2DEXT								= (glMultiTexImage2DEXTFunc)								loader->get("glMultiTexImage2DEXT");
	gl->multiTexImage3DEXT								= (glMultiTexImage3DEXTFunc)								loader->get("glMultiTexImage3DEXT");
	gl->multiTexParameterIivEXT							= (glMultiTexParameterIivEXTFunc)							loader->get("glMultiTexParameterIivEXT");
	gl->multiTexParameterIuivEXT						= (glMultiTexParameterIuivEXTFunc)							loader->get("glMultiTexParameterIuivEXT");
	gl->multiTexParameterfEXT							= (glMultiTexParameterfEXTFunc)								loader->get("glMultiTexParameterfEXT");
	gl->multiTexParameterfvEXT							= (glMultiTexParameterfvEXTFunc)							loader->get("glMultiTexParameterfvEXT");
	gl->multiTexParameteriEXT							= (glMultiTexParameteriEXTFunc)								loader->get("glMultiTexParameteriEXT");
	gl->multiTexParameterivEXT							= (glMultiTexParameterivEXTFunc)							loader->get("glMultiTexParameterivEXT");
	gl->multiTexRenderbufferEXT							= (glMultiTexRenderbufferEXTFunc)							loader->get("glMultiTexRenderbufferEXT");
	gl->multiTexSubImage1DEXT							= (glMultiTexSubImage1DEXTFunc)								loader->get("glMultiTexSubImage1DEXT");
	gl->multiTexSubImage2DEXT							= (glMultiTexSubImage2DEXTFunc)								loader->get("glMultiTexSubImage2DEXT");
	gl->multiTexSubImage3DEXT							= (glMultiTexSubImage3DEXTFunc)								loader->get("glMultiTexSubImage3DEXT");
	gl->namedBufferDataEXT								= (glNamedBufferDataEXTFunc)								loader->get("glNamedBufferDataEXT");
	gl->namedBufferStorage								= (glNamedBufferStorageFunc)								loader->get("glNamedBufferStorageEXT");
	gl->namedBufferSubData								= (glNamedBufferSubDataFunc)								loader->get("glNamedBufferSubDataEXT");
	gl->namedCopyBufferSubDataEXT						= (glNamedCopyBufferSubDataEXTFunc)							loader->get("glNamedCopyBufferSubDataEXT");
	gl->namedFramebufferParameteriEXT					= (glNamedFramebufferParameteriEXTFunc)						loader->get("glNamedFramebufferParameteriEXT");
	gl->namedFramebufferRenderbufferEXT					= (glNamedFramebufferRenderbufferEXTFunc)					loader->get("glNamedFramebufferRenderbufferEXT");
	gl->namedFramebufferTexture1DEXT					= (glNamedFramebufferTexture1DEXTFunc)						loader->get("glNamedFramebufferTexture1DEXT");
	gl->namedFramebufferTexture2DEXT					= (glNamedFramebufferTexture2DEXTFunc)						loader->get("glNamedFramebufferTexture2DEXT");
	gl->namedFramebufferTexture3DEXT					= (glNamedFramebufferTexture3DEXTFunc)						loader->get("glNamedFramebufferTexture3DEXT");
	gl->namedFramebufferTextureEXT						= (glNamedFramebufferTextureEXTFunc)						loader->get("glNamedFramebufferTextureEXT");
	gl->namedFramebufferTextureFaceEXT					= (glNamedFramebufferTextureFaceEXTFunc)					loader->get("glNamedFramebufferTextureFaceEXT");
	gl->namedFramebufferTextureLayerEXT					= (glNamedFramebufferTextureLayerEXTFunc)					loader->get("glNamedFramebufferTextureLayerEXT");
	gl->namedProgramLocalParameter4dEXT					= (glNamedProgramLocalParameter4dEXTFunc)					loader->get("glNamedProgramLocalParameter4dEXT");
	gl->namedProgramLocalParameter4dvEXT				= (glNamedProgramLocalParameter4dvEXTFunc)					loader->get("glNamedProgramLocalParameter4dvEXT");
	gl->namedProgramLocalParameter4fEXT					= (glNamedProgramLocalParameter4fEXTFunc)					loader->get("glNamedProgramLocalParameter4fEXT");
	gl->namedProgramLocalParameter4fvEXT				= (glNamedProgramLocalParameter4fvEXTFunc)					loader->get("glNamedProgramLocalParameter4fvEXT");
	gl->namedProgramLocalParameterI4iEXT				= (glNamedProgramLocalParameterI4iEXTFunc)					loader->get("glNamedProgramLocalParameterI4iEXT");
	gl->namedProgramLocalParameterI4ivEXT				= (glNamedProgramLocalParameterI4ivEXTFunc)					loader->get("glNamedProgramLocalParameterI4ivEXT");
	gl->namedProgramLocalParameterI4uiEXT				= (glNamedProgramLocalParameterI4uiEXTFunc)					loader->get("glNamedProgramLocalParameterI4uiEXT");
	gl->namedProgramLocalParameterI4uivEXT				= (glNamedProgramLocalParameterI4uivEXTFunc)				loader->get("glNamedProgramLocalParameterI4uivEXT");
	gl->namedProgramLocalParameters4fvEXT				= (glNamedProgramLocalParameters4fvEXTFunc)					loader->get("glNamedProgramLocalParameters4fvEXT");
	gl->namedProgramLocalParametersI4ivEXT				= (glNamedProgramLocalParametersI4ivEXTFunc)				loader->get("glNamedProgramLocalParametersI4ivEXT");
	gl->namedProgramLocalParametersI4uivEXT				= (glNamedProgramLocalParametersI4uivEXTFunc)				loader->get("glNamedProgramLocalParametersI4uivEXT");
	gl->namedProgramStringEXT							= (glNamedProgramStringEXTFunc)								loader->get("glNamedProgramStringEXT");
	gl->namedRenderbufferStorageEXT						= (glNamedRenderbufferStorageEXTFunc)						loader->get("glNamedRenderbufferStorageEXT");
	gl->namedRenderbufferStorageMultisampleCoverageEXT	= (glNamedRenderbufferStorageMultisampleCoverageEXTFunc)	loader->get("glNamedRenderbufferStorageMultisampleCoverageEXT");
	gl->namedRenderbufferStorageMultisampleEXT			= (glNamedRenderbufferStorageMultisampleEXTFunc)			loader->get("glNamedRenderbufferStorageMultisampleEXT");
	gl->programUniform1dEXT								= (glProgramUniform1dEXTFunc)								loader->get("glProgramUniform1dEXT");
	gl->programUniform1dvEXT							= (glProgramUniform1dvEXTFunc)								loader->get("glProgramUniform1dvEXT");
	gl->programUniform1f								= (glProgramUniform1fFunc)									loader->get("glProgramUniform1fEXT");
	gl->programUniform1fv								= (glProgramUniform1fvFunc)									loader->get("glProgramUniform1fvEXT");
	gl->programUniform1i								= (glProgramUniform1iFunc)									loader->get("glProgramUniform1iEXT");
	gl->programUniform1iv								= (glProgramUniform1ivFunc)									loader->get("glProgramUniform1ivEXT");
	gl->programUniform1ui								= (glProgramUniform1uiFunc)									loader->get("glProgramUniform1uiEXT");
	gl->programUniform1uiv								= (glProgramUniform1uivFunc)								loader->get("glProgramUniform1uivEXT");
	gl->programUniform2dEXT								= (glProgramUniform2dEXTFunc)								loader->get("glProgramUniform2dEXT");
	gl->programUniform2dvEXT							= (glProgramUniform2dvEXTFunc)								loader->get("glProgramUniform2dvEXT");
	gl->programUniform2f								= (glProgramUniform2fFunc)									loader->get("glProgramUniform2fEXT");
	gl->programUniform2fv								= (glProgramUniform2fvFunc)									loader->get("glProgramUniform2fvEXT");
	gl->programUniform2i								= (glProgramUniform2iFunc)									loader->get("glProgramUniform2iEXT");
	gl->programUniform2iv								= (glProgramUniform2ivFunc)									loader->get("glProgramUniform2ivEXT");
	gl->programUniform2ui								= (glProgramUniform2uiFunc)									loader->get("glProgramUniform2uiEXT");
	gl->programUniform2uiv								= (glProgramUniform2uivFunc)								loader->get("glProgramUniform2uivEXT");
	gl->programUniform3dEXT								= (glProgramUniform3dEXTFunc)								loader->get("glProgramUniform3dEXT");
	gl->programUniform3dvEXT							= (glProgramUniform3dvEXTFunc)								loader->get("glProgramUniform3dvEXT");
	gl->programUniform3f								= (glProgramUniform3fFunc)									loader->get("glProgramUniform3fEXT");
	gl->programUniform3fv								= (glProgramUniform3fvFunc)									loader->get("glProgramUniform3fvEXT");
	gl->programUniform3i								= (glProgramUniform3iFunc)									loader->get("glProgramUniform3iEXT");
	gl->programUniform3iv								= (glProgramUniform3ivFunc)									loader->get("glProgramUniform3ivEXT");
	gl->programUniform3ui								= (glProgramUniform3uiFunc)									loader->get("glProgramUniform3uiEXT");
	gl->programUniform3uiv								= (glProgramUniform3uivFunc)								loader->get("glProgramUniform3uivEXT");
	gl->programUniform4dEXT								= (glProgramUniform4dEXTFunc)								loader->get("glProgramUniform4dEXT");
	gl->programUniform4dvEXT							= (glProgramUniform4dvEXTFunc)								loader->get("glProgramUniform4dvEXT");
	gl->programUniform4f								= (glProgramUniform4fFunc)									loader->get("glProgramUniform4fEXT");
	gl->programUniform4fv								= (glProgramUniform4fvFunc)									loader->get("glProgramUniform4fvEXT");
	gl->programUniform4i								= (glProgramUniform4iFunc)									loader->get("glProgramUniform4iEXT");
	gl->programUniform4iv								= (glProgramUniform4ivFunc)									loader->get("glProgramUniform4ivEXT");
	gl->programUniform4ui								= (glProgramUniform4uiFunc)									loader->get("glProgramUniform4uiEXT");
	gl->programUniform4uiv								= (glProgramUniform4uivFunc)								loader->get("glProgramUniform4uivEXT");
	gl->programUniformMatrix2dvEXT						= (glProgramUniformMatrix2dvEXTFunc)						loader->get("glProgramUniformMatrix2dvEXT");
	gl->programUniformMatrix2fv							= (glProgramUniformMatrix2fvFunc)							loader->get("glProgramUniformMatrix2fvEXT");
	gl->programUniformMatrix2x3dvEXT					= (glProgramUniformMatrix2x3dvEXTFunc)						loader->get("glProgramUniformMatrix2x3dvEXT");
	gl->programUniformMatrix2x3fv						= (glProgramUniformMatrix2x3fvFunc)							loader->get("glProgramUniformMatrix2x3fvEXT");
	gl->programUniformMatrix2x4dvEXT					= (glProgramUniformMatrix2x4dvEXTFunc)						loader->get("glProgramUniformMatrix2x4dvEXT");
	gl->programUniformMatrix2x4fv						= (glProgramUniformMatrix2x4fvFunc)							loader->get("glProgramUniformMatrix2x4fvEXT");
	gl->programUniformMatrix3dvEXT						= (glProgramUniformMatrix3dvEXTFunc)						loader->get("glProgramUniformMatrix3dvEXT");
	gl->programUniformMatrix3fv							= (glProgramUniformMatrix3fvFunc)							loader->get("glProgramUniformMatrix3fvEXT");
	gl->programUniformMatrix3x2dvEXT					= (glProgramUniformMatrix3x2dvEXTFunc)						loader->get("glProgramUniformMatrix3x2dvEXT");
	gl->programUniformMatrix3x2fv						= (glProgramUniformMatrix3x2fvFunc)							loader->get("glProgramUniformMatrix3x2fvEXT");
	gl->programUniformMatrix3x4dvEXT					= (glProgramUniformMatrix3x4dvEXTFunc)						loader->get("glProgramUniformMatrix3x4dvEXT");
	gl->programUniformMatrix3x4fv						= (glProgramUniformMatrix3x4fvFunc)							loader->get("glProgramUniformMatrix3x4fvEXT");
	gl->programUniformMatrix4dvEXT						= (glProgramUniformMatrix4dvEXTFunc)						loader->get("glProgramUniformMatrix4dvEXT");
	gl->programUniformMatrix4fv							= (glProgramUniformMatrix4fvFunc)							loader->get("glProgramUniformMatrix4fvEXT");
	gl->programUniformMatrix4x2dvEXT					= (glProgramUniformMatrix4x2dvEXTFunc)						loader->get("glProgramUniformMatrix4x2dvEXT");
	gl->programUniformMatrix4x2fv						= (glProgramUniformMatrix4x2fvFunc)							loader->get("glProgramUniformMatrix4x2fvEXT");
	gl->programUniformMatrix4x3dvEXT					= (glProgramUniformMatrix4x3dvEXTFunc)						loader->get("glProgramUniformMatrix4x3dvEXT");
	gl->programUniformMatrix4x3fv						= (glProgramUniformMatrix4x3fvFunc)							loader->get("glProgramUniformMatrix4x3fvEXT");
	gl->pushClientAttribDefaultEXT						= (glPushClientAttribDefaultEXTFunc)						loader->get("glPushClientAttribDefaultEXT");
	gl->textureBufferEXT								= (glTextureBufferEXTFunc)									loader->get("glTextureBufferEXT");
	gl->textureBufferRangeEXT							= (glTextureBufferRangeEXTFunc)								loader->get("glTextureBufferRangeEXT");
	gl->textureImage1DEXT								= (glTextureImage1DEXTFunc)									loader->get("glTextureImage1DEXT");
	gl->textureImage2DEXT								= (glTextureImage2DEXTFunc)									loader->get("glTextureImage2DEXT");
	gl->textureImage3DEXT								= (glTextureImage3DEXTFunc)									loader->get("glTextureImage3DEXT");
	gl->texturePageCommitmentEXT						= (glTexturePageCommitmentEXTFunc)							loader->get("glTexturePageCommitmentEXT");
	gl->textureParameterIivEXT							= (glTextureParameterIivEXTFunc)							loader->get("glTextureParameterIivEXT");
	gl->textureParameterIuivEXT							= (glTextureParameterIuivEXTFunc)							loader->get("glTextureParameterIuivEXT");
	gl->textureParameterfEXT							= (glTextureParameterfEXTFunc)								loader->get("glTextureParameterfEXT");
	gl->textureParameterfvEXT							= (glTextureParameterfvEXTFunc)								loader->get("glTextureParameterfvEXT");
	gl->textureParameteriEXT							= (glTextureParameteriEXTFunc)								loader->get("glTextureParameteriEXT");
	gl->textureParameterivEXT							= (glTextureParameterivEXTFunc)								loader->get("glTextureParameterivEXT");
	gl->textureRenderbufferEXT							= (glTextureRenderbufferEXTFunc)							loader->get("glTextureRenderbufferEXT");
	gl->textureStorage1DEXT								= (glTextureStorage1DEXTFunc)								loader->get("glTextureStorage1DEXT");
	gl->textureStorage2DEXT								= (glTextureStorage2DEXTFunc)								loader->get("glTextureStorage2DEXT");
	gl->textureStorage2DMultisampleEXT					= (glTextureStorage2DMultisampleEXTFunc)					loader->get("glTextureStorage2DMultisampleEXT");
	gl->textureStorage3DEXT								= (glTextureStorage3DEXTFunc)								loader->get("glTextureStorage3DEXT");
	gl->textureStorage3DMultisampleEXT					= (glTextureStorage3DMultisampleEXTFunc)					loader->get("glTextureStorage3DMultisampleEXT");
	gl->textureSubImage1DEXT							= (glTextureSubImage1DEXTFunc)								loader->get("glTextureSubImage1DEXT");
	gl->textureSubImage2DEXT							= (glTextureSubImage2DEXTFunc)								loader->get("glTextureSubImage2DEXT");
	gl->textureSubImage3DEXT							= (glTextureSubImage3DEXTFunc)								loader->get("glTextureSubImage3DEXT");
	gl->unmapNamedBufferEXT								= (glUnmapNamedBufferEXTFunc)								loader->get("glUnmapNamedBufferEXT");
	gl->vertexArrayBindVertexBufferEXT					= (glVertexArrayBindVertexBufferEXTFunc)					loader->get("glVertexArrayBindVertexBufferEXT");
	gl->vertexArrayColorOffsetEXT						= (glVertexArrayColorOffsetEXTFunc)							loader->get("glVertexArrayColorOffsetEXT");
	gl->vertexArrayEdgeFlagOffsetEXT					= (glVertexArrayEdgeFlagOffsetEXTFunc)						loader->get("glVertexArrayEdgeFlagOffsetEXT");
	gl->vertexArrayFogCoordOffsetEXT					= (glVertexArrayFogCoordOffsetEXTFunc)						loader->get("glVertexArrayFogCoordOffsetEXT");
	gl->vertexArrayIndexOffsetEXT						= (glVertexArrayIndexOffsetEXTFunc)							loader->get("glVertexArrayIndexOffsetEXT");
	gl->vertexArrayMultiTexCoordOffsetEXT				= (glVertexArrayMultiTexCoordOffsetEXTFunc)					loader->get("glVertexArrayMultiTexCoordOffsetEXT");
	gl->vertexArrayNormalOffsetEXT						= (glVertexArrayNormalOffsetEXTFunc)						loader->get("glVertexArrayNormalOffsetEXT");
	gl->vertexArraySecondaryColorOffsetEXT				= (glVertexArraySecondaryColorOffsetEXTFunc)				loader->get("glVertexArraySecondaryColorOffsetEXT");
	gl->vertexArrayTexCoordOffsetEXT					= (glVertexArrayTexCoordOffsetEXTFunc)						loader->get("glVertexArrayTexCoordOffsetEXT");
	gl->vertexArrayVertexAttribBindingEXT				= (glVertexArrayVertexAttribBindingEXTFunc)					loader->get("glVertexArrayVertexAttribBindingEXT");
	gl->vertexArrayVertexAttribDivisorEXT				= (glVertexArrayVertexAttribDivisorEXTFunc)					loader->get("glVertexArrayVertexAttribDivisorEXT");
	gl->vertexArrayVertexAttribFormatEXT				= (glVertexArrayVertexAttribFormatEXTFunc)					loader->get("glVertexArrayVertexAttribFormatEXT");
	gl->vertexArrayVertexAttribIFormatEXT				= (glVertexArrayVertexAttribIFormatEXTFunc)					loader->get("glVertexArrayVertexAttribIFormatEXT");
	gl->vertexArrayVertexAttribIOffsetEXT				= (glVertexArrayVertexAttribIOffsetEXTFunc)					loader->get("glVertexArrayVertexAttribIOffsetEXT");
	gl->vertexArrayVertexAttribLFormatEXT				= (glVertexArrayVertexAttribLFormatEXTFunc)					loader->get("glVertexArrayVertexAttribLFormatEXT");
	gl->vertexArrayVertexAttribLOffsetEXT				= (glVertexArrayVertexAttribLOffsetEXTFunc)					loader->get("glVertexArrayVertexAttribLOffsetEXT");
	gl->vertexArrayVertexAttribOffsetEXT				= (glVertexArrayVertexAttribOffsetEXTFunc)					loader->get("glVertexArrayVertexAttribOffsetEXT");
	gl->vertexArrayVertexBindingDivisorEXT				= (glVertexArrayVertexBindingDivisorEXTFunc)				loader->get("glVertexArrayVertexBindingDivisorEXT");
	gl->vertexArrayVertexOffsetEXT						= (glVertexArrayVertexOffsetEXTFunc)						loader->get("glVertexArrayVertexOffsetEXT");
}

if (de::contains(extSet, "GL_EXT_direct_state_access"))
{
	gl->bindMultiTextureEXT								= (glBindMultiTextureEXTFunc)								loader->get("glBindMultiTextureEXT");
	gl->checkNamedFramebufferStatusEXT					= (glCheckNamedFramebufferStatusEXTFunc)					loader->get("glCheckNamedFramebufferStatusEXT");
	gl->clearNamedBufferDataEXT							= (glClearNamedBufferDataEXTFunc)							loader->get("glClearNamedBufferDataEXT");
	gl->clearNamedBufferSubDataEXT						= (glClearNamedBufferSubDataEXTFunc)						loader->get("glClearNamedBufferSubDataEXT");
	gl->clientAttribDefaultEXT							= (glClientAttribDefaultEXTFunc)							loader->get("glClientAttribDefaultEXT");
	gl->compressedMultiTexImage1DEXT					= (glCompressedMultiTexImage1DEXTFunc)						loader->get("glCompressedMultiTexImage1DEXT");
	gl->compressedMultiTexImage2DEXT					= (glCompressedMultiTexImage2DEXTFunc)						loader->get("glCompressedMultiTexImage2DEXT");
	gl->compressedMultiTexImage3DEXT					= (glCompressedMultiTexImage3DEXTFunc)						loader->get("glCompressedMultiTexImage3DEXT");
	gl->compressedMultiTexSubImage1DEXT					= (glCompressedMultiTexSubImage1DEXTFunc)					loader->get("glCompressedMultiTexSubImage1DEXT");
	gl->compressedMultiTexSubImage2DEXT					= (glCompressedMultiTexSubImage2DEXTFunc)					loader->get("glCompressedMultiTexSubImage2DEXT");
	gl->compressedMultiTexSubImage3DEXT					= (glCompressedMultiTexSubImage3DEXTFunc)					loader->get("glCompressedMultiTexSubImage3DEXT");
	gl->compressedTextureImage1DEXT						= (glCompressedTextureImage1DEXTFunc)						loader->get("glCompressedTextureImage1DEXT");
	gl->compressedTextureImage2DEXT						= (glCompressedTextureImage2DEXTFunc)						loader->get("glCompressedTextureImage2DEXT");
	gl->compressedTextureImage3DEXT						= (glCompressedTextureImage3DEXTFunc)						loader->get("glCompressedTextureImage3DEXT");
	gl->compressedTextureSubImage1DEXT					= (glCompressedTextureSubImage1DEXTFunc)					loader->get("glCompressedTextureSubImage1DEXT");
	gl->compressedTextureSubImage2DEXT					= (glCompressedTextureSubImage2DEXTFunc)					loader->get("glCompressedTextureSubImage2DEXT");
	gl->compressedTextureSubImage3DEXT					= (glCompressedTextureSubImage3DEXTFunc)					loader->get("glCompressedTextureSubImage3DEXT");
	gl->copyMultiTexImage1DEXT							= (glCopyMultiTexImage1DEXTFunc)							loader->get("glCopyMultiTexImage1DEXT");
	gl->copyMultiTexImage2DEXT							= (glCopyMultiTexImage2DEXTFunc)							loader->get("glCopyMultiTexImage2DEXT");
	gl->copyMultiTexSubImage1DEXT						= (glCopyMultiTexSubImage1DEXTFunc)							loader->get("glCopyMultiTexSubImage1DEXT");
	gl->copyMultiTexSubImage2DEXT						= (glCopyMultiTexSubImage2DEXTFunc)							loader->get("glCopyMultiTexSubImage2DEXT");
	gl->copyMultiTexSubImage3DEXT						= (glCopyMultiTexSubImage3DEXTFunc)							loader->get("glCopyMultiTexSubImage3DEXT");
	gl->copyTextureImage1DEXT							= (glCopyTextureImage1DEXTFunc)								loader->get("glCopyTextureImage1DEXT");
	gl->copyTextureImage2DEXT							= (glCopyTextureImage2DEXTFunc)								loader->get("glCopyTextureImage2DEXT");
	gl->copyTextureSubImage1DEXT						= (glCopyTextureSubImage1DEXTFunc)							loader->get("glCopyTextureSubImage1DEXT");
	gl->copyTextureSubImage2DEXT						= (glCopyTextureSubImage2DEXTFunc)							loader->get("glCopyTextureSubImage2DEXT");
	gl->copyTextureSubImage3DEXT						= (glCopyTextureSubImage3DEXTFunc)							loader->get("glCopyTextureSubImage3DEXT");
	gl->disableClientStateIndexedEXT					= (glDisableClientStateIndexedEXTFunc)						loader->get("glDisableClientStateIndexedEXT");
	gl->disableClientStateiEXT							= (glDisableClientStateiEXTFunc)							loader->get("glDisableClientStateiEXT");
	gl->disablei										= (glDisableiFunc)											loader->get("glDisableIndexedEXT");
	gl->disableVertexArrayAttribEXT						= (glDisableVertexArrayAttribEXTFunc)						loader->get("glDisableVertexArrayAttribEXT");
	gl->disableVertexArrayEXT							= (glDisableVertexArrayEXTFunc)								loader->get("glDisableVertexArrayEXT");
	gl->enableClientStateIndexedEXT						= (glEnableClientStateIndexedEXTFunc)						loader->get("glEnableClientStateIndexedEXT");
	gl->enableClientStateiEXT							= (glEnableClientStateiEXTFunc)								loader->get("glEnableClientStateiEXT");
	gl->enablei											= (glEnableiFunc)											loader->get("glEnableIndexedEXT");
	gl->enableVertexArrayAttribEXT						= (glEnableVertexArrayAttribEXTFunc)						loader->get("glEnableVertexArrayAttribEXT");
	gl->enableVertexArrayEXT							= (glEnableVertexArrayEXTFunc)								loader->get("glEnableVertexArrayEXT");
	gl->flushMappedNamedBufferRangeEXT					= (glFlushMappedNamedBufferRangeEXTFunc)					loader->get("glFlushMappedNamedBufferRangeEXT");
	gl->framebufferDrawBufferEXT						= (glFramebufferDrawBufferEXTFunc)							loader->get("glFramebufferDrawBufferEXT");
	gl->framebufferDrawBuffersEXT						= (glFramebufferDrawBuffersEXTFunc)							loader->get("glFramebufferDrawBuffersEXT");
	gl->framebufferReadBufferEXT						= (glFramebufferReadBufferEXTFunc)							loader->get("glFramebufferReadBufferEXT");
	gl->generateMultiTexMipmapEXT						= (glGenerateMultiTexMipmapEXTFunc)							loader->get("glGenerateMultiTexMipmapEXT");
	gl->generateTextureMipmapEXT						= (glGenerateTextureMipmapEXTFunc)							loader->get("glGenerateTextureMipmapEXT");
	gl->getBooleani_v									= (glGetBooleani_vFunc)										loader->get("glGetBooleanIndexedvEXT");
	gl->getCompressedMultiTexImageEXT					= (glGetCompressedMultiTexImageEXTFunc)						loader->get("glGetCompressedMultiTexImageEXT");
	gl->getCompressedTextureImageEXT					= (glGetCompressedTextureImageEXTFunc)						loader->get("glGetCompressedTextureImageEXT");
	gl->getDoublei_v									= (glGetDoublei_vFunc)										loader->get("glGetDoubleIndexedvEXT");
	gl->getDoublei_v									= (glGetDoublei_vFunc)										loader->get("glGetDoublei_vEXT");
	gl->getFloati_v										= (glGetFloati_vFunc)										loader->get("glGetFloatIndexedvEXT");
	gl->getFloati_v										= (glGetFloati_vFunc)										loader->get("glGetFloati_vEXT");
	gl->getFramebufferParameterivEXT					= (glGetFramebufferParameterivEXTFunc)						loader->get("glGetFramebufferParameterivEXT");
	gl->getIntegeri_v									= (glGetIntegeri_vFunc)										loader->get("glGetIntegerIndexedvEXT");
	gl->getMultiTexEnvfvEXT								= (glGetMultiTexEnvfvEXTFunc)								loader->get("glGetMultiTexEnvfvEXT");
	gl->getMultiTexEnvivEXT								= (glGetMultiTexEnvivEXTFunc)								loader->get("glGetMultiTexEnvivEXT");
	gl->getMultiTexGendvEXT								= (glGetMultiTexGendvEXTFunc)								loader->get("glGetMultiTexGendvEXT");
	gl->getMultiTexGenfvEXT								= (glGetMultiTexGenfvEXTFunc)								loader->get("glGetMultiTexGenfvEXT");
	gl->getMultiTexGenivEXT								= (glGetMultiTexGenivEXTFunc)								loader->get("glGetMultiTexGenivEXT");
	gl->getMultiTexImageEXT								= (glGetMultiTexImageEXTFunc)								loader->get("glGetMultiTexImageEXT");
	gl->getMultiTexLevelParameterfvEXT					= (glGetMultiTexLevelParameterfvEXTFunc)					loader->get("glGetMultiTexLevelParameterfvEXT");
	gl->getMultiTexLevelParameterivEXT					= (glGetMultiTexLevelParameterivEXTFunc)					loader->get("glGetMultiTexLevelParameterivEXT");
	gl->getMultiTexParameterIivEXT						= (glGetMultiTexParameterIivEXTFunc)						loader->get("glGetMultiTexParameterIivEXT");
	gl->getMultiTexParameterIuivEXT						= (glGetMultiTexParameterIuivEXTFunc)						loader->get("glGetMultiTexParameterIuivEXT");
	gl->getMultiTexParameterfvEXT						= (glGetMultiTexParameterfvEXTFunc)							loader->get("glGetMultiTexParameterfvEXT");
	gl->getMultiTexParameterivEXT						= (glGetMultiTexParameterivEXTFunc)							loader->get("glGetMultiTexParameterivEXT");
	gl->getNamedBufferParameterivEXT					= (glGetNamedBufferParameterivEXTFunc)						loader->get("glGetNamedBufferParameterivEXT");
	gl->getNamedBufferPointervEXT						= (glGetNamedBufferPointervEXTFunc)							loader->get("glGetNamedBufferPointervEXT");
	gl->getNamedBufferSubDataEXT						= (glGetNamedBufferSubDataEXTFunc)							loader->get("glGetNamedBufferSubDataEXT");
	gl->getNamedFramebufferAttachmentParameterivEXT		= (glGetNamedFramebufferAttachmentParameterivEXTFunc)		loader->get("glGetNamedFramebufferAttachmentParameterivEXT");
	gl->getNamedFramebufferParameterivEXT				= (glGetNamedFramebufferParameterivEXTFunc)					loader->get("glGetNamedFramebufferParameterivEXT");
	gl->getNamedProgramLocalParameterIivEXT				= (glGetNamedProgramLocalParameterIivEXTFunc)				loader->get("glGetNamedProgramLocalParameterIivEXT");
	gl->getNamedProgramLocalParameterIuivEXT			= (glGetNamedProgramLocalParameterIuivEXTFunc)				loader->get("glGetNamedProgramLocalParameterIuivEXT");
	gl->getNamedProgramLocalParameterdvEXT				= (glGetNamedProgramLocalParameterdvEXTFunc)				loader->get("glGetNamedProgramLocalParameterdvEXT");
	gl->getNamedProgramLocalParameterfvEXT				= (glGetNamedProgramLocalParameterfvEXTFunc)				loader->get("glGetNamedProgramLocalParameterfvEXT");
	gl->getNamedProgramStringEXT						= (glGetNamedProgramStringEXTFunc)							loader->get("glGetNamedProgramStringEXT");
	gl->getNamedProgramivEXT							= (glGetNamedProgramivEXTFunc)								loader->get("glGetNamedProgramivEXT");
	gl->getNamedRenderbufferParameterivEXT				= (glGetNamedRenderbufferParameterivEXTFunc)				loader->get("glGetNamedRenderbufferParameterivEXT");
	gl->getPointerIndexedvEXT							= (glGetPointerIndexedvEXTFunc)								loader->get("glGetPointerIndexedvEXT");
	gl->getPointeri_vEXT								= (glGetPointeri_vEXTFunc)									loader->get("glGetPointeri_vEXT");
	gl->getTextureImageEXT								= (glGetTextureImageEXTFunc)								loader->get("glGetTextureImageEXT");
	gl->getTextureLevelParameterfvEXT					= (glGetTextureLevelParameterfvEXTFunc)						loader->get("glGetTextureLevelParameterfvEXT");
	gl->getTextureLevelParameterivEXT					= (glGetTextureLevelParameterivEXTFunc)						loader->get("glGetTextureLevelParameterivEXT");
	gl->getTextureParameterIivEXT						= (glGetTextureParameterIivEXTFunc)							loader->get("glGetTextureParameterIivEXT");
	gl->getTextureParameterIuivEXT						= (glGetTextureParameterIuivEXTFunc)						loader->get("glGetTextureParameterIuivEXT");
	gl->getTextureParameterfvEXT						= (glGetTextureParameterfvEXTFunc)							loader->get("glGetTextureParameterfvEXT");
	gl->getTextureParameterivEXT						= (glGetTextureParameterivEXTFunc)							loader->get("glGetTextureParameterivEXT");
	gl->getVertexArrayIntegeri_vEXT						= (glGetVertexArrayIntegeri_vEXTFunc)						loader->get("glGetVertexArrayIntegeri_vEXT");
	gl->getVertexArrayIntegervEXT						= (glGetVertexArrayIntegervEXTFunc)							loader->get("glGetVertexArrayIntegervEXT");
	gl->getVertexArrayPointeri_vEXT						= (glGetVertexArrayPointeri_vEXTFunc)						loader->get("glGetVertexArrayPointeri_vEXT");
	gl->getVertexArrayPointervEXT						= (glGetVertexArrayPointervEXTFunc)							loader->get("glGetVertexArrayPointervEXT");
	gl->isEnabledi										= (glIsEnablediFunc)										loader->get("glIsEnabledIndexedEXT");
	gl->mapNamedBufferEXT								= (glMapNamedBufferEXTFunc)									loader->get("glMapNamedBufferEXT");
	gl->mapNamedBufferRangeEXT							= (glMapNamedBufferRangeEXTFunc)							loader->get("glMapNamedBufferRangeEXT");
	gl->matrixFrustumEXT								= (glMatrixFrustumEXTFunc)									loader->get("glMatrixFrustumEXT");
	gl->matrixLoadIdentityEXT							= (glMatrixLoadIdentityEXTFunc)								loader->get("glMatrixLoadIdentityEXT");
	gl->matrixLoadTransposedEXT							= (glMatrixLoadTransposedEXTFunc)							loader->get("glMatrixLoadTransposedEXT");
	gl->matrixLoadTransposefEXT							= (glMatrixLoadTransposefEXTFunc)							loader->get("glMatrixLoadTransposefEXT");
	gl->matrixLoaddEXT									= (glMatrixLoaddEXTFunc)									loader->get("glMatrixLoaddEXT");
	gl->matrixLoadfEXT									= (glMatrixLoadfEXTFunc)									loader->get("glMatrixLoadfEXT");
	gl->matrixMultTransposedEXT							= (glMatrixMultTransposedEXTFunc)							loader->get("glMatrixMultTransposedEXT");
	gl->matrixMultTransposefEXT							= (glMatrixMultTransposefEXTFunc)							loader->get("glMatrixMultTransposefEXT");
	gl->matrixMultdEXT									= (glMatrixMultdEXTFunc)									loader->get("glMatrixMultdEXT");
	gl->matrixMultfEXT									= (glMatrixMultfEXTFunc)									loader->get("glMatrixMultfEXT");
	gl->matrixOrthoEXT									= (glMatrixOrthoEXTFunc)									loader->get("glMatrixOrthoEXT");
	gl->matrixPopEXT									= (glMatrixPopEXTFunc)										loader->get("glMatrixPopEXT");
	gl->matrixPushEXT									= (glMatrixPushEXTFunc)										loader->get("glMatrixPushEXT");
	gl->matrixRotatedEXT								= (glMatrixRotatedEXTFunc)									loader->get("glMatrixRotatedEXT");
	gl->matrixRotatefEXT								= (glMatrixRotatefEXTFunc)									loader->get("glMatrixRotatefEXT");
	gl->matrixScaledEXT									= (glMatrixScaledEXTFunc)									loader->get("glMatrixScaledEXT");
	gl->matrixScalefEXT									= (glMatrixScalefEXTFunc)									loader->get("glMatrixScalefEXT");
	gl->matrixTranslatedEXT								= (glMatrixTranslatedEXTFunc)								loader->get("glMatrixTranslatedEXT");
	gl->matrixTranslatefEXT								= (glMatrixTranslatefEXTFunc)								loader->get("glMatrixTranslatefEXT");
	gl->multiTexBufferEXT								= (glMultiTexBufferEXTFunc)									loader->get("glMultiTexBufferEXT");
	gl->multiTexCoordPointerEXT							= (glMultiTexCoordPointerEXTFunc)							loader->get("glMultiTexCoordPointerEXT");
	gl->multiTexEnvfEXT									= (glMultiTexEnvfEXTFunc)									loader->get("glMultiTexEnvfEXT");
	gl->multiTexEnvfvEXT								= (glMultiTexEnvfvEXTFunc)									loader->get("glMultiTexEnvfvEXT");
	gl->multiTexEnviEXT									= (glMultiTexEnviEXTFunc)									loader->get("glMultiTexEnviEXT");
	gl->multiTexEnvivEXT								= (glMultiTexEnvivEXTFunc)									loader->get("glMultiTexEnvivEXT");
	gl->multiTexGendEXT									= (glMultiTexGendEXTFunc)									loader->get("glMultiTexGendEXT");
	gl->multiTexGendvEXT								= (glMultiTexGendvEXTFunc)									loader->get("glMultiTexGendvEXT");
	gl->multiTexGenfEXT									= (glMultiTexGenfEXTFunc)									loader->get("glMultiTexGenfEXT");
	gl->multiTexGenfvEXT								= (glMultiTexGenfvEXTFunc)									loader->get("glMultiTexGenfvEXT");
	gl->multiTexGeniEXT									= (glMultiTexGeniEXTFunc)									loader->get("glMultiTexGeniEXT");
	gl->multiTexGenivEXT								= (glMultiTexGenivEXTFunc)									loader->get("glMultiTexGenivEXT");
	gl->multiTexImage1DEXT								= (glMultiTexImage1DEXTFunc)								loader->get("glMultiTexImage1DEXT");
	gl->multiTexImage2DEXT								= (glMultiTexImage2DEXTFunc)								loader->get("glMultiTexImage2DEXT");
	gl->multiTexImage3DEXT								= (glMultiTexImage3DEXTFunc)								loader->get("glMultiTexImage3DEXT");
	gl->multiTexParameterIivEXT							= (glMultiTexParameterIivEXTFunc)							loader->get("glMultiTexParameterIivEXT");
	gl->multiTexParameterIuivEXT						= (glMultiTexParameterIuivEXTFunc)							loader->get("glMultiTexParameterIuivEXT");
	gl->multiTexParameterfEXT							= (glMultiTexParameterfEXTFunc)								loader->get("glMultiTexParameterfEXT");
	gl->multiTexParameterfvEXT							= (glMultiTexParameterfvEXTFunc)							loader->get("glMultiTexParameterfvEXT");
	gl->multiTexParameteriEXT							= (glMultiTexParameteriEXTFunc)								loader->get("glMultiTexParameteriEXT");
	gl->multiTexParameterivEXT							= (glMultiTexParameterivEXTFunc)							loader->get("glMultiTexParameterivEXT");
	gl->multiTexRenderbufferEXT							= (glMultiTexRenderbufferEXTFunc)							loader->get("glMultiTexRenderbufferEXT");
	gl->multiTexSubImage1DEXT							= (glMultiTexSubImage1DEXTFunc)								loader->get("glMultiTexSubImage1DEXT");
	gl->multiTexSubImage2DEXT							= (glMultiTexSubImage2DEXTFunc)								loader->get("glMultiTexSubImage2DEXT");
	gl->multiTexSubImage3DEXT							= (glMultiTexSubImage3DEXTFunc)								loader->get("glMultiTexSubImage3DEXT");
	gl->namedBufferDataEXT								= (glNamedBufferDataEXTFunc)								loader->get("glNamedBufferDataEXT");
	gl->namedBufferStorage								= (glNamedBufferStorageFunc)								loader->get("glNamedBufferStorageEXT");
	gl->namedBufferSubData								= (glNamedBufferSubDataFunc)								loader->get("glNamedBufferSubDataEXT");
	gl->namedCopyBufferSubDataEXT						= (glNamedCopyBufferSubDataEXTFunc)							loader->get("glNamedCopyBufferSubDataEXT");
	gl->namedFramebufferParameteriEXT					= (glNamedFramebufferParameteriEXTFunc)						loader->get("glNamedFramebufferParameteriEXT");
	gl->namedFramebufferRenderbufferEXT					= (glNamedFramebufferRenderbufferEXTFunc)					loader->get("glNamedFramebufferRenderbufferEXT");
	gl->namedFramebufferTexture1DEXT					= (glNamedFramebufferTexture1DEXTFunc)						loader->get("glNamedFramebufferTexture1DEXT");
	gl->namedFramebufferTexture2DEXT					= (glNamedFramebufferTexture2DEXTFunc)						loader->get("glNamedFramebufferTexture2DEXT");
	gl->namedFramebufferTexture3DEXT					= (glNamedFramebufferTexture3DEXTFunc)						loader->get("glNamedFramebufferTexture3DEXT");
	gl->namedFramebufferTextureEXT						= (glNamedFramebufferTextureEXTFunc)						loader->get("glNamedFramebufferTextureEXT");
	gl->namedFramebufferTextureFaceEXT					= (glNamedFramebufferTextureFaceEXTFunc)					loader->get("glNamedFramebufferTextureFaceEXT");
	gl->namedFramebufferTextureLayerEXT					= (glNamedFramebufferTextureLayerEXTFunc)					loader->get("glNamedFramebufferTextureLayerEXT");
	gl->namedProgramLocalParameter4dEXT					= (glNamedProgramLocalParameter4dEXTFunc)					loader->get("glNamedProgramLocalParameter4dEXT");
	gl->namedProgramLocalParameter4dvEXT				= (glNamedProgramLocalParameter4dvEXTFunc)					loader->get("glNamedProgramLocalParameter4dvEXT");
	gl->namedProgramLocalParameter4fEXT					= (glNamedProgramLocalParameter4fEXTFunc)					loader->get("glNamedProgramLocalParameter4fEXT");
	gl->namedProgramLocalParameter4fvEXT				= (glNamedProgramLocalParameter4fvEXTFunc)					loader->get("glNamedProgramLocalParameter4fvEXT");
	gl->namedProgramLocalParameterI4iEXT				= (glNamedProgramLocalParameterI4iEXTFunc)					loader->get("glNamedProgramLocalParameterI4iEXT");
	gl->namedProgramLocalParameterI4ivEXT				= (glNamedProgramLocalParameterI4ivEXTFunc)					loader->get("glNamedProgramLocalParameterI4ivEXT");
	gl->namedProgramLocalParameterI4uiEXT				= (glNamedProgramLocalParameterI4uiEXTFunc)					loader->get("glNamedProgramLocalParameterI4uiEXT");
	gl->namedProgramLocalParameterI4uivEXT				= (glNamedProgramLocalParameterI4uivEXTFunc)				loader->get("glNamedProgramLocalParameterI4uivEXT");
	gl->namedProgramLocalParameters4fvEXT				= (glNamedProgramLocalParameters4fvEXTFunc)					loader->get("glNamedProgramLocalParameters4fvEXT");
	gl->namedProgramLocalParametersI4ivEXT				= (glNamedProgramLocalParametersI4ivEXTFunc)				loader->get("glNamedProgramLocalParametersI4ivEXT");
	gl->namedProgramLocalParametersI4uivEXT				= (glNamedProgramLocalParametersI4uivEXTFunc)				loader->get("glNamedProgramLocalParametersI4uivEXT");
	gl->namedProgramStringEXT							= (glNamedProgramStringEXTFunc)								loader->get("glNamedProgramStringEXT");
	gl->namedRenderbufferStorageEXT						= (glNamedRenderbufferStorageEXTFunc)						loader->get("glNamedRenderbufferStorageEXT");
	gl->namedRenderbufferStorageMultisampleCoverageEXT	= (glNamedRenderbufferStorageMultisampleCoverageEXTFunc)	loader->get("glNamedRenderbufferStorageMultisampleCoverageEXT");
	gl->namedRenderbufferStorageMultisampleEXT			= (glNamedRenderbufferStorageMultisampleEXTFunc)			loader->get("glNamedRenderbufferStorageMultisampleEXT");
	gl->programUniform1dEXT								= (glProgramUniform1dEXTFunc)								loader->get("glProgramUniform1dEXT");
	gl->programUniform1dvEXT							= (glProgramUniform1dvEXTFunc)								loader->get("glProgramUniform1dvEXT");
	gl->programUniform1f								= (glProgramUniform1fFunc)									loader->get("glProgramUniform1fEXT");
	gl->programUniform1fv								= (glProgramUniform1fvFunc)									loader->get("glProgramUniform1fvEXT");
	gl->programUniform1i								= (glProgramUniform1iFunc)									loader->get("glProgramUniform1iEXT");
	gl->programUniform1iv								= (glProgramUniform1ivFunc)									loader->get("glProgramUniform1ivEXT");
	gl->programUniform1ui								= (glProgramUniform1uiFunc)									loader->get("glProgramUniform1uiEXT");
	gl->programUniform1uiv								= (glProgramUniform1uivFunc)								loader->get("glProgramUniform1uivEXT");
	gl->programUniform2dEXT								= (glProgramUniform2dEXTFunc)								loader->get("glProgramUniform2dEXT");
	gl->programUniform2dvEXT							= (glProgramUniform2dvEXTFunc)								loader->get("glProgramUniform2dvEXT");
	gl->programUniform2f								= (glProgramUniform2fFunc)									loader->get("glProgramUniform2fEXT");
	gl->programUniform2fv								= (glProgramUniform2fvFunc)									loader->get("glProgramUniform2fvEXT");
	gl->programUniform2i								= (glProgramUniform2iFunc)									loader->get("glProgramUniform2iEXT");
	gl->programUniform2iv								= (glProgramUniform2ivFunc)									loader->get("glProgramUniform2ivEXT");
	gl->programUniform2ui								= (glProgramUniform2uiFunc)									loader->get("glProgramUniform2uiEXT");
	gl->programUniform2uiv								= (glProgramUniform2uivFunc)								loader->get("glProgramUniform2uivEXT");
	gl->programUniform3dEXT								= (glProgramUniform3dEXTFunc)								loader->get("glProgramUniform3dEXT");
	gl->programUniform3dvEXT							= (glProgramUniform3dvEXTFunc)								loader->get("glProgramUniform3dvEXT");
	gl->programUniform3f								= (glProgramUniform3fFunc)									loader->get("glProgramUniform3fEXT");
	gl->programUniform3fv								= (glProgramUniform3fvFunc)									loader->get("glProgramUniform3fvEXT");
	gl->programUniform3i								= (glProgramUniform3iFunc)									loader->get("glProgramUniform3iEXT");
	gl->programUniform3iv								= (glProgramUniform3ivFunc)									loader->get("glProgramUniform3ivEXT");
	gl->programUniform3ui								= (glProgramUniform3uiFunc)									loader->get("glProgramUniform3uiEXT");
	gl->programUniform3uiv								= (glProgramUniform3uivFunc)								loader->get("glProgramUniform3uivEXT");
	gl->programUniform4dEXT								= (glProgramUniform4dEXTFunc)								loader->get("glProgramUniform4dEXT");
	gl->programUniform4dvEXT							= (glProgramUniform4dvEXTFunc)								loader->get("glProgramUniform4dvEXT");
	gl->programUniform4f								= (glProgramUniform4fFunc)									loader->get("glProgramUniform4fEXT");
	gl->programUniform4fv								= (glProgramUniform4fvFunc)									loader->get("glProgramUniform4fvEXT");
	gl->programUniform4i								= (glProgramUniform4iFunc)									loader->get("glProgramUniform4iEXT");
	gl->programUniform4iv								= (glProgramUniform4ivFunc)									loader->get("glProgramUniform4ivEXT");
	gl->programUniform4ui								= (glProgramUniform4uiFunc)									loader->get("glProgramUniform4uiEXT");
	gl->programUniform4uiv								= (glProgramUniform4uivFunc)								loader->get("glProgramUniform4uivEXT");
	gl->programUniformMatrix2dvEXT						= (glProgramUniformMatrix2dvEXTFunc)						loader->get("glProgramUniformMatrix2dvEXT");
	gl->programUniformMatrix2fv							= (glProgramUniformMatrix2fvFunc)							loader->get("glProgramUniformMatrix2fvEXT");
	gl->programUniformMatrix2x3dvEXT					= (glProgramUniformMatrix2x3dvEXTFunc)						loader->get("glProgramUniformMatrix2x3dvEXT");
	gl->programUniformMatrix2x3fv						= (glProgramUniformMatrix2x3fvFunc)							loader->get("glProgramUniformMatrix2x3fvEXT");
	gl->programUniformMatrix2x4dvEXT					= (glProgramUniformMatrix2x4dvEXTFunc)						loader->get("glProgramUniformMatrix2x4dvEXT");
	gl->programUniformMatrix2x4fv						= (glProgramUniformMatrix2x4fvFunc)							loader->get("glProgramUniformMatrix2x4fvEXT");
	gl->programUniformMatrix3dvEXT						= (glProgramUniformMatrix3dvEXTFunc)						loader->get("glProgramUniformMatrix3dvEXT");
	gl->programUniformMatrix3fv							= (glProgramUniformMatrix3fvFunc)							loader->get("glProgramUniformMatrix3fvEXT");
	gl->programUniformMatrix3x2dvEXT					= (glProgramUniformMatrix3x2dvEXTFunc)						loader->get("glProgramUniformMatrix3x2dvEXT");
	gl->programUniformMatrix3x2fv						= (glProgramUniformMatrix3x2fvFunc)							loader->get("glProgramUniformMatrix3x2fvEXT");
	gl->programUniformMatrix3x4dvEXT					= (glProgramUniformMatrix3x4dvEXTFunc)						loader->get("glProgramUniformMatrix3x4dvEXT");
	gl->programUniformMatrix3x4fv						= (glProgramUniformMatrix3x4fvFunc)							loader->get("glProgramUniformMatrix3x4fvEXT");
	gl->programUniformMatrix4dvEXT						= (glProgramUniformMatrix4dvEXTFunc)						loader->get("glProgramUniformMatrix4dvEXT");
	gl->programUniformMatrix4fv							= (glProgramUniformMatrix4fvFunc)							loader->get("glProgramUniformMatrix4fvEXT");
	gl->programUniformMatrix4x2dvEXT					= (glProgramUniformMatrix4x2dvEXTFunc)						loader->get("glProgramUniformMatrix4x2dvEXT");
	gl->programUniformMatrix4x2fv						= (glProgramUniformMatrix4x2fvFunc)							loader->get("glProgramUniformMatrix4x2fvEXT");
	gl->programUniformMatrix4x3dvEXT					= (glProgramUniformMatrix4x3dvEXTFunc)						loader->get("glProgramUniformMatrix4x3dvEXT");
	gl->programUniformMatrix4x3fv						= (glProgramUniformMatrix4x3fvFunc)							loader->get("glProgramUniformMatrix4x3fvEXT");
	gl->pushClientAttribDefaultEXT						= (glPushClientAttribDefaultEXTFunc)						loader->get("glPushClientAttribDefaultEXT");
	gl->textureBufferEXT								= (glTextureBufferEXTFunc)									loader->get("glTextureBufferEXT");
	gl->textureBufferRangeEXT							= (glTextureBufferRangeEXTFunc)								loader->get("glTextureBufferRangeEXT");
	gl->textureImage1DEXT								= (glTextureImage1DEXTFunc)									loader->get("glTextureImage1DEXT");
	gl->textureImage2DEXT								= (glTextureImage2DEXTFunc)									loader->get("glTextureImage2DEXT");
	gl->textureImage3DEXT								= (glTextureImage3DEXTFunc)									loader->get("glTextureImage3DEXT");
	gl->texturePageCommitmentEXT						= (glTexturePageCommitmentEXTFunc)							loader->get("glTexturePageCommitmentEXT");
	gl->textureParameterIivEXT							= (glTextureParameterIivEXTFunc)							loader->get("glTextureParameterIivEXT");
	gl->textureParameterIuivEXT							= (glTextureParameterIuivEXTFunc)							loader->get("glTextureParameterIuivEXT");
	gl->textureParameterfEXT							= (glTextureParameterfEXTFunc)								loader->get("glTextureParameterfEXT");
	gl->textureParameterfvEXT							= (glTextureParameterfvEXTFunc)								loader->get("glTextureParameterfvEXT");
	gl->textureParameteriEXT							= (glTextureParameteriEXTFunc)								loader->get("glTextureParameteriEXT");
	gl->textureParameterivEXT							= (glTextureParameterivEXTFunc)								loader->get("glTextureParameterivEXT");
	gl->textureRenderbufferEXT							= (glTextureRenderbufferEXTFunc)							loader->get("glTextureRenderbufferEXT");
	gl->textureStorage1DEXT								= (glTextureStorage1DEXTFunc)								loader->get("glTextureStorage1DEXT");
	gl->textureStorage2DEXT								= (glTextureStorage2DEXTFunc)								loader->get("glTextureStorage2DEXT");
	gl->textureStorage2DMultisampleEXT					= (glTextureStorage2DMultisampleEXTFunc)					loader->get("glTextureStorage2DMultisampleEXT");
	gl->textureStorage3DEXT								= (glTextureStorage3DEXTFunc)								loader->get("glTextureStorage3DEXT");
	gl->textureStorage3DMultisampleEXT					= (glTextureStorage3DMultisampleEXTFunc)					loader->get("glTextureStorage3DMultisampleEXT");
	gl->textureSubImage1DEXT							= (glTextureSubImage1DEXTFunc)								loader->get("glTextureSubImage1DEXT");
	gl->textureSubImage2DEXT							= (glTextureSubImage2DEXTFunc)								loader->get("glTextureSubImage2DEXT");
	gl->textureSubImage3DEXT							= (glTextureSubImage3DEXTFunc)								loader->get("glTextureSubImage3DEXT");
	gl->unmapNamedBufferEXT								= (glUnmapNamedBufferEXTFunc)								loader->get("glUnmapNamedBufferEXT");
	gl->vertexArrayBindVertexBufferEXT					= (glVertexArrayBindVertexBufferEXTFunc)					loader->get("glVertexArrayBindVertexBufferEXT");
	gl->vertexArrayColorOffsetEXT						= (glVertexArrayColorOffsetEXTFunc)							loader->get("glVertexArrayColorOffsetEXT");
	gl->vertexArrayEdgeFlagOffsetEXT					= (glVertexArrayEdgeFlagOffsetEXTFunc)						loader->get("glVertexArrayEdgeFlagOffsetEXT");
	gl->vertexArrayFogCoordOffsetEXT					= (glVertexArrayFogCoordOffsetEXTFunc)						loader->get("glVertexArrayFogCoordOffsetEXT");
	gl->vertexArrayIndexOffsetEXT						= (glVertexArrayIndexOffsetEXTFunc)							loader->get("glVertexArrayIndexOffsetEXT");
	gl->vertexArrayMultiTexCoordOffsetEXT				= (glVertexArrayMultiTexCoordOffsetEXTFunc)					loader->get("glVertexArrayMultiTexCoordOffsetEXT");
	gl->vertexArrayNormalOffsetEXT						= (glVertexArrayNormalOffsetEXTFunc)						loader->get("glVertexArrayNormalOffsetEXT");
	gl->vertexArraySecondaryColorOffsetEXT				= (glVertexArraySecondaryColorOffsetEXTFunc)				loader->get("glVertexArraySecondaryColorOffsetEXT");
	gl->vertexArrayTexCoordOffsetEXT					= (glVertexArrayTexCoordOffsetEXTFunc)						loader->get("glVertexArrayTexCoordOffsetEXT");
	gl->vertexArrayVertexAttribBindingEXT				= (glVertexArrayVertexAttribBindingEXTFunc)					loader->get("glVertexArrayVertexAttribBindingEXT");
	gl->vertexArrayVertexAttribDivisorEXT				= (glVertexArrayVertexAttribDivisorEXTFunc)					loader->get("glVertexArrayVertexAttribDivisorEXT");
	gl->vertexArrayVertexAttribFormatEXT				= (glVertexArrayVertexAttribFormatEXTFunc)					loader->get("glVertexArrayVertexAttribFormatEXT");
	gl->vertexArrayVertexAttribIFormatEXT				= (glVertexArrayVertexAttribIFormatEXTFunc)					loader->get("glVertexArrayVertexAttribIFormatEXT");
	gl->vertexArrayVertexAttribIOffsetEXT				= (glVertexArrayVertexAttribIOffsetEXTFunc)					loader->get("glVertexArrayVertexAttribIOffsetEXT");
	gl->vertexArrayVertexAttribLFormatEXT				= (glVertexArrayVertexAttribLFormatEXTFunc)					loader->get("glVertexArrayVertexAttribLFormatEXT");
	gl->vertexArrayVertexAttribLOffsetEXT				= (glVertexArrayVertexAttribLOffsetEXTFunc)					loader->get("glVertexArrayVertexAttribLOffsetEXT");
	gl->vertexArrayVertexAttribOffsetEXT				= (glVertexArrayVertexAttribOffsetEXTFunc)					loader->get("glVertexArrayVertexAttribOffsetEXT");
	gl->vertexArrayVertexBindingDivisorEXT				= (glVertexArrayVertexBindingDivisorEXTFunc)				loader->get("glVertexArrayVertexBindingDivisorEXT");
	gl->vertexArrayVertexOffsetEXT						= (glVertexArrayVertexOffsetEXTFunc)						loader->get("glVertexArrayVertexOffsetEXT");
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

if (de::contains(extSet, "GL_ARB_clip_control"))
{
	gl->clipControl	= (glClipControlFunc)	loader->get("glClipControl");
}

if (de::contains(extSet, "GL_ARB_buffer_storage"))
{
	gl->bufferStorage	= (glBufferStorageFunc)	loader->get("glBufferStorage");
}

if (de::contains(extSet, "GL_ARB_compute_shader"))
{
	gl->dispatchCompute			= (glDispatchComputeFunc)			loader->get("glDispatchCompute");
	gl->dispatchComputeIndirect	= (glDispatchComputeIndirectFunc)	loader->get("glDispatchComputeIndirect");
}

if (de::contains(extSet, "GL_ARB_draw_indirect"))
{
	gl->drawArraysIndirect		= (glDrawArraysIndirectFunc)	loader->get("glDrawArraysIndirect");
	gl->drawElementsIndirect	= (glDrawElementsIndirectFunc)	loader->get("glDrawElementsIndirect");
}

if (de::contains(extSet, "GL_ARB_draw_instanced"))
{
	gl->drawArraysInstanced		= (glDrawArraysInstancedFunc)	loader->get("glDrawArraysInstancedARB");
	gl->drawElementsInstanced	= (glDrawElementsInstancedFunc)	loader->get("glDrawElementsInstancedARB");
}

if (de::contains(extSet, "GL_ARB_draw_elements_base_vertex"))
{
	gl->drawElementsBaseVertex			= (glDrawElementsBaseVertexFunc)			loader->get("glDrawElementsBaseVertex");
	gl->drawElementsInstancedBaseVertex	= (glDrawElementsInstancedBaseVertexFunc)	loader->get("glDrawElementsInstancedBaseVertex");
	gl->drawRangeElementsBaseVertex		= (glDrawRangeElementsBaseVertexFunc)		loader->get("glDrawRangeElementsBaseVertex");
	gl->multiDrawElementsBaseVertex		= (glMultiDrawElementsBaseVertexFunc)		loader->get("glMultiDrawElementsBaseVertex");
}

if (de::contains(extSet, "GL_ARB_direct_state_access"))
{
	gl->bindTextureUnit								= (glBindTextureUnitFunc)							loader->get("glBindTextureUnit");
	gl->blitNamedFramebuffer						= (glBlitNamedFramebufferFunc)						loader->get("glBlitNamedFramebuffer");
	gl->checkNamedFramebufferStatus					= (glCheckNamedFramebufferStatusFunc)				loader->get("glCheckNamedFramebufferStatus");
	gl->clearNamedBufferData						= (glClearNamedBufferDataFunc)						loader->get("glClearNamedBufferData");
	gl->clearNamedBufferSubData						= (glClearNamedBufferSubDataFunc)					loader->get("glClearNamedBufferSubData");
	gl->clearNamedFramebufferfi						= (glClearNamedFramebufferfiFunc)					loader->get("glClearNamedFramebufferfi");
	gl->clearNamedFramebufferfv						= (glClearNamedFramebufferfvFunc)					loader->get("glClearNamedFramebufferfv");
	gl->clearNamedFramebufferiv						= (glClearNamedFramebufferivFunc)					loader->get("glClearNamedFramebufferiv");
	gl->clearNamedFramebufferuiv					= (glClearNamedFramebufferuivFunc)					loader->get("glClearNamedFramebufferuiv");
	gl->compressedTextureSubImage1D					= (glCompressedTextureSubImage1DFunc)				loader->get("glCompressedTextureSubImage1D");
	gl->compressedTextureSubImage2D					= (glCompressedTextureSubImage2DFunc)				loader->get("glCompressedTextureSubImage2D");
	gl->compressedTextureSubImage3D					= (glCompressedTextureSubImage3DFunc)				loader->get("glCompressedTextureSubImage3D");
	gl->copyNamedBufferSubData						= (glCopyNamedBufferSubDataFunc)					loader->get("glCopyNamedBufferSubData");
	gl->copyTextureSubImage1D						= (glCopyTextureSubImage1DFunc)						loader->get("glCopyTextureSubImage1D");
	gl->copyTextureSubImage2D						= (glCopyTextureSubImage2DFunc)						loader->get("glCopyTextureSubImage2D");
	gl->copyTextureSubImage3D						= (glCopyTextureSubImage3DFunc)						loader->get("glCopyTextureSubImage3D");
	gl->createBuffers								= (glCreateBuffersFunc)								loader->get("glCreateBuffers");
	gl->createFramebuffers							= (glCreateFramebuffersFunc)						loader->get("glCreateFramebuffers");
	gl->createProgramPipelines						= (glCreateProgramPipelinesFunc)					loader->get("glCreateProgramPipelines");
	gl->createQueries								= (glCreateQueriesFunc)								loader->get("glCreateQueries");
	gl->createRenderbuffers							= (glCreateRenderbuffersFunc)						loader->get("glCreateRenderbuffers");
	gl->createSamplers								= (glCreateSamplersFunc)							loader->get("glCreateSamplers");
	gl->createTextures								= (glCreateTexturesFunc)							loader->get("glCreateTextures");
	gl->createTransformFeedbacks					= (glCreateTransformFeedbacksFunc)					loader->get("glCreateTransformFeedbacks");
	gl->createVertexArrays							= (glCreateVertexArraysFunc)						loader->get("glCreateVertexArrays");
	gl->disableVertexArrayAttrib					= (glDisableVertexArrayAttribFunc)					loader->get("glDisableVertexArrayAttrib");
	gl->enableVertexArrayAttrib						= (glEnableVertexArrayAttribFunc)					loader->get("glEnableVertexArrayAttrib");
	gl->flushMappedNamedBufferRange					= (glFlushMappedNamedBufferRangeFunc)				loader->get("glFlushMappedNamedBufferRange");
	gl->generateTextureMipmap						= (glGenerateTextureMipmapFunc)						loader->get("glGenerateTextureMipmap");
	gl->getCompressedTextureImage					= (glGetCompressedTextureImageFunc)					loader->get("glGetCompressedTextureImage");
	gl->getNamedBufferParameteri64v					= (glGetNamedBufferParameteri64vFunc)				loader->get("glGetNamedBufferParameteri64v");
	gl->getNamedBufferParameteriv					= (glGetNamedBufferParameterivFunc)					loader->get("glGetNamedBufferParameteriv");
	gl->getNamedBufferPointerv						= (glGetNamedBufferPointervFunc)					loader->get("glGetNamedBufferPointerv");
	gl->getNamedBufferSubData						= (glGetNamedBufferSubDataFunc)						loader->get("glGetNamedBufferSubData");
	gl->getNamedFramebufferAttachmentParameteriv	= (glGetNamedFramebufferAttachmentParameterivFunc)	loader->get("glGetNamedFramebufferAttachmentParameteriv");
	gl->getNamedFramebufferParameteriv				= (glGetNamedFramebufferParameterivFunc)			loader->get("glGetNamedFramebufferParameteriv");
	gl->getNamedRenderbufferParameteriv				= (glGetNamedRenderbufferParameterivFunc)			loader->get("glGetNamedRenderbufferParameteriv");
	gl->getQueryBufferObjecti64v					= (glGetQueryBufferObjecti64vFunc)					loader->get("glGetQueryBufferObjecti64v");
	gl->getQueryBufferObjectiv						= (glGetQueryBufferObjectivFunc)					loader->get("glGetQueryBufferObjectiv");
	gl->getQueryBufferObjectui64v					= (glGetQueryBufferObjectui64vFunc)					loader->get("glGetQueryBufferObjectui64v");
	gl->getQueryBufferObjectuiv						= (glGetQueryBufferObjectuivFunc)					loader->get("glGetQueryBufferObjectuiv");
	gl->getTextureImage								= (glGetTextureImageFunc)							loader->get("glGetTextureImage");
	gl->getTextureLevelParameterfv					= (glGetTextureLevelParameterfvFunc)				loader->get("glGetTextureLevelParameterfv");
	gl->getTextureLevelParameteriv					= (glGetTextureLevelParameterivFunc)				loader->get("glGetTextureLevelParameteriv");
	gl->getTextureParameterIiv						= (glGetTextureParameterIivFunc)					loader->get("glGetTextureParameterIiv");
	gl->getTextureParameterIuiv						= (glGetTextureParameterIuivFunc)					loader->get("glGetTextureParameterIuiv");
	gl->getTextureParameterfv						= (glGetTextureParameterfvFunc)						loader->get("glGetTextureParameterfv");
	gl->getTextureParameteriv						= (glGetTextureParameterivFunc)						loader->get("glGetTextureParameteriv");
	gl->getTransformFeedbacki64_v					= (glGetTransformFeedbacki64_vFunc)					loader->get("glGetTransformFeedbacki64_v");
	gl->getTransformFeedbacki_v						= (glGetTransformFeedbacki_vFunc)					loader->get("glGetTransformFeedbacki_v");
	gl->getTransformFeedbackiv						= (glGetTransformFeedbackivFunc)					loader->get("glGetTransformFeedbackiv");
	gl->getVertexArrayIndexed64iv					= (glGetVertexArrayIndexed64ivFunc)					loader->get("glGetVertexArrayIndexed64iv");
	gl->getVertexArrayIndexediv						= (glGetVertexArrayIndexedivFunc)					loader->get("glGetVertexArrayIndexediv");
	gl->getVertexArrayiv							= (glGetVertexArrayivFunc)							loader->get("glGetVertexArrayiv");
	gl->invalidateNamedFramebufferData				= (glInvalidateNamedFramebufferDataFunc)			loader->get("glInvalidateNamedFramebufferData");
	gl->invalidateNamedFramebufferSubData			= (glInvalidateNamedFramebufferSubDataFunc)			loader->get("glInvalidateNamedFramebufferSubData");
	gl->mapNamedBuffer								= (glMapNamedBufferFunc)							loader->get("glMapNamedBuffer");
	gl->mapNamedBufferRange							= (glMapNamedBufferRangeFunc)						loader->get("glMapNamedBufferRange");
	gl->namedBufferData								= (glNamedBufferDataFunc)							loader->get("glNamedBufferData");
	gl->namedBufferStorage							= (glNamedBufferStorageFunc)						loader->get("glNamedBufferStorage");
	gl->namedBufferSubData							= (glNamedBufferSubDataFunc)						loader->get("glNamedBufferSubData");
	gl->namedFramebufferDrawBuffer					= (glNamedFramebufferDrawBufferFunc)				loader->get("glNamedFramebufferDrawBuffer");
	gl->namedFramebufferDrawBuffers					= (glNamedFramebufferDrawBuffersFunc)				loader->get("glNamedFramebufferDrawBuffers");
	gl->namedFramebufferParameteri					= (glNamedFramebufferParameteriFunc)				loader->get("glNamedFramebufferParameteri");
	gl->namedFramebufferReadBuffer					= (glNamedFramebufferReadBufferFunc)				loader->get("glNamedFramebufferReadBuffer");
	gl->namedFramebufferRenderbuffer				= (glNamedFramebufferRenderbufferFunc)				loader->get("glNamedFramebufferRenderbuffer");
	gl->namedFramebufferTexture						= (glNamedFramebufferTextureFunc)					loader->get("glNamedFramebufferTexture");
	gl->namedFramebufferTextureLayer				= (glNamedFramebufferTextureLayerFunc)				loader->get("glNamedFramebufferTextureLayer");
	gl->namedRenderbufferStorage					= (glNamedRenderbufferStorageFunc)					loader->get("glNamedRenderbufferStorage");
	gl->namedRenderbufferStorageMultisample			= (glNamedRenderbufferStorageMultisampleFunc)		loader->get("glNamedRenderbufferStorageMultisample");
	gl->textureBuffer								= (glTextureBufferFunc)								loader->get("glTextureBuffer");
	gl->textureBufferRange							= (glTextureBufferRangeFunc)						loader->get("glTextureBufferRange");
	gl->textureParameterIiv							= (glTextureParameterIivFunc)						loader->get("glTextureParameterIiv");
	gl->textureParameterIuiv						= (glTextureParameterIuivFunc)						loader->get("glTextureParameterIuiv");
	gl->textureParameterf							= (glTextureParameterfFunc)							loader->get("glTextureParameterf");
	gl->textureParameterfv							= (glTextureParameterfvFunc)						loader->get("glTextureParameterfv");
	gl->textureParameteri							= (glTextureParameteriFunc)							loader->get("glTextureParameteri");
	gl->textureParameteriv							= (glTextureParameterivFunc)						loader->get("glTextureParameteriv");
	gl->textureStorage1D							= (glTextureStorage1DFunc)							loader->get("glTextureStorage1D");
	gl->textureStorage2D							= (glTextureStorage2DFunc)							loader->get("glTextureStorage2D");
	gl->textureStorage2DMultisample					= (glTextureStorage2DMultisampleFunc)				loader->get("glTextureStorage2DMultisample");
	gl->textureStorage3D							= (glTextureStorage3DFunc)							loader->get("glTextureStorage3D");
	gl->textureStorage3DMultisample					= (glTextureStorage3DMultisampleFunc)				loader->get("glTextureStorage3DMultisample");
	gl->textureSubImage1D							= (glTextureSubImage1DFunc)							loader->get("glTextureSubImage1D");
	gl->textureSubImage2D							= (glTextureSubImage2DFunc)							loader->get("glTextureSubImage2D");
	gl->textureSubImage3D							= (glTextureSubImage3DFunc)							loader->get("glTextureSubImage3D");
	gl->transformFeedbackBufferBase					= (glTransformFeedbackBufferBaseFunc)				loader->get("glTransformFeedbackBufferBase");
	gl->transformFeedbackBufferRange				= (glTransformFeedbackBufferRangeFunc)				loader->get("glTransformFeedbackBufferRange");
	gl->unmapNamedBuffer							= (glUnmapNamedBufferFunc)							loader->get("glUnmapNamedBuffer");
	gl->vertexArrayAttribBinding					= (glVertexArrayAttribBindingFunc)					loader->get("glVertexArrayAttribBinding");
	gl->vertexArrayAttribFormat						= (glVertexArrayAttribFormatFunc)					loader->get("glVertexArrayAttribFormat");
	gl->vertexArrayAttribIFormat					= (glVertexArrayAttribIFormatFunc)					loader->get("glVertexArrayAttribIFormat");
	gl->vertexArrayAttribLFormat					= (glVertexArrayAttribLFormatFunc)					loader->get("glVertexArrayAttribLFormat");
	gl->vertexArrayBindingDivisor					= (glVertexArrayBindingDivisorFunc)					loader->get("glVertexArrayBindingDivisor");
	gl->vertexArrayElementBuffer					= (glVertexArrayElementBufferFunc)					loader->get("glVertexArrayElementBuffer");
	gl->vertexArrayVertexBuffer						= (glVertexArrayVertexBufferFunc)					loader->get("glVertexArrayVertexBuffer");
	gl->vertexArrayVertexBuffers					= (glVertexArrayVertexBuffersFunc)					loader->get("glVertexArrayVertexBuffers");
}

if (de::contains(extSet, "GL_ARB_get_program_binary"))
{
	gl->getProgramBinary	= (glGetProgramBinaryFunc)	loader->get("glGetProgramBinary");
	gl->programBinary		= (glProgramBinaryFunc)		loader->get("glProgramBinary");
	gl->programParameteri	= (glProgramParameteriFunc)	loader->get("glProgramParameteri");
}

if (de::contains(extSet, "GL_ARB_gl_spirv"))
{
	gl->specializeShader	= (glSpecializeShaderFunc)	loader->get("glSpecializeShaderARB");
}

if (de::contains(extSet, "GL_ARB_indirect_parameters"))
{
	gl->multiDrawArraysIndirectCount	= (glMultiDrawArraysIndirectCountFunc)		loader->get("glMultiDrawArraysIndirectCountARB");
	gl->multiDrawElementsIndirectCount	= (glMultiDrawElementsIndirectCountFunc)	loader->get("glMultiDrawElementsIndirectCountARB");
}

if (de::contains(extSet, "GL_ARB_internalformat_query"))
{
	gl->getInternalformativ	= (glGetInternalformativFunc)	loader->get("glGetInternalformativ");
}

if (de::contains(extSet, "GL_ARB_instanced_arrays"))
{
	gl->vertexAttribDivisor	= (glVertexAttribDivisorFunc)	loader->get("glVertexAttribDivisorARB");
}

if (de::contains(extSet, "GL_ARB_multi_draw_indirect"))
{
	gl->multiDrawArraysIndirect		= (glMultiDrawArraysIndirectFunc)	loader->get("glMultiDrawArraysIndirect");
	gl->multiDrawElementsIndirect	= (glMultiDrawElementsIndirectFunc)	loader->get("glMultiDrawElementsIndirect");
}

if (de::contains(extSet, "GL_ARB_parallel_shader_compile"))
{
	gl->maxShaderCompilerThreadsKHR	= (glMaxShaderCompilerThreadsKHRFunc)	loader->get("glMaxShaderCompilerThreadsARB");
}

if (de::contains(extSet, "GL_ARB_program_interface_query"))
{
	gl->getProgramInterfaceiv			= (glGetProgramInterfaceivFunc)				loader->get("glGetProgramInterfaceiv");
	gl->getProgramResourceIndex			= (glGetProgramResourceIndexFunc)			loader->get("glGetProgramResourceIndex");
	gl->getProgramResourceLocation		= (glGetProgramResourceLocationFunc)		loader->get("glGetProgramResourceLocation");
	gl->getProgramResourceLocationIndex	= (glGetProgramResourceLocationIndexFunc)	loader->get("glGetProgramResourceLocationIndex");
	gl->getProgramResourceName			= (glGetProgramResourceNameFunc)			loader->get("glGetProgramResourceName");
	gl->getProgramResourceiv			= (glGetProgramResourceivFunc)				loader->get("glGetProgramResourceiv");
}

if (de::contains(extSet, "GL_ARB_separate_shader_objects"))
{
	gl->activeShaderProgram			= (glActiveShaderProgramFunc)		loader->get("glActiveShaderProgram");
	gl->bindProgramPipeline			= (glBindProgramPipelineFunc)		loader->get("glBindProgramPipeline");
	gl->createShaderProgramv		= (glCreateShaderProgramvFunc)		loader->get("glCreateShaderProgramv");
	gl->deleteProgramPipelines		= (glDeleteProgramPipelinesFunc)	loader->get("glDeleteProgramPipelines");
	gl->genProgramPipelines			= (glGenProgramPipelinesFunc)		loader->get("glGenProgramPipelines");
	gl->getProgramPipelineInfoLog	= (glGetProgramPipelineInfoLogFunc)	loader->get("glGetProgramPipelineInfoLog");
	gl->getProgramPipelineiv		= (glGetProgramPipelineivFunc)		loader->get("glGetProgramPipelineiv");
	gl->isProgramPipeline			= (glIsProgramPipelineFunc)			loader->get("glIsProgramPipeline");
	gl->programParameteri			= (glProgramParameteriFunc)			loader->get("glProgramParameteri");
	gl->programUniform1d			= (glProgramUniform1dFunc)			loader->get("glProgramUniform1d");
	gl->programUniform1dv			= (glProgramUniform1dvFunc)			loader->get("glProgramUniform1dv");
	gl->programUniform1f			= (glProgramUniform1fFunc)			loader->get("glProgramUniform1f");
	gl->programUniform1fv			= (glProgramUniform1fvFunc)			loader->get("glProgramUniform1fv");
	gl->programUniform1i			= (glProgramUniform1iFunc)			loader->get("glProgramUniform1i");
	gl->programUniform1iv			= (glProgramUniform1ivFunc)			loader->get("glProgramUniform1iv");
	gl->programUniform1ui			= (glProgramUniform1uiFunc)			loader->get("glProgramUniform1ui");
	gl->programUniform1uiv			= (glProgramUniform1uivFunc)		loader->get("glProgramUniform1uiv");
	gl->programUniform2d			= (glProgramUniform2dFunc)			loader->get("glProgramUniform2d");
	gl->programUniform2dv			= (glProgramUniform2dvFunc)			loader->get("glProgramUniform2dv");
	gl->programUniform2f			= (glProgramUniform2fFunc)			loader->get("glProgramUniform2f");
	gl->programUniform2fv			= (glProgramUniform2fvFunc)			loader->get("glProgramUniform2fv");
	gl->programUniform2i			= (glProgramUniform2iFunc)			loader->get("glProgramUniform2i");
	gl->programUniform2iv			= (glProgramUniform2ivFunc)			loader->get("glProgramUniform2iv");
	gl->programUniform2ui			= (glProgramUniform2uiFunc)			loader->get("glProgramUniform2ui");
	gl->programUniform2uiv			= (glProgramUniform2uivFunc)		loader->get("glProgramUniform2uiv");
	gl->programUniform3d			= (glProgramUniform3dFunc)			loader->get("glProgramUniform3d");
	gl->programUniform3dv			= (glProgramUniform3dvFunc)			loader->get("glProgramUniform3dv");
	gl->programUniform3f			= (glProgramUniform3fFunc)			loader->get("glProgramUniform3f");
	gl->programUniform3fv			= (glProgramUniform3fvFunc)			loader->get("glProgramUniform3fv");
	gl->programUniform3i			= (glProgramUniform3iFunc)			loader->get("glProgramUniform3i");
	gl->programUniform3iv			= (glProgramUniform3ivFunc)			loader->get("glProgramUniform3iv");
	gl->programUniform3ui			= (glProgramUniform3uiFunc)			loader->get("glProgramUniform3ui");
	gl->programUniform3uiv			= (glProgramUniform3uivFunc)		loader->get("glProgramUniform3uiv");
	gl->programUniform4d			= (glProgramUniform4dFunc)			loader->get("glProgramUniform4d");
	gl->programUniform4dv			= (glProgramUniform4dvFunc)			loader->get("glProgramUniform4dv");
	gl->programUniform4f			= (glProgramUniform4fFunc)			loader->get("glProgramUniform4f");
	gl->programUniform4fv			= (glProgramUniform4fvFunc)			loader->get("glProgramUniform4fv");
	gl->programUniform4i			= (glProgramUniform4iFunc)			loader->get("glProgramUniform4i");
	gl->programUniform4iv			= (glProgramUniform4ivFunc)			loader->get("glProgramUniform4iv");
	gl->programUniform4ui			= (glProgramUniform4uiFunc)			loader->get("glProgramUniform4ui");
	gl->programUniform4uiv			= (glProgramUniform4uivFunc)		loader->get("glProgramUniform4uiv");
	gl->programUniformMatrix2dv		= (glProgramUniformMatrix2dvFunc)	loader->get("glProgramUniformMatrix2dv");
	gl->programUniformMatrix2fv		= (glProgramUniformMatrix2fvFunc)	loader->get("glProgramUniformMatrix2fv");
	gl->programUniformMatrix2x3dv	= (glProgramUniformMatrix2x3dvFunc)	loader->get("glProgramUniformMatrix2x3dv");
	gl->programUniformMatrix2x3fv	= (glProgramUniformMatrix2x3fvFunc)	loader->get("glProgramUniformMatrix2x3fv");
	gl->programUniformMatrix2x4dv	= (glProgramUniformMatrix2x4dvFunc)	loader->get("glProgramUniformMatrix2x4dv");
	gl->programUniformMatrix2x4fv	= (glProgramUniformMatrix2x4fvFunc)	loader->get("glProgramUniformMatrix2x4fv");
	gl->programUniformMatrix3dv		= (glProgramUniformMatrix3dvFunc)	loader->get("glProgramUniformMatrix3dv");
	gl->programUniformMatrix3fv		= (glProgramUniformMatrix3fvFunc)	loader->get("glProgramUniformMatrix3fv");
	gl->programUniformMatrix3x2dv	= (glProgramUniformMatrix3x2dvFunc)	loader->get("glProgramUniformMatrix3x2dv");
	gl->programUniformMatrix3x2fv	= (glProgramUniformMatrix3x2fvFunc)	loader->get("glProgramUniformMatrix3x2fv");
	gl->programUniformMatrix3x4dv	= (glProgramUniformMatrix3x4dvFunc)	loader->get("glProgramUniformMatrix3x4dv");
	gl->programUniformMatrix3x4fv	= (glProgramUniformMatrix3x4fvFunc)	loader->get("glProgramUniformMatrix3x4fv");
	gl->programUniformMatrix4dv		= (glProgramUniformMatrix4dvFunc)	loader->get("glProgramUniformMatrix4dv");
	gl->programUniformMatrix4fv		= (glProgramUniformMatrix4fvFunc)	loader->get("glProgramUniformMatrix4fv");
	gl->programUniformMatrix4x2dv	= (glProgramUniformMatrix4x2dvFunc)	loader->get("glProgramUniformMatrix4x2dv");
	gl->programUniformMatrix4x2fv	= (glProgramUniformMatrix4x2fvFunc)	loader->get("glProgramUniformMatrix4x2fv");
	gl->programUniformMatrix4x3dv	= (glProgramUniformMatrix4x3dvFunc)	loader->get("glProgramUniformMatrix4x3dv");
	gl->programUniformMatrix4x3fv	= (glProgramUniformMatrix4x3fvFunc)	loader->get("glProgramUniformMatrix4x3fv");
	gl->useProgramStages			= (glUseProgramStagesFunc)			loader->get("glUseProgramStages");
	gl->validateProgramPipeline		= (glValidateProgramPipelineFunc)	loader->get("glValidateProgramPipeline");
}

if (de::contains(extSet, "GL_ARB_shader_image_load_store"))
{
	gl->bindImageTexture	= (glBindImageTextureFunc)	loader->get("glBindImageTexture");
	gl->memoryBarrier		= (glMemoryBarrierFunc)		loader->get("glMemoryBarrier");
}

if (de::contains(extSet, "GL_ARB_sparse_buffer"))
{
	gl->bufferPageCommitmentARB			= (glBufferPageCommitmentARBFunc)		loader->get("glBufferPageCommitmentARB");
	gl->namedBufferPageCommitmentARB	= (glNamedBufferPageCommitmentARBFunc)	loader->get("glNamedBufferPageCommitmentARB");
	gl->namedBufferPageCommitmentEXT	= (glNamedBufferPageCommitmentEXTFunc)	loader->get("glNamedBufferPageCommitmentEXT");
}

if (de::contains(extSet, "GL_ARB_sparse_texture"))
{
	gl->texPageCommitmentARB	= (glTexPageCommitmentARBFunc)	loader->get("glTexPageCommitmentARB");
}

if (de::contains(extSet, "GL_ARB_tessellation_shader"))
{
	gl->patchParameterfv	= (glPatchParameterfvFunc)	loader->get("glPatchParameterfv");
	gl->patchParameteri		= (glPatchParameteriFunc)	loader->get("glPatchParameteri");
}

if (de::contains(extSet, "GL_ARB_texture_barrier"))
{
	gl->textureBarrier	= (glTextureBarrierFunc)	loader->get("glTextureBarrier");
}

if (de::contains(extSet, "GL_ARB_texture_storage"))
{
	gl->texStorage1D	= (glTexStorage1DFunc)	loader->get("glTexStorage1D");
	gl->texStorage2D	= (glTexStorage2DFunc)	loader->get("glTexStorage2D");
	gl->texStorage3D	= (glTexStorage3DFunc)	loader->get("glTexStorage3D");
}

if (de::contains(extSet, "GL_ARB_texture_storage_multisample"))
{
	gl->texStorage2DMultisample	= (glTexStorage2DMultisampleFunc)	loader->get("glTexStorage2DMultisample");
	gl->texStorage3DMultisample	= (glTexStorage3DMultisampleFunc)	loader->get("glTexStorage3DMultisample");
}

if (de::contains(extSet, "GL_ARB_texture_multisample"))
{
	gl->getMultisamplefv		= (glGetMultisamplefvFunc)		loader->get("glGetMultisamplefv");
	gl->sampleMaski				= (glSampleMaskiFunc)			loader->get("glSampleMaski");
	gl->texImage2DMultisample	= (glTexImage2DMultisampleFunc)	loader->get("glTexImage2DMultisample");
	gl->texImage3DMultisample	= (glTexImage3DMultisampleFunc)	loader->get("glTexImage3DMultisample");
}

if (de::contains(extSet, "GL_ARB_texture_view"))
{
	gl->textureView	= (glTextureViewFunc)	loader->get("glTextureView");
}

if (de::contains(extSet, "GL_ARB_transform_feedback2"))
{
	gl->bindTransformFeedback		= (glBindTransformFeedbackFunc)		loader->get("glBindTransformFeedback");
	gl->deleteTransformFeedbacks	= (glDeleteTransformFeedbacksFunc)	loader->get("glDeleteTransformFeedbacks");
	gl->drawTransformFeedback		= (glDrawTransformFeedbackFunc)		loader->get("glDrawTransformFeedback");
	gl->genTransformFeedbacks		= (glGenTransformFeedbacksFunc)		loader->get("glGenTransformFeedbacks");
	gl->isTransformFeedback			= (glIsTransformFeedbackFunc)		loader->get("glIsTransformFeedback");
	gl->pauseTransformFeedback		= (glPauseTransformFeedbackFunc)	loader->get("glPauseTransformFeedback");
	gl->resumeTransformFeedback		= (glResumeTransformFeedbackFunc)	loader->get("glResumeTransformFeedback");
}

if (de::contains(extSet, "GL_ARB_transform_feedback3"))
{
	gl->beginQueryIndexed			= (glBeginQueryIndexedFunc)				loader->get("glBeginQueryIndexed");
	gl->drawTransformFeedbackStream	= (glDrawTransformFeedbackStreamFunc)	loader->get("glDrawTransformFeedbackStream");
	gl->endQueryIndexed				= (glEndQueryIndexedFunc)				loader->get("glEndQueryIndexed");
	gl->getQueryIndexediv			= (glGetQueryIndexedivFunc)				loader->get("glGetQueryIndexediv");
}

if (de::contains(extSet, "GL_ARB_transform_feedback_instanced"))
{
	gl->drawTransformFeedbackInstanced			= (glDrawTransformFeedbackInstancedFunc)		loader->get("glDrawTransformFeedbackInstanced");
	gl->drawTransformFeedbackStreamInstanced	= (glDrawTransformFeedbackStreamInstancedFunc)	loader->get("glDrawTransformFeedbackStreamInstanced");
}

if (de::contains(extSet, "GL_ARB_vertex_attrib_64bit"))
{
	gl->getVertexAttribLdv		= (glGetVertexAttribLdvFunc)	loader->get("glGetVertexAttribLdv");
	gl->vertexAttribL1d			= (glVertexAttribL1dFunc)		loader->get("glVertexAttribL1d");
	gl->vertexAttribL1dv		= (glVertexAttribL1dvFunc)		loader->get("glVertexAttribL1dv");
	gl->vertexAttribL2d			= (glVertexAttribL2dFunc)		loader->get("glVertexAttribL2d");
	gl->vertexAttribL2dv		= (glVertexAttribL2dvFunc)		loader->get("glVertexAttribL2dv");
	gl->vertexAttribL3d			= (glVertexAttribL3dFunc)		loader->get("glVertexAttribL3d");
	gl->vertexAttribL3dv		= (glVertexAttribL3dvFunc)		loader->get("glVertexAttribL3dv");
	gl->vertexAttribL4d			= (glVertexAttribL4dFunc)		loader->get("glVertexAttribL4d");
	gl->vertexAttribL4dv		= (glVertexAttribL4dvFunc)		loader->get("glVertexAttribL4dv");
	gl->vertexAttribLPointer	= (glVertexAttribLPointerFunc)	loader->get("glVertexAttribLPointer");
}

if (de::contains(extSet, "GL_ARB_vertex_attrib_binding"))
{
	gl->bindVertexBuffer		= (glBindVertexBufferFunc)		loader->get("glBindVertexBuffer");
	gl->vertexAttribBinding		= (glVertexAttribBindingFunc)	loader->get("glVertexAttribBinding");
	gl->vertexAttribFormat		= (glVertexAttribFormatFunc)	loader->get("glVertexAttribFormat");
	gl->vertexAttribIFormat		= (glVertexAttribIFormatFunc)	loader->get("glVertexAttribIFormat");
	gl->vertexAttribLFormat		= (glVertexAttribLFormatFunc)	loader->get("glVertexAttribLFormat");
	gl->vertexBindingDivisor	= (glVertexBindingDivisorFunc)	loader->get("glVertexBindingDivisor");
}

if (de::contains(extSet, "GL_NV_internalformat_sample_query"))
{
	gl->getInternalformatSampleivNV	= (glGetInternalformatSampleivNVFunc)	loader->get("glGetInternalformatSampleivNV");
}

if (de::contains(extSet, "GL_OVR_multiview"))
{
	gl->framebufferTextureMultiviewOVR		= (glFramebufferTextureMultiviewOVRFunc)		loader->get("glFramebufferTextureMultiviewOVR");
	gl->namedFramebufferTextureMultiviewOVR	= (glNamedFramebufferTextureMultiviewOVRFunc)	loader->get("glNamedFramebufferTextureMultiviewOVR");
}
