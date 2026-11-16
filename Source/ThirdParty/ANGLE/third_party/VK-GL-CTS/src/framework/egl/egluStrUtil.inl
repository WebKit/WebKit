/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos EGL API description (egl.xml) revision 682c662d48fbae076c5ed89a1bd5b2aa7e2e4449.
 */

const char* getBooleanName (int value)
{
	switch (value)
	{
		case EGL_TRUE:	return "EGL_TRUE";
		case EGL_FALSE:	return "EGL_FALSE";
		default:		return nullptr;
	}
}

const char* getBoolDontCareName (int value)
{
	switch (value)
	{
		case EGL_TRUE:		return "EGL_TRUE";
		case EGL_FALSE:		return "EGL_FALSE";
		case EGL_DONT_CARE:	return "EGL_DONT_CARE";
		default:			return nullptr;
	}
}

const char* getAPIName (int value)
{
	switch (value)
	{
		case EGL_OPENGL_API:	return "EGL_OPENGL_API";
		case EGL_OPENGL_ES_API:	return "EGL_OPENGL_ES_API";
		case EGL_OPENVG_API:	return "EGL_OPENVG_API";
		default:				return nullptr;
	}
}

const char* getErrorName (int value)
{
	switch (value)
	{
		case EGL_SUCCESS:				return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:		return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:				return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONFIG:			return "EGL_BAD_CONFIG";
		case EGL_BAD_CONTEXT:			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CURRENT_SURFACE:	return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:			return "EGL_BAD_DISPLAY";
		case EGL_BAD_MATCH:				return "EGL_BAD_MATCH";
		case EGL_BAD_NATIVE_PIXMAP:		return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:		return "EGL_BAD_NATIVE_WINDOW";
		case EGL_BAD_PARAMETER:			return "EGL_BAD_PARAMETER";
		case EGL_BAD_SURFACE:			return "EGL_BAD_SURFACE";
		case EGL_CONTEXT_LOST:			return "EGL_CONTEXT_LOST";
		default:						return nullptr;
	}
}

const char* getContextAttribName (int value)
{
	switch (value)
	{
		case EGL_CONFIG_ID:					return "EGL_CONFIG_ID";
		case EGL_CONTEXT_CLIENT_TYPE:		return "EGL_CONTEXT_CLIENT_TYPE";
		case EGL_CONTEXT_CLIENT_VERSION:	return "EGL_CONTEXT_CLIENT_VERSION";
		case EGL_RENDER_BUFFER:				return "EGL_RENDER_BUFFER";
		default:							return nullptr;
	}
}

const char* getConfigAttribName (int value)
{
	switch (value)
	{
		case EGL_BUFFER_SIZE:				return "EGL_BUFFER_SIZE";
		case EGL_RED_SIZE:					return "EGL_RED_SIZE";
		case EGL_GREEN_SIZE:				return "EGL_GREEN_SIZE";
		case EGL_BLUE_SIZE:					return "EGL_BLUE_SIZE";
		case EGL_LUMINANCE_SIZE:			return "EGL_LUMINANCE_SIZE";
		case EGL_ALPHA_SIZE:				return "EGL_ALPHA_SIZE";
		case EGL_ALPHA_MASK_SIZE:			return "EGL_ALPHA_MASK_SIZE";
		case EGL_BIND_TO_TEXTURE_RGB:		return "EGL_BIND_TO_TEXTURE_RGB";
		case EGL_BIND_TO_TEXTURE_RGBA:		return "EGL_BIND_TO_TEXTURE_RGBA";
		case EGL_COLOR_BUFFER_TYPE:			return "EGL_COLOR_BUFFER_TYPE";
		case EGL_CONFIG_CAVEAT:				return "EGL_CONFIG_CAVEAT";
		case EGL_CONFIG_ID:					return "EGL_CONFIG_ID";
		case EGL_CONFORMANT:				return "EGL_CONFORMANT";
		case EGL_DEPTH_SIZE:				return "EGL_DEPTH_SIZE";
		case EGL_LEVEL:						return "EGL_LEVEL";
		case EGL_MATCH_NATIVE_PIXMAP:		return "EGL_MATCH_NATIVE_PIXMAP";
		case EGL_MAX_SWAP_INTERVAL:			return "EGL_MAX_SWAP_INTERVAL";
		case EGL_MIN_SWAP_INTERVAL:			return "EGL_MIN_SWAP_INTERVAL";
		case EGL_NATIVE_RENDERABLE:			return "EGL_NATIVE_RENDERABLE";
		case EGL_NATIVE_VISUAL_TYPE:		return "EGL_NATIVE_VISUAL_TYPE";
		case EGL_RENDERABLE_TYPE:			return "EGL_RENDERABLE_TYPE";
		case EGL_SAMPLE_BUFFERS:			return "EGL_SAMPLE_BUFFERS";
		case EGL_SAMPLES:					return "EGL_SAMPLES";
		case EGL_STENCIL_SIZE:				return "EGL_STENCIL_SIZE";
		case EGL_SURFACE_TYPE:				return "EGL_SURFACE_TYPE";
		case EGL_TRANSPARENT_TYPE:			return "EGL_TRANSPARENT_TYPE";
		case EGL_TRANSPARENT_RED_VALUE:		return "EGL_TRANSPARENT_RED_VALUE";
		case EGL_TRANSPARENT_GREEN_VALUE:	return "EGL_TRANSPARENT_GREEN_VALUE";
		case EGL_TRANSPARENT_BLUE_VALUE:	return "EGL_TRANSPARENT_BLUE_VALUE";
		case EGL_COLOR_COMPONENT_TYPE_EXT:	return "EGL_COLOR_COMPONENT_TYPE_EXT";
		case EGL_RECORDABLE_ANDROID:		return "EGL_RECORDABLE_ANDROID";
		default:							return nullptr;
	}
}

const char* getSurfaceAttribName (int value)
{
	switch (value)
	{
		case EGL_CONFIG_ID:				return "EGL_CONFIG_ID";
		case EGL_WIDTH:					return "EGL_WIDTH";
		case EGL_HEIGHT:				return "EGL_HEIGHT";
		case EGL_HORIZONTAL_RESOLUTION:	return "EGL_HORIZONTAL_RESOLUTION";
		case EGL_VERTICAL_RESOLUTION:	return "EGL_VERTICAL_RESOLUTION";
		case EGL_LARGEST_PBUFFER:		return "EGL_LARGEST_PBUFFER";
		case EGL_MIPMAP_TEXTURE:		return "EGL_MIPMAP_TEXTURE";
		case EGL_MIPMAP_LEVEL:			return "EGL_MIPMAP_LEVEL";
		case EGL_MULTISAMPLE_RESOLVE:	return "EGL_MULTISAMPLE_RESOLVE";
		case EGL_PIXEL_ASPECT_RATIO:	return "EGL_PIXEL_ASPECT_RATIO";
		case EGL_RENDER_BUFFER:			return "EGL_RENDER_BUFFER";
		case EGL_SWAP_BEHAVIOR:			return "EGL_SWAP_BEHAVIOR";
		case EGL_TEXTURE_FORMAT:		return "EGL_TEXTURE_FORMAT";
		case EGL_TEXTURE_TARGET:		return "EGL_TEXTURE_TARGET";
		case EGL_ALPHA_FORMAT:			return "EGL_ALPHA_FORMAT";
		case EGL_COLORSPACE:			return "EGL_COLORSPACE";
		default:						return nullptr;
	}
}

const char* getYuvOrderName (int value)
{
	switch (value)
	{
		case EGL_NONE:					return "EGL_NONE";
		case EGL_YUV_ORDER_YUV_EXT:		return "EGL_YUV_ORDER_YUV_EXT";
		case EGL_YUV_ORDER_YVU_EXT:		return "EGL_YUV_ORDER_YVU_EXT";
		case EGL_YUV_ORDER_YUYV_EXT:	return "EGL_YUV_ORDER_YUYV_EXT";
		case EGL_YUV_ORDER_UYVY_EXT:	return "EGL_YUV_ORDER_UYVY_EXT";
		case EGL_YUV_ORDER_YVYU_EXT:	return "EGL_YUV_ORDER_YVYU_EXT";
		case EGL_YUV_ORDER_VYUY_EXT:	return "EGL_YUV_ORDER_VYUY_EXT";
		case EGL_YUV_ORDER_AYUV_EXT:	return "EGL_YUV_ORDER_AYUV_EXT";
		default:						return nullptr;
	}
}

const char* getYuvPlaneBppName (int value)
{
	switch (value)
	{
		case EGL_YUV_PLANE_BPP_0_EXT:	return "EGL_YUV_PLANE_BPP_0_EXT";
		case EGL_YUV_PLANE_BPP_8_EXT:	return "EGL_YUV_PLANE_BPP_8_EXT";
		case EGL_YUV_PLANE_BPP_10_EXT:	return "EGL_YUV_PLANE_BPP_10_EXT";
		default:						return nullptr;
	}
}

const char* getColorComponentTypeName (int value)
{
	switch (value)
	{
		case EGL_COLOR_COMPONENT_TYPE_FIXED_EXT:	return "EGL_COLOR_COMPONENT_TYPE_FIXED_EXT";
		case EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT:	return "EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT";
		default:									return nullptr;
	}
}

const char* getSurfaceTargetName (int value)
{
	switch (value)
	{
		case EGL_READ:	return "EGL_READ";
		case EGL_DRAW:	return "EGL_DRAW";
		default:		return nullptr;
	}
}

const char* getColorBufferTypeName (int value)
{
	switch (value)
	{
		case EGL_RGB_BUFFER:		return "EGL_RGB_BUFFER";
		case EGL_LUMINANCE_BUFFER:	return "EGL_LUMINANCE_BUFFER";
		default:					return nullptr;
	}
}

const char* getConfigCaveatName (int value)
{
	switch (value)
	{
		case EGL_NONE:					return "EGL_NONE";
		case EGL_SLOW_CONFIG:			return "EGL_SLOW_CONFIG";
		case EGL_NON_CONFORMANT_CONFIG:	return "EGL_NON_CONFORMANT_CONFIG";
		default:						return nullptr;
	}
}

const char* getTransparentTypeName (int value)
{
	switch (value)
	{
		case EGL_NONE:				return "EGL_NONE";
		case EGL_TRANSPARENT_RGB:	return "EGL_TRANSPARENT_RGB";
		default:					return nullptr;
	}
}

const char* getMultisampleResolveName (int value)
{
	switch (value)
	{
		case EGL_MULTISAMPLE_RESOLVE_DEFAULT:	return "EGL_MULTISAMPLE_RESOLVE_DEFAULT";
		case EGL_MULTISAMPLE_RESOLVE_BOX:		return "EGL_MULTISAMPLE_RESOLVE_BOX";
		default:								return nullptr;
	}
}

const char* getRenderBufferName (int value)
{
	switch (value)
	{
		case EGL_SINGLE_BUFFER:	return "EGL_SINGLE_BUFFER";
		case EGL_BACK_BUFFER:	return "EGL_BACK_BUFFER";
		default:				return nullptr;
	}
}

const char* getSwapBehaviorName (int value)
{
	switch (value)
	{
		case EGL_BUFFER_DESTROYED:	return "EGL_BUFFER_DESTROYED";
		case EGL_BUFFER_PRESERVED:	return "EGL_BUFFER_PRESERVED";
		default:					return nullptr;
	}
}

const char* getTextureFormatName (int value)
{
	switch (value)
	{
		case EGL_NO_TEXTURE:	return "EGL_NO_TEXTURE";
		case EGL_TEXTURE_RGB:	return "EGL_TEXTURE_RGB";
		case EGL_TEXTURE_RGBA:	return "EGL_TEXTURE_RGBA";
		default:				return nullptr;
	}
}

const char* getTextureTargetName (int value)
{
	switch (value)
	{
		case EGL_NO_TEXTURE:	return "EGL_NO_TEXTURE";
		case EGL_TEXTURE_2D:	return "EGL_TEXTURE_2D";
		default:				return nullptr;
	}
}

const char* getAlphaFormatName (int value)
{
	switch (value)
	{
		case EGL_ALPHA_FORMAT_NONPRE:	return "EGL_ALPHA_FORMAT_NONPRE";
		case EGL_ALPHA_FORMAT_PRE:		return "EGL_ALPHA_FORMAT_PRE";
		default:						return nullptr;
	}
}

const char* getColorspaceName (int value)
{
	switch (value)
	{
		case EGL_COLORSPACE_sRGB:	return "EGL_COLORSPACE_sRGB";
		case EGL_COLORSPACE_LINEAR:	return "EGL_COLORSPACE_LINEAR";
		default:					return nullptr;
	}
}

tcu::Format::Bitfield<16> getAPIBitsStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(EGL_OPENGL_BIT,			"EGL_OPENGL_BIT"),
		tcu::Format::BitDesc(EGL_OPENGL_ES_BIT,			"EGL_OPENGL_ES_BIT"),
		tcu::Format::BitDesc(EGL_OPENGL_ES2_BIT,		"EGL_OPENGL_ES2_BIT"),
		tcu::Format::BitDesc(EGL_OPENGL_ES3_BIT_KHR,	"EGL_OPENGL_ES3_BIT_KHR"),
		tcu::Format::BitDesc(EGL_OPENVG_BIT,			"EGL_OPENVG_BIT"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}

tcu::Format::Bitfield<16> getSurfaceBitsStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(EGL_PBUFFER_BIT,					"EGL_PBUFFER_BIT"),
		tcu::Format::BitDesc(EGL_PIXMAP_BIT,					"EGL_PIXMAP_BIT"),
		tcu::Format::BitDesc(EGL_WINDOW_BIT,					"EGL_WINDOW_BIT"),
		tcu::Format::BitDesc(EGL_MULTISAMPLE_RESOLVE_BOX_BIT,	"EGL_MULTISAMPLE_RESOLVE_BOX_BIT"),
		tcu::Format::BitDesc(EGL_SWAP_BEHAVIOR_PRESERVED_BIT,	"EGL_SWAP_BEHAVIOR_PRESERVED_BIT"),
		tcu::Format::BitDesc(EGL_VG_ALPHA_FORMAT_PRE_BIT,		"EGL_VG_ALPHA_FORMAT_PRE_BIT"),
		tcu::Format::BitDesc(EGL_VG_COLORSPACE_LINEAR_BIT,		"EGL_VG_COLORSPACE_LINEAR_BIT"),
		tcu::Format::BitDesc(EGL_LOCK_SURFACE_BIT_KHR,			"EGL_LOCK_SURFACE_BIT_KHR"),
		tcu::Format::BitDesc(EGL_OPTIMAL_FORMAT_BIT_KHR,		"EGL_OPTIMAL_FORMAT_BIT_KHR"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}
