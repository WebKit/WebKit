/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 *
 * Generated from Khronos GL API description (gl.xml) revision 7d0cb181dace7461bc4f26650fec71efeacc18a5.
 */

const char* getErrorName (int value)
{
	switch (value)
	{
		case GL_NO_ERROR:						return "GL_NO_ERROR";
		case GL_INVALID_ENUM:					return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:					return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:				return "GL_INVALID_OPERATION";
		case GL_OUT_OF_MEMORY:					return "GL_OUT_OF_MEMORY";
		case GL_INVALID_FRAMEBUFFER_OPERATION:	return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_CONTEXT_LOST:					return "GL_CONTEXT_LOST";
		default:								return nullptr;
	}
}

const char* getTypeName (int value)
{
	switch (value)
	{
		case GL_BYTE:							return "GL_BYTE";
		case GL_UNSIGNED_BYTE:					return "GL_UNSIGNED_BYTE";
		case GL_SHORT:							return "GL_SHORT";
		case GL_UNSIGNED_SHORT:					return "GL_UNSIGNED_SHORT";
		case GL_INT:							return "GL_INT";
		case GL_UNSIGNED_INT:					return "GL_UNSIGNED_INT";
		case GL_FLOAT:							return "GL_FLOAT";
		case GL_DOUBLE:							return "GL_DOUBLE";
		case GL_FIXED:							return "GL_FIXED";
		case GL_UNSIGNED_SHORT_5_6_5:			return "GL_UNSIGNED_SHORT_5_6_5";
		case GL_UNSIGNED_SHORT_4_4_4_4:			return "GL_UNSIGNED_SHORT_4_4_4_4";
		case GL_UNSIGNED_SHORT_5_5_5_1:			return "GL_UNSIGNED_SHORT_5_5_5_1";
		case GL_UNSIGNED_BYTE_3_3_2:			return "GL_UNSIGNED_BYTE_3_3_2";
		case GL_UNSIGNED_INT_8_8_8_8:			return "GL_UNSIGNED_INT_8_8_8_8";
		case GL_UNSIGNED_INT_10_10_10_2:		return "GL_UNSIGNED_INT_10_10_10_2";
		case GL_UNSIGNED_BYTE_2_3_3_REV:		return "GL_UNSIGNED_BYTE_2_3_3_REV";
		case GL_UNSIGNED_SHORT_5_6_5_REV:		return "GL_UNSIGNED_SHORT_5_6_5_REV";
		case GL_UNSIGNED_SHORT_4_4_4_4_REV:		return "GL_UNSIGNED_SHORT_4_4_4_4_REV";
		case GL_UNSIGNED_SHORT_1_5_5_5_REV:		return "GL_UNSIGNED_SHORT_1_5_5_5_REV";
		case GL_UNSIGNED_INT_8_8_8_8_REV:		return "GL_UNSIGNED_INT_8_8_8_8_REV";
		case GL_HALF_FLOAT:						return "GL_HALF_FLOAT";
		case GL_INT_2_10_10_10_REV:				return "GL_INT_2_10_10_10_REV";
		case GL_UNSIGNED_INT_2_10_10_10_REV:	return "GL_UNSIGNED_INT_2_10_10_10_REV";
		case GL_UNSIGNED_INT_10F_11F_11F_REV:	return "GL_UNSIGNED_INT_10F_11F_11F_REV";
		case GL_UNSIGNED_INT_5_9_9_9_REV:		return "GL_UNSIGNED_INT_5_9_9_9_REV";
		case GL_UNSIGNED_INT_24_8:				return "GL_UNSIGNED_INT_24_8";
		case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:	return "GL_FLOAT_32_UNSIGNED_INT_24_8_REV";
		case GL_SIGNED_NORMALIZED:				return "GL_SIGNED_NORMALIZED";
		case GL_UNSIGNED_NORMALIZED:			return "GL_UNSIGNED_NORMALIZED";
		case GL_HALF_FLOAT_OES:					return "GL_HALF_FLOAT_OES";
		case GL_DOUBLE_VEC2:					return "GL_DOUBLE_VEC2";
		case GL_DOUBLE_VEC3:					return "GL_DOUBLE_VEC3";
		case GL_DOUBLE_VEC4:					return "GL_DOUBLE_VEC4";
		case GL_DOUBLE_MAT2:					return "GL_DOUBLE_MAT2";
		case GL_DOUBLE_MAT3:					return "GL_DOUBLE_MAT3";
		case GL_DOUBLE_MAT4:					return "GL_DOUBLE_MAT4";
		case GL_DOUBLE_MAT2x3:					return "GL_DOUBLE_MAT2x3";
		case GL_DOUBLE_MAT2x4:					return "GL_DOUBLE_MAT2x4";
		case GL_DOUBLE_MAT3x2:					return "GL_DOUBLE_MAT3x2";
		case GL_DOUBLE_MAT3x4:					return "GL_DOUBLE_MAT3x4";
		case GL_DOUBLE_MAT4x2:					return "GL_DOUBLE_MAT4x2";
		case GL_DOUBLE_MAT4x3:					return "GL_DOUBLE_MAT4x3";
		default:								return nullptr;
	}
}

const char* getParamQueryName (int value)
{
	switch (value)
	{
		case GL_LINE_WIDTH:							return "GL_LINE_WIDTH";
		case GL_LINE_SMOOTH:						return "GL_LINE_SMOOTH";
		case GL_LINE_SMOOTH_HINT:					return "GL_LINE_SMOOTH_HINT";
		case GL_LINE_WIDTH_RANGE:					return "GL_LINE_WIDTH_RANGE";
		case GL_LINE_WIDTH_GRANULARITY:				return "GL_LINE_WIDTH_GRANULARITY";
		case GL_POLYGON_SMOOTH:						return "GL_POLYGON_SMOOTH";
		case GL_POLYGON_SMOOTH_HINT:				return "GL_POLYGON_SMOOTH_HINT";
		case GL_ALIASED_POINT_SIZE_RANGE:			return "GL_ALIASED_POINT_SIZE_RANGE";
		case GL_ALIASED_LINE_WIDTH_RANGE:			return "GL_ALIASED_LINE_WIDTH_RANGE";
		case GL_CULL_FACE_MODE:						return "GL_CULL_FACE_MODE";
		case GL_FRONT_FACE:							return "GL_FRONT_FACE";
		case GL_DEPTH_RANGE:						return "GL_DEPTH_RANGE";
		case GL_DEPTH_WRITEMASK:					return "GL_DEPTH_WRITEMASK";
		case GL_DEPTH_CLEAR_VALUE:					return "GL_DEPTH_CLEAR_VALUE";
		case GL_DEPTH_FUNC:							return "GL_DEPTH_FUNC";
		case GL_STENCIL_CLEAR_VALUE:				return "GL_STENCIL_CLEAR_VALUE";
		case GL_STENCIL_FUNC:						return "GL_STENCIL_FUNC";
		case GL_STENCIL_FAIL:						return "GL_STENCIL_FAIL";
		case GL_STENCIL_PASS_DEPTH_FAIL:			return "GL_STENCIL_PASS_DEPTH_FAIL";
		case GL_STENCIL_PASS_DEPTH_PASS:			return "GL_STENCIL_PASS_DEPTH_PASS";
		case GL_STENCIL_REF:						return "GL_STENCIL_REF";
		case GL_STENCIL_VALUE_MASK:					return "GL_STENCIL_VALUE_MASK";
		case GL_STENCIL_WRITEMASK:					return "GL_STENCIL_WRITEMASK";
		case GL_STENCIL_BACK_FUNC:					return "GL_STENCIL_BACK_FUNC";
		case GL_STENCIL_BACK_FAIL:					return "GL_STENCIL_BACK_FAIL";
		case GL_STENCIL_BACK_PASS_DEPTH_FAIL:		return "GL_STENCIL_BACK_PASS_DEPTH_FAIL";
		case GL_STENCIL_BACK_PASS_DEPTH_PASS:		return "GL_STENCIL_BACK_PASS_DEPTH_PASS";
		case GL_STENCIL_BACK_REF:					return "GL_STENCIL_BACK_REF";
		case GL_STENCIL_BACK_VALUE_MASK:			return "GL_STENCIL_BACK_VALUE_MASK";
		case GL_STENCIL_BACK_WRITEMASK:				return "GL_STENCIL_BACK_WRITEMASK";
		case GL_VIEWPORT:							return "GL_VIEWPORT";
		case GL_SCISSOR_BOX:						return "GL_SCISSOR_BOX";
		case GL_SCISSOR_TEST:						return "GL_SCISSOR_TEST";
		case GL_COLOR_CLEAR_VALUE:					return "GL_COLOR_CLEAR_VALUE";
		case GL_COLOR_WRITEMASK:					return "GL_COLOR_WRITEMASK";
		case GL_UNPACK_ALIGNMENT:					return "GL_UNPACK_ALIGNMENT";
		case GL_PACK_ALIGNMENT:						return "GL_PACK_ALIGNMENT";
		case GL_MAX_TEXTURE_SIZE:					return "GL_MAX_TEXTURE_SIZE";
		case GL_MAX_VIEWPORT_DIMS:					return "GL_MAX_VIEWPORT_DIMS";
		case GL_SUBPIXEL_BITS:						return "GL_SUBPIXEL_BITS";
		case GL_RED_BITS:							return "GL_RED_BITS";
		case GL_GREEN_BITS:							return "GL_GREEN_BITS";
		case GL_BLUE_BITS:							return "GL_BLUE_BITS";
		case GL_ALPHA_BITS:							return "GL_ALPHA_BITS";
		case GL_DEPTH_BITS:							return "GL_DEPTH_BITS";
		case GL_STENCIL_BITS:						return "GL_STENCIL_BITS";
		case GL_POLYGON_OFFSET_UNITS:				return "GL_POLYGON_OFFSET_UNITS";
		case GL_POLYGON_OFFSET_FILL:				return "GL_POLYGON_OFFSET_FILL";
		case GL_POLYGON_OFFSET_FACTOR:				return "GL_POLYGON_OFFSET_FACTOR";
		case GL_POLYGON_OFFSET_POINT:				return "GL_POLYGON_OFFSET_POINT";
		case GL_POLYGON_OFFSET_LINE:				return "GL_POLYGON_OFFSET_LINE";
		case GL_TEXTURE_BINDING_2D:					return "GL_TEXTURE_BINDING_2D";
		case GL_SAMPLE_BUFFERS:						return "GL_SAMPLE_BUFFERS";
		case GL_SAMPLES:							return "GL_SAMPLES";
		case GL_SAMPLE_COVERAGE_VALUE:				return "GL_SAMPLE_COVERAGE_VALUE";
		case GL_SAMPLE_COVERAGE_INVERT:				return "GL_SAMPLE_COVERAGE_INVERT";
		case GL_MAX_CUBE_MAP_TEXTURE_SIZE:			return "GL_MAX_CUBE_MAP_TEXTURE_SIZE";
		case GL_MAX_RENDERBUFFER_SIZE:				return "GL_MAX_RENDERBUFFER_SIZE";
		case GL_MAX_VERTEX_ATTRIBS:					return "GL_MAX_VERTEX_ATTRIBS";
		case GL_MAX_VERTEX_UNIFORM_VECTORS:			return "GL_MAX_VERTEX_UNIFORM_VECTORS";
		case GL_MAX_VARYING_VECTORS:				return "GL_MAX_VARYING_VECTORS";
		case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:	return "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS";
		case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:		return "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS";
		case GL_MAX_TEXTURE_IMAGE_UNITS:			return "GL_MAX_TEXTURE_IMAGE_UNITS";
		case GL_MAX_FRAGMENT_UNIFORM_VECTORS:		return "GL_MAX_FRAGMENT_UNIFORM_VECTORS";
		default:									return nullptr;
	}
}

const char* getProgramParamName (int value)
{
	switch (value)
	{
		case GL_ACTIVE_ATTRIBUTES:						return "GL_ACTIVE_ATTRIBUTES";
		case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:			return "GL_ACTIVE_ATTRIBUTE_MAX_LENGTH";
		case GL_ACTIVE_UNIFORMS:						return "GL_ACTIVE_UNIFORMS";
		case GL_ACTIVE_UNIFORM_BLOCKS:					return "GL_ACTIVE_UNIFORM_BLOCKS";
		case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:	return "GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH";
		case GL_ACTIVE_UNIFORM_MAX_LENGTH:				return "GL_ACTIVE_UNIFORM_MAX_LENGTH";
		case GL_ATTACHED_SHADERS:						return "GL_ATTACHED_SHADERS";
		case GL_DELETE_STATUS:							return "GL_DELETE_STATUS";
		case GL_INFO_LOG_LENGTH:						return "GL_INFO_LOG_LENGTH";
		case GL_LINK_STATUS:							return "GL_LINK_STATUS";
		case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:		return "GL_PROGRAM_BINARY_RETRIEVABLE_HINT";
		case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:			return "GL_TRANSFORM_FEEDBACK_BUFFER_MODE";
		case GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED:		return "GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED";
		case GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE:		return "GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE";
		case GL_TRANSFORM_FEEDBACK_VARYINGS:			return "GL_TRANSFORM_FEEDBACK_VARYINGS";
		case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH:	return "GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH";
		case GL_VALIDATE_STATUS:						return "GL_VALIDATE_STATUS";
		case GL_GEOMETRY_INPUT_TYPE:					return "GL_GEOMETRY_INPUT_TYPE";
		case GL_GEOMETRY_OUTPUT_TYPE:					return "GL_GEOMETRY_OUTPUT_TYPE";
		case GL_GEOMETRY_VERTICES_OUT:					return "GL_GEOMETRY_VERTICES_OUT";
		case GL_GEOMETRY_SHADER_INVOCATIONS:			return "GL_GEOMETRY_SHADER_INVOCATIONS";
		case GL_PROGRAM_SEPARABLE:						return "GL_PROGRAM_SEPARABLE";
		case GL_COMPUTE_WORK_GROUP_SIZE:				return "GL_COMPUTE_WORK_GROUP_SIZE";
		case GL_ACTIVE_ATOMIC_COUNTER_BUFFERS:			return "GL_ACTIVE_ATOMIC_COUNTER_BUFFERS";
		case GL_TESS_CONTROL_OUTPUT_VERTICES:			return "GL_TESS_CONTROL_OUTPUT_VERTICES";
		case GL_TESS_GEN_MODE:							return "GL_TESS_GEN_MODE";
		case GL_TESS_GEN_SPACING:						return "GL_TESS_GEN_SPACING";
		case GL_TESS_GEN_VERTEX_ORDER:					return "GL_TESS_GEN_VERTEX_ORDER";
		case GL_TESS_GEN_POINT_MODE:					return "GL_TESS_GEN_POINT_MODE";
		default:										return nullptr;
	}
}

const char* getUniformParamName (int value)
{
	switch (value)
	{
		case GL_UNIFORM_TYPE:			return "GL_UNIFORM_TYPE";
		case GL_UNIFORM_SIZE:			return "GL_UNIFORM_SIZE";
		case GL_UNIFORM_NAME_LENGTH:	return "GL_UNIFORM_NAME_LENGTH";
		case GL_UNIFORM_BLOCK_INDEX:	return "GL_UNIFORM_BLOCK_INDEX";
		case GL_UNIFORM_OFFSET:			return "GL_UNIFORM_OFFSET";
		case GL_UNIFORM_ARRAY_STRIDE:	return "GL_UNIFORM_ARRAY_STRIDE";
		case GL_UNIFORM_MATRIX_STRIDE:	return "GL_UNIFORM_MATRIX_STRIDE";
		case GL_UNIFORM_IS_ROW_MAJOR:	return "GL_UNIFORM_IS_ROW_MAJOR";
		default:						return nullptr;
	}
}

const char* getFramebufferAttachmentName (int value)
{
	switch (value)
	{
		case GL_COLOR_ATTACHMENT0:			return "GL_COLOR_ATTACHMENT0";
		case GL_COLOR_ATTACHMENT1:			return "GL_COLOR_ATTACHMENT1";
		case GL_COLOR_ATTACHMENT2:			return "GL_COLOR_ATTACHMENT2";
		case GL_COLOR_ATTACHMENT3:			return "GL_COLOR_ATTACHMENT3";
		case GL_COLOR_ATTACHMENT4:			return "GL_COLOR_ATTACHMENT4";
		case GL_COLOR_ATTACHMENT5:			return "GL_COLOR_ATTACHMENT5";
		case GL_COLOR_ATTACHMENT6:			return "GL_COLOR_ATTACHMENT6";
		case GL_COLOR_ATTACHMENT7:			return "GL_COLOR_ATTACHMENT7";
		case GL_COLOR_ATTACHMENT8:			return "GL_COLOR_ATTACHMENT8";
		case GL_COLOR_ATTACHMENT9:			return "GL_COLOR_ATTACHMENT9";
		case GL_COLOR_ATTACHMENT10:			return "GL_COLOR_ATTACHMENT10";
		case GL_COLOR_ATTACHMENT11:			return "GL_COLOR_ATTACHMENT11";
		case GL_COLOR_ATTACHMENT12:			return "GL_COLOR_ATTACHMENT12";
		case GL_COLOR_ATTACHMENT13:			return "GL_COLOR_ATTACHMENT13";
		case GL_COLOR_ATTACHMENT14:			return "GL_COLOR_ATTACHMENT14";
		case GL_COLOR_ATTACHMENT15:			return "GL_COLOR_ATTACHMENT15";
		case GL_DEPTH_ATTACHMENT:			return "GL_DEPTH_ATTACHMENT";
		case GL_STENCIL_ATTACHMENT:			return "GL_STENCIL_ATTACHMENT";
		case GL_DEPTH_STENCIL_ATTACHMENT:	return "GL_DEPTH_STENCIL_ATTACHMENT";
		case GL_BACK:						return "GL_BACK";
		case GL_FRONT_LEFT:					return "GL_FRONT_LEFT";
		case GL_FRONT_RIGHT:				return "GL_FRONT_RIGHT";
		case GL_BACK_LEFT:					return "GL_BACK_LEFT";
		case GL_BACK_RIGHT:					return "GL_BACK_RIGHT";
		case GL_LEFT:						return "GL_LEFT";
		case GL_RIGHT:						return "GL_RIGHT";
		case GL_DEPTH:						return "GL_DEPTH";
		case GL_STENCIL:					return "GL_STENCIL";
		default:							return nullptr;
	}
}

const char* getFramebufferAttachmentParameterName (int value)
{
	switch (value)
	{
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:				return "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE";
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:				return "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME";
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:			return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL";
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:	return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE";
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:			return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER";
		case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:			return "GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE";
		case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:			return "GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING";
		case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:				return "GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE";
		case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:				return "GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE";
		case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:				return "GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE";
		case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:				return "GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE";
		case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:				return "GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE";
		case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:			return "GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE";
		case GL_FRAMEBUFFER_ATTACHMENT_LAYERED:					return "GL_FRAMEBUFFER_ATTACHMENT_LAYERED";
		default:												return nullptr;
	}
}

const char* getFramebufferTargetName (int value)
{
	switch (value)
	{
		case GL_FRAMEBUFFER:		return "GL_FRAMEBUFFER";
		case GL_RENDERBUFFER:		return "GL_RENDERBUFFER";
		case GL_DRAW_FRAMEBUFFER:	return "GL_DRAW_FRAMEBUFFER";
		case GL_READ_FRAMEBUFFER:	return "GL_READ_FRAMEBUFFER";
		default:					return nullptr;
	}
}

const char* getFramebufferStatusName (int value)
{
	switch (value)
	{
		case GL_FRAMEBUFFER_COMPLETE:						return "GL_FRAMEBUFFER_COMPLETE";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:			return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
		case GL_FRAMEBUFFER_UNSUPPORTED:					return "GL_FRAMEBUFFER_UNSUPPORTED";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:			return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:		return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
		case GL_FRAMEBUFFER_UNDEFINED:						return "GL_FRAMEBUFFER_UNDEFINED";
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:			return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:			return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
		default:											return nullptr;
	}
}

const char* getFramebufferAttachmentTypeName (int value)
{
	switch (value)
	{
		case GL_NONE:					return "GL_NONE";
		case GL_FRAMEBUFFER_DEFAULT:	return "GL_FRAMEBUFFER_DEFAULT";
		case GL_TEXTURE:				return "GL_TEXTURE";
		case GL_RENDERBUFFER:			return "GL_RENDERBUFFER";
		default:						return nullptr;
	}
}

const char* getFramebufferColorEncodingName (int value)
{
	switch (value)
	{
		case GL_LINEAR:	return "GL_LINEAR";
		case GL_SRGB:	return "GL_SRGB";
		default:		return nullptr;
	}
}

const char* getFramebufferParameterName (int value)
{
	switch (value)
	{
		case GL_FRAMEBUFFER_DEFAULT_WIDTH:					return "GL_FRAMEBUFFER_DEFAULT_WIDTH";
		case GL_FRAMEBUFFER_DEFAULT_HEIGHT:					return "GL_FRAMEBUFFER_DEFAULT_HEIGHT";
		case GL_FRAMEBUFFER_DEFAULT_SAMPLES:				return "GL_FRAMEBUFFER_DEFAULT_SAMPLES";
		case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:	return "GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS";
		case GL_FRAMEBUFFER_DEFAULT_LAYERS:					return "GL_FRAMEBUFFER_DEFAULT_LAYERS";
		case GL_DOUBLEBUFFER:								return "GL_DOUBLEBUFFER";
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT:			return "GL_IMPLEMENTATION_COLOR_READ_FORMAT";
		case GL_IMPLEMENTATION_COLOR_READ_TYPE:				return "GL_IMPLEMENTATION_COLOR_READ_TYPE";
		case GL_SAMPLES:									return "GL_SAMPLES";
		case GL_SAMPLE_BUFFERS:								return "GL_SAMPLE_BUFFERS";
		case GL_STEREO:										return "GL_STEREO";
		default:											return nullptr;
	}
}

const char* getRenderbufferParameterName (int value)
{
	switch (value)
	{
		case GL_RENDERBUFFER_WIDTH:				return "GL_RENDERBUFFER_WIDTH";
		case GL_RENDERBUFFER_HEIGHT:			return "GL_RENDERBUFFER_HEIGHT";
		case GL_RENDERBUFFER_INTERNAL_FORMAT:	return "GL_RENDERBUFFER_INTERNAL_FORMAT";
		case GL_RENDERBUFFER_RED_SIZE:			return "GL_RENDERBUFFER_RED_SIZE";
		case GL_RENDERBUFFER_GREEN_SIZE:		return "GL_RENDERBUFFER_GREEN_SIZE";
		case GL_RENDERBUFFER_BLUE_SIZE:			return "GL_RENDERBUFFER_BLUE_SIZE";
		case GL_RENDERBUFFER_ALPHA_SIZE:		return "GL_RENDERBUFFER_ALPHA_SIZE";
		case GL_RENDERBUFFER_DEPTH_SIZE:		return "GL_RENDERBUFFER_DEPTH_SIZE";
		case GL_RENDERBUFFER_STENCIL_SIZE:		return "GL_RENDERBUFFER_STENCIL_SIZE";
		case GL_RENDERBUFFER_SAMPLES:			return "GL_RENDERBUFFER_SAMPLES";
		default:								return nullptr;
	}
}

const char* getPrimitiveTypeName (int value)
{
	switch (value)
	{
		case GL_POINTS:						return "GL_POINTS";
		case GL_LINES:						return "GL_LINES";
		case GL_LINE_STRIP:					return "GL_LINE_STRIP";
		case GL_LINE_LOOP:					return "GL_LINE_LOOP";
		case GL_TRIANGLES:					return "GL_TRIANGLES";
		case GL_TRIANGLE_STRIP:				return "GL_TRIANGLE_STRIP";
		case GL_TRIANGLE_FAN:				return "GL_TRIANGLE_FAN";
		case GL_LINES_ADJACENCY:			return "GL_LINES_ADJACENCY";
		case GL_LINE_STRIP_ADJACENCY:		return "GL_LINE_STRIP_ADJACENCY";
		case GL_TRIANGLES_ADJACENCY:		return "GL_TRIANGLES_ADJACENCY";
		case GL_TRIANGLE_STRIP_ADJACENCY:	return "GL_TRIANGLE_STRIP_ADJACENCY";
		case GL_PATCHES:					return "GL_PATCHES";
		case GL_ISOLINES:					return "GL_ISOLINES";
		default:							return nullptr;
	}
}

const char* getBlendFactorName (int value)
{
	switch (value)
	{
		case GL_ZERO:						return "GL_ZERO";
		case GL_ONE:						return "GL_ONE";
		case GL_SRC_ALPHA_SATURATE:			return "GL_SRC_ALPHA_SATURATE";
		case GL_SRC_COLOR:					return "GL_SRC_COLOR";
		case GL_ONE_MINUS_SRC_COLOR:		return "GL_ONE_MINUS_SRC_COLOR";
		case GL_SRC_ALPHA:					return "GL_SRC_ALPHA";
		case GL_ONE_MINUS_SRC_ALPHA:		return "GL_ONE_MINUS_SRC_ALPHA";
		case GL_SRC1_COLOR:					return "GL_SRC1_COLOR";
		case GL_ONE_MINUS_SRC1_COLOR:		return "GL_ONE_MINUS_SRC1_COLOR";
		case GL_SRC1_ALPHA:					return "GL_SRC1_ALPHA";
		case GL_ONE_MINUS_SRC1_ALPHA:		return "GL_ONE_MINUS_SRC1_ALPHA";
		case GL_DST_COLOR:					return "GL_DST_COLOR";
		case GL_ONE_MINUS_DST_COLOR:		return "GL_ONE_MINUS_DST_COLOR";
		case GL_DST_ALPHA:					return "GL_DST_ALPHA";
		case GL_ONE_MINUS_DST_ALPHA:		return "GL_ONE_MINUS_DST_ALPHA";
		case GL_CONSTANT_COLOR:				return "GL_CONSTANT_COLOR";
		case GL_ONE_MINUS_CONSTANT_COLOR:	return "GL_ONE_MINUS_CONSTANT_COLOR";
		case GL_CONSTANT_ALPHA:				return "GL_CONSTANT_ALPHA";
		case GL_ONE_MINUS_CONSTANT_ALPHA:	return "GL_ONE_MINUS_CONSTANT_ALPHA";
		default:							return nullptr;
	}
}

const char* getBlendEquationName (int value)
{
	switch (value)
	{
		case GL_FUNC_ADD:				return "GL_FUNC_ADD";
		case GL_FUNC_SUBTRACT:			return "GL_FUNC_SUBTRACT";
		case GL_FUNC_REVERSE_SUBTRACT:	return "GL_FUNC_REVERSE_SUBTRACT";
		case GL_MIN:					return "GL_MIN";
		case GL_MAX:					return "GL_MAX";
		case GL_MULTIPLY_KHR:			return "GL_MULTIPLY_KHR";
		case GL_SCREEN_KHR:				return "GL_SCREEN_KHR";
		case GL_OVERLAY_KHR:			return "GL_OVERLAY_KHR";
		case GL_DARKEN_KHR:				return "GL_DARKEN_KHR";
		case GL_LIGHTEN_KHR:			return "GL_LIGHTEN_KHR";
		case GL_COLORDODGE_KHR:			return "GL_COLORDODGE_KHR";
		case GL_COLORBURN_KHR:			return "GL_COLORBURN_KHR";
		case GL_HARDLIGHT_KHR:			return "GL_HARDLIGHT_KHR";
		case GL_SOFTLIGHT_KHR:			return "GL_SOFTLIGHT_KHR";
		case GL_DIFFERENCE_KHR:			return "GL_DIFFERENCE_KHR";
		case GL_EXCLUSION_KHR:			return "GL_EXCLUSION_KHR";
		case GL_HSL_HUE_KHR:			return "GL_HSL_HUE_KHR";
		case GL_HSL_SATURATION_KHR:		return "GL_HSL_SATURATION_KHR";
		case GL_HSL_COLOR_KHR:			return "GL_HSL_COLOR_KHR";
		case GL_HSL_LUMINOSITY_KHR:		return "GL_HSL_LUMINOSITY_KHR";
		case GL_BLEND_EQUATION:			return "GL_BLEND_EQUATION";
		default:						return nullptr;
	}
}

const char* getBufferTargetName (int value)
{
	switch (value)
	{
		case GL_ARRAY_BUFFER:				return "GL_ARRAY_BUFFER";
		case GL_ELEMENT_ARRAY_BUFFER:		return "GL_ELEMENT_ARRAY_BUFFER";
		case GL_COPY_READ_BUFFER:			return "GL_COPY_READ_BUFFER";
		case GL_COPY_WRITE_BUFFER:			return "GL_COPY_WRITE_BUFFER";
		case GL_PIXEL_PACK_BUFFER:			return "GL_PIXEL_PACK_BUFFER";
		case GL_PIXEL_UNPACK_BUFFER:		return "GL_PIXEL_UNPACK_BUFFER";
		case GL_TRANSFORM_FEEDBACK_BUFFER:	return "GL_TRANSFORM_FEEDBACK_BUFFER";
		case GL_UNIFORM_BUFFER:				return "GL_UNIFORM_BUFFER";
		case GL_DRAW_INDIRECT_BUFFER:		return "GL_DRAW_INDIRECT_BUFFER";
		case GL_SHADER_STORAGE_BUFFER:		return "GL_SHADER_STORAGE_BUFFER";
		case GL_TEXTURE_BUFFER:				return "GL_TEXTURE_BUFFER";
		case GL_DISPATCH_INDIRECT_BUFFER:	return "GL_DISPATCH_INDIRECT_BUFFER";
		case GL_ATOMIC_COUNTER_BUFFER:		return "GL_ATOMIC_COUNTER_BUFFER";
		default:							return nullptr;
	}
}

const char* getBufferBindingName (int value)
{
	switch (value)
	{
		case GL_ARRAY_BUFFER_BINDING:			return "GL_ARRAY_BUFFER_BINDING";
		case GL_ELEMENT_ARRAY_BUFFER_BINDING:	return "GL_ELEMENT_ARRAY_BUFFER_BINDING";
		default:								return nullptr;
	}
}

const char* getUsageName (int value)
{
	switch (value)
	{
		case GL_STREAM_DRAW:	return "GL_STREAM_DRAW";
		case GL_STREAM_READ:	return "GL_STREAM_READ";
		case GL_STREAM_COPY:	return "GL_STREAM_COPY";
		case GL_STATIC_DRAW:	return "GL_STATIC_DRAW";
		case GL_STATIC_READ:	return "GL_STATIC_READ";
		case GL_STATIC_COPY:	return "GL_STATIC_COPY";
		case GL_DYNAMIC_DRAW:	return "GL_DYNAMIC_DRAW";
		case GL_DYNAMIC_READ:	return "GL_DYNAMIC_READ";
		case GL_DYNAMIC_COPY:	return "GL_DYNAMIC_COPY";
		default:				return nullptr;
	}
}

const char* getBufferQueryName (int value)
{
	switch (value)
	{
		case GL_BUFFER_SIZE:			return "GL_BUFFER_SIZE";
		case GL_BUFFER_USAGE:			return "GL_BUFFER_USAGE";
		case GL_BUFFER_ACCESS_FLAGS:	return "GL_BUFFER_ACCESS_FLAGS";
		case GL_BUFFER_MAPPED:			return "GL_BUFFER_MAPPED";
		case GL_BUFFER_MAP_OFFSET:		return "GL_BUFFER_MAP_OFFSET";
		case GL_BUFFER_MAP_LENGTH:		return "GL_BUFFER_MAP_LENGTH";
		case GL_BUFFER_ACCESS:			return "GL_BUFFER_ACCESS";
		case GL_BUFFER_MAP_POINTER:		return "GL_BUFFER_MAP_POINTER";
		default:						return nullptr;
	}
}

const char* getFaceName (int value)
{
	switch (value)
	{
		case GL_FRONT:			return "GL_FRONT";
		case GL_BACK:			return "GL_BACK";
		case GL_FRONT_AND_BACK:	return "GL_FRONT_AND_BACK";
		default:				return nullptr;
	}
}

const char* getCompareFuncName (int value)
{
	switch (value)
	{
		case GL_NEVER:		return "GL_NEVER";
		case GL_LESS:		return "GL_LESS";
		case GL_LEQUAL:		return "GL_LEQUAL";
		case GL_GREATER:	return "GL_GREATER";
		case GL_EQUAL:		return "GL_EQUAL";
		case GL_NOTEQUAL:	return "GL_NOTEQUAL";
		case GL_GEQUAL:		return "GL_GEQUAL";
		case GL_ALWAYS:		return "GL_ALWAYS";
		default:			return nullptr;
	}
}

const char* getEnableCapName (int value)
{
	switch (value)
	{
		case GL_TEXTURE_2D:						return "GL_TEXTURE_2D";
		case GL_CULL_FACE:						return "GL_CULL_FACE";
		case GL_BLEND:							return "GL_BLEND";
		case GL_DITHER:							return "GL_DITHER";
		case GL_STENCIL_TEST:					return "GL_STENCIL_TEST";
		case GL_DEPTH_TEST:						return "GL_DEPTH_TEST";
		case GL_SCISSOR_TEST:					return "GL_SCISSOR_TEST";
		case GL_POLYGON_OFFSET_FILL:			return "GL_POLYGON_OFFSET_FILL";
		case GL_SAMPLE_ALPHA_TO_COVERAGE:		return "GL_SAMPLE_ALPHA_TO_COVERAGE";
		case GL_SAMPLE_COVERAGE:				return "GL_SAMPLE_COVERAGE";
		case GL_STEREO:							return "GL_STEREO";
		case GL_PRIMITIVE_RESTART_FIXED_INDEX:	return "GL_PRIMITIVE_RESTART_FIXED_INDEX";
		case GL_RASTERIZER_DISCARD:				return "GL_RASTERIZER_DISCARD";
		case GL_FRAMEBUFFER_SRGB:				return "GL_FRAMEBUFFER_SRGB";
		case GL_SAMPLE_SHADING:					return "GL_SAMPLE_SHADING";
		case GL_DEPTH_CLAMP:					return "GL_DEPTH_CLAMP";
		case GL_PRIMITIVE_RESTART:				return "GL_PRIMITIVE_RESTART";
		case GL_SAMPLE_MASK:					return "GL_SAMPLE_MASK";
		case GL_DEBUG_OUTPUT:					return "GL_DEBUG_OUTPUT";
		case GL_DEBUG_OUTPUT_SYNCHRONOUS:		return "GL_DEBUG_OUTPUT_SYNCHRONOUS";
		case GL_BLEND_ADVANCED_COHERENT_KHR:	return "GL_BLEND_ADVANCED_COHERENT_KHR";
		case GL_MULTISAMPLE:					return "GL_MULTISAMPLE";
		case GL_SAMPLE_ALPHA_TO_ONE:			return "GL_SAMPLE_ALPHA_TO_ONE";
		case GL_VERTEX_PROGRAM_POINT_SIZE:		return "GL_VERTEX_PROGRAM_POINT_SIZE";
		case GL_TEXTURE_CUBE_MAP_SEAMLESS:		return "GL_TEXTURE_CUBE_MAP_SEAMLESS";
		default:								return nullptr;
	}
}

const char* getIndexedEnableCapName (int value)
{
	switch (value)
	{
		case GL_BLEND:	return "GL_BLEND";
		default:		return nullptr;
	}
}

const char* getWindingName (int value)
{
	switch (value)
	{
		case GL_CW:		return "GL_CW";
		case GL_CCW:	return "GL_CCW";
		default:		return nullptr;
	}
}

const char* getHintModeName (int value)
{
	switch (value)
	{
		case GL_DONT_CARE:	return "GL_DONT_CARE";
		case GL_FASTEST:	return "GL_FASTEST";
		case GL_NICEST:		return "GL_NICEST";
		default:			return nullptr;
	}
}

const char* getHintName (int value)
{
	switch (value)
	{
		case GL_GENERATE_MIPMAP_HINT:		return "GL_GENERATE_MIPMAP_HINT";
		case GL_TEXTURE_COMPRESSION_HINT:	return "GL_TEXTURE_COMPRESSION_HINT";
		default:							return nullptr;
	}
}

const char* getStencilOpName (int value)
{
	switch (value)
	{
		case GL_ZERO:		return "GL_ZERO";
		case GL_KEEP:		return "GL_KEEP";
		case GL_REPLACE:	return "GL_REPLACE";
		case GL_INCR:		return "GL_INCR";
		case GL_DECR:		return "GL_DECR";
		case GL_INVERT:		return "GL_INVERT";
		case GL_INCR_WRAP:	return "GL_INCR_WRAP";
		case GL_DECR_WRAP:	return "GL_DECR_WRAP";
		default:			return nullptr;
	}
}

const char* getShaderTypeName (int value)
{
	switch (value)
	{
		case GL_VERTEX_SHADER:			return "GL_VERTEX_SHADER";
		case GL_FRAGMENT_SHADER:		return "GL_FRAGMENT_SHADER";
		case GL_COMPUTE_SHADER:			return "GL_COMPUTE_SHADER";
		case GL_TESS_CONTROL_SHADER:	return "GL_TESS_CONTROL_SHADER";
		case GL_TESS_EVALUATION_SHADER:	return "GL_TESS_EVALUATION_SHADER";
		case GL_GEOMETRY_SHADER:		return "GL_GEOMETRY_SHADER";
		default:						return nullptr;
	}
}

const char* getBufferName (int value)
{
	switch (value)
	{
		case GL_COLOR:			return "GL_COLOR";
		case GL_DEPTH:			return "GL_DEPTH";
		case GL_STENCIL:		return "GL_STENCIL";
		case GL_DEPTH_STENCIL:	return "GL_DEPTH_STENCIL";
		default:				return nullptr;
	}
}

const char* getInvalidateAttachmentName (int value)
{
	switch (value)
	{
		case GL_COLOR:						return "GL_COLOR";
		case GL_DEPTH:						return "GL_DEPTH";
		case GL_STENCIL:					return "GL_STENCIL";
		case GL_COLOR_ATTACHMENT0:			return "GL_COLOR_ATTACHMENT0";
		case GL_COLOR_ATTACHMENT1:			return "GL_COLOR_ATTACHMENT1";
		case GL_COLOR_ATTACHMENT2:			return "GL_COLOR_ATTACHMENT2";
		case GL_COLOR_ATTACHMENT3:			return "GL_COLOR_ATTACHMENT3";
		case GL_COLOR_ATTACHMENT4:			return "GL_COLOR_ATTACHMENT4";
		case GL_COLOR_ATTACHMENT5:			return "GL_COLOR_ATTACHMENT5";
		case GL_COLOR_ATTACHMENT6:			return "GL_COLOR_ATTACHMENT6";
		case GL_COLOR_ATTACHMENT7:			return "GL_COLOR_ATTACHMENT7";
		case GL_DEPTH_ATTACHMENT:			return "GL_DEPTH_ATTACHMENT";
		case GL_STENCIL_ATTACHMENT:			return "GL_STENCIL_ATTACHMENT";
		case GL_DEPTH_STENCIL_ATTACHMENT:	return "GL_DEPTH_STENCIL_ATTACHMENT";
		default:							return nullptr;
	}
}

const char* getDrawReadBufferName (int value)
{
	switch (value)
	{
		case GL_COLOR:						return "GL_COLOR";
		case GL_DEPTH:						return "GL_DEPTH";
		case GL_STENCIL:					return "GL_STENCIL";
		case GL_BACK:						return "GL_BACK";
		case GL_NONE:						return "GL_NONE";
		case GL_COLOR_ATTACHMENT0:			return "GL_COLOR_ATTACHMENT0";
		case GL_COLOR_ATTACHMENT1:			return "GL_COLOR_ATTACHMENT1";
		case GL_COLOR_ATTACHMENT2:			return "GL_COLOR_ATTACHMENT2";
		case GL_COLOR_ATTACHMENT3:			return "GL_COLOR_ATTACHMENT3";
		case GL_COLOR_ATTACHMENT4:			return "GL_COLOR_ATTACHMENT4";
		case GL_COLOR_ATTACHMENT5:			return "GL_COLOR_ATTACHMENT5";
		case GL_COLOR_ATTACHMENT6:			return "GL_COLOR_ATTACHMENT6";
		case GL_COLOR_ATTACHMENT7:			return "GL_COLOR_ATTACHMENT7";
		case GL_DEPTH_ATTACHMENT:			return "GL_DEPTH_ATTACHMENT";
		case GL_STENCIL_ATTACHMENT:			return "GL_STENCIL_ATTACHMENT";
		case GL_DEPTH_STENCIL_ATTACHMENT:	return "GL_DEPTH_STENCIL_ATTACHMENT";
		default:							return nullptr;
	}
}

const char* getTextureTargetName (int value)
{
	switch (value)
	{
		case GL_TEXTURE_1D:							return "GL_TEXTURE_1D";
		case GL_TEXTURE_1D_ARRAY:					return "GL_TEXTURE_1D_ARRAY";
		case GL_PROXY_TEXTURE_1D:					return "GL_PROXY_TEXTURE_1D";
		case GL_PROXY_TEXTURE_1D_ARRAY:				return "GL_PROXY_TEXTURE_1D_ARRAY";
		case GL_TEXTURE_2D:							return "GL_TEXTURE_2D";
		case GL_TEXTURE_RECTANGLE:					return "GL_TEXTURE_RECTANGLE";
		case GL_PROXY_TEXTURE_2D:					return "GL_PROXY_TEXTURE_2D";
		case GL_TEXTURE_CUBE_MAP:					return "GL_TEXTURE_CUBE_MAP";
		case GL_PROXY_TEXTURE_CUBE_MAP:				return "GL_PROXY_TEXTURE_CUBE_MAP";
		case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY:		return "GL_PROXY_TEXTURE_CUBE_MAP_ARRAY";
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:		return "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:		return "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:		return "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:		return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:		return "GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:		return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
		case GL_TEXTURE_3D:							return "GL_TEXTURE_3D";
		case GL_PROXY_TEXTURE_3D:					return "GL_PROXY_TEXTURE_3D";
		case GL_TEXTURE_2D_ARRAY:					return "GL_TEXTURE_2D_ARRAY";
		case GL_PROXY_TEXTURE_2D_ARRAY:				return "GL_PROXY_TEXTURE_2D_ARRAY";
		case GL_TEXTURE_2D_MULTISAMPLE:				return "GL_TEXTURE_2D_MULTISAMPLE";
		case GL_PROXY_TEXTURE_2D_MULTISAMPLE:		return "GL_PROXY_TEXTURE_2D_MULTISAMPLE";
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:		return "GL_TEXTURE_2D_MULTISAMPLE_ARRAY";
		case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:	return "GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY";
		case GL_PROXY_TEXTURE_RECTANGLE:			return "GL_PROXY_TEXTURE_RECTANGLE";
		case GL_TEXTURE_BUFFER:						return "GL_TEXTURE_BUFFER";
		case GL_TEXTURE_CUBE_MAP_ARRAY:				return "GL_TEXTURE_CUBE_MAP_ARRAY";
		default:									return nullptr;
	}
}

const char* getTextureParameterName (int value)
{
	switch (value)
	{
		case GL_TEXTURE_WRAP_S:				return "GL_TEXTURE_WRAP_S";
		case GL_TEXTURE_WRAP_T:				return "GL_TEXTURE_WRAP_T";
		case GL_TEXTURE_WRAP_R:				return "GL_TEXTURE_WRAP_R";
		case GL_TEXTURE_BASE_LEVEL:			return "GL_TEXTURE_BASE_LEVEL";
		case GL_TEXTURE_MAX_LEVEL:			return "GL_TEXTURE_MAX_LEVEL";
		case GL_TEXTURE_COMPARE_MODE:		return "GL_TEXTURE_COMPARE_MODE";
		case GL_TEXTURE_COMPARE_FUNC:		return "GL_TEXTURE_COMPARE_FUNC";
		case GL_TEXTURE_MAX_LOD:			return "GL_TEXTURE_MAX_LOD";
		case GL_TEXTURE_MIN_LOD:			return "GL_TEXTURE_MIN_LOD";
		case GL_TEXTURE_SWIZZLE_R:			return "GL_TEXTURE_SWIZZLE_R";
		case GL_TEXTURE_SWIZZLE_G:			return "GL_TEXTURE_SWIZZLE_G";
		case GL_TEXTURE_SWIZZLE_B:			return "GL_TEXTURE_SWIZZLE_B";
		case GL_TEXTURE_SWIZZLE_A:			return "GL_TEXTURE_SWIZZLE_A";
		case GL_TEXTURE_SWIZZLE_RGBA:		return "GL_TEXTURE_SWIZZLE_RGBA";
		case GL_TEXTURE_MIN_FILTER:			return "GL_TEXTURE_MIN_FILTER";
		case GL_TEXTURE_MAG_FILTER:			return "GL_TEXTURE_MAG_FILTER";
		case GL_DEPTH_STENCIL_TEXTURE_MODE:	return "GL_DEPTH_STENCIL_TEXTURE_MODE";
		case GL_TEXTURE_SRGB_DECODE_EXT:	return "GL_TEXTURE_SRGB_DECODE_EXT";
		case GL_TEXTURE_BORDER_COLOR:		return "GL_TEXTURE_BORDER_COLOR";
		case GL_TEXTURE_IMMUTABLE_LEVELS:	return "GL_TEXTURE_IMMUTABLE_LEVELS";
		case GL_TEXTURE_IMMUTABLE_FORMAT:	return "GL_TEXTURE_IMMUTABLE_FORMAT";
		default:							return nullptr;
	}
}

const char* getTextureLevelParameterName (int value)
{
	switch (value)
	{
		case GL_TEXTURE_SAMPLES:					return "GL_TEXTURE_SAMPLES";
		case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:		return "GL_TEXTURE_FIXED_SAMPLE_LOCATIONS";
		case GL_TEXTURE_WIDTH:						return "GL_TEXTURE_WIDTH";
		case GL_TEXTURE_HEIGHT:						return "GL_TEXTURE_HEIGHT";
		case GL_TEXTURE_DEPTH:						return "GL_TEXTURE_DEPTH";
		case GL_TEXTURE_INTERNAL_FORMAT:			return "GL_TEXTURE_INTERNAL_FORMAT";
		case GL_TEXTURE_RED_SIZE:					return "GL_TEXTURE_RED_SIZE";
		case GL_TEXTURE_GREEN_SIZE:					return "GL_TEXTURE_GREEN_SIZE";
		case GL_TEXTURE_BLUE_SIZE:					return "GL_TEXTURE_BLUE_SIZE";
		case GL_TEXTURE_ALPHA_SIZE:					return "GL_TEXTURE_ALPHA_SIZE";
		case GL_TEXTURE_DEPTH_SIZE:					return "GL_TEXTURE_DEPTH_SIZE";
		case GL_TEXTURE_STENCIL_SIZE:				return "GL_TEXTURE_STENCIL_SIZE";
		case GL_TEXTURE_SHARED_SIZE:				return "GL_TEXTURE_SHARED_SIZE";
		case GL_TEXTURE_RED_TYPE:					return "GL_TEXTURE_RED_TYPE";
		case GL_TEXTURE_GREEN_TYPE:					return "GL_TEXTURE_GREEN_TYPE";
		case GL_TEXTURE_BLUE_TYPE:					return "GL_TEXTURE_BLUE_TYPE";
		case GL_TEXTURE_ALPHA_TYPE:					return "GL_TEXTURE_ALPHA_TYPE";
		case GL_TEXTURE_DEPTH_TYPE:					return "GL_TEXTURE_DEPTH_TYPE";
		case GL_TEXTURE_COMPRESSED:					return "GL_TEXTURE_COMPRESSED";
		case GL_TEXTURE_BUFFER_DATA_STORE_BINDING:	return "GL_TEXTURE_BUFFER_DATA_STORE_BINDING";
		case GL_TEXTURE_BUFFER_OFFSET:				return "GL_TEXTURE_BUFFER_OFFSET";
		case GL_TEXTURE_BUFFER_SIZE:				return "GL_TEXTURE_BUFFER_SIZE";
		case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:		return "GL_TEXTURE_COMPRESSED_IMAGE_SIZE";
		default:									return nullptr;
	}
}

const char* getRepeatModeName (int value)
{
	switch (value)
	{
		case GL_REPEAT:				return "GL_REPEAT";
		case GL_CLAMP_TO_EDGE:		return "GL_CLAMP_TO_EDGE";
		case GL_MIRRORED_REPEAT:	return "GL_MIRRORED_REPEAT";
		case GL_CLAMP_TO_BORDER:	return "GL_CLAMP_TO_BORDER";
		default:					return nullptr;
	}
}

const char* getTextureFilterName (int value)
{
	switch (value)
	{
		case GL_NEAREST:				return "GL_NEAREST";
		case GL_LINEAR:					return "GL_LINEAR";
		case GL_NEAREST_MIPMAP_NEAREST:	return "GL_NEAREST_MIPMAP_NEAREST";
		case GL_LINEAR_MIPMAP_NEAREST:	return "GL_LINEAR_MIPMAP_NEAREST";
		case GL_NEAREST_MIPMAP_LINEAR:	return "GL_NEAREST_MIPMAP_LINEAR";
		case GL_LINEAR_MIPMAP_LINEAR:	return "GL_LINEAR_MIPMAP_LINEAR";
		default:						return nullptr;
	}
}

const char* getTextureWrapModeName (int value)
{
	switch (value)
	{
		case GL_REPEAT:				return "GL_REPEAT";
		case GL_CLAMP_TO_EDGE:		return "GL_CLAMP_TO_EDGE";
		case GL_MIRRORED_REPEAT:	return "GL_MIRRORED_REPEAT";
		default:					return nullptr;
	}
}

const char* getTextureSwizzleName (int value)
{
	switch (value)
	{
		case GL_RED:	return "GL_RED";
		case GL_GREEN:	return "GL_GREEN";
		case GL_BLUE:	return "GL_BLUE";
		case GL_ALPHA:	return "GL_ALPHA";
		case GL_ZERO:	return "GL_ZERO";
		case GL_ONE:	return "GL_ONE";
		default:		return nullptr;
	}
}

const char* getTextureCompareModeName (int value)
{
	switch (value)
	{
		case GL_NONE:					return "GL_NONE";
		case GL_COMPARE_REF_TO_TEXTURE:	return "GL_COMPARE_REF_TO_TEXTURE";
		default:						return nullptr;
	}
}

const char* getCubeMapFaceName (int value)
{
	switch (value)
	{
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:	return "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:	return "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:	return "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:	return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:	return "GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:	return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
		default:								return nullptr;
	}
}

const char* getTextureDepthStencilModeName (int value)
{
	switch (value)
	{
		case GL_DEPTH_COMPONENT:	return "GL_DEPTH_COMPONENT";
		case GL_STENCIL_INDEX:		return "GL_STENCIL_INDEX";
		default:					return nullptr;
	}
}

const char* getPixelStoreParameterName (int value)
{
	switch (value)
	{
		case GL_UNPACK_ALIGNMENT:		return "GL_UNPACK_ALIGNMENT";
		case GL_UNPACK_IMAGE_HEIGHT:	return "GL_UNPACK_IMAGE_HEIGHT";
		case GL_UNPACK_ROW_LENGTH:		return "GL_UNPACK_ROW_LENGTH";
		case GL_UNPACK_SKIP_IMAGES:		return "GL_UNPACK_SKIP_IMAGES";
		case GL_UNPACK_SKIP_ROWS:		return "GL_UNPACK_SKIP_ROWS";
		case GL_UNPACK_SKIP_PIXELS:		return "GL_UNPACK_SKIP_PIXELS";
		case GL_PACK_ROW_LENGTH:		return "GL_PACK_ROW_LENGTH";
		case GL_PACK_SKIP_ROWS:			return "GL_PACK_SKIP_ROWS";
		case GL_PACK_SKIP_PIXELS:		return "GL_PACK_SKIP_PIXELS";
		default:						return nullptr;
	}
}

const char* getUncompressedTextureFormatName (int value)
{
	switch (value)
	{
		case GL_LUMINANCE:			return "GL_LUMINANCE";
		case GL_LUMINANCE_ALPHA:	return "GL_LUMINANCE_ALPHA";
		case GL_ALPHA:				return "GL_ALPHA";
		case GL_RGB:				return "GL_RGB";
		case GL_RGBA:				return "GL_RGBA";
		case GL_RGBA4:				return "GL_RGBA4";
		case GL_RGB4:				return "GL_RGB4";
		case GL_RGB5_A1:			return "GL_RGB5_A1";
		case GL_RGB565:				return "GL_RGB565";
		case GL_R3_G3_B2:			return "GL_R3_G3_B2";
		case GL_RGB5:				return "GL_RGB5";
		case GL_RGB12:				return "GL_RGB12";
		case GL_RGBA2:				return "GL_RGBA2";
		case GL_RGBA12:				return "GL_RGBA12";
		case GL_DEPTH_COMPONENT16:	return "GL_DEPTH_COMPONENT16";
		case GL_STENCIL_INDEX1:		return "GL_STENCIL_INDEX1";
		case GL_STENCIL_INDEX4:		return "GL_STENCIL_INDEX4";
		case GL_STENCIL_INDEX8:		return "GL_STENCIL_INDEX8";
		case GL_STENCIL_INDEX16:	return "GL_STENCIL_INDEX16";
		case GL_RG:					return "GL_RG";
		case GL_RED:				return "GL_RED";
		case GL_RGBA_INTEGER:		return "GL_RGBA_INTEGER";
		case GL_RGB_INTEGER:		return "GL_RGB_INTEGER";
		case GL_RG_INTEGER:			return "GL_RG_INTEGER";
		case GL_RED_INTEGER:		return "GL_RED_INTEGER";
		case GL_GREEN_INTEGER:		return "GL_GREEN_INTEGER";
		case GL_BLUE_INTEGER:		return "GL_BLUE_INTEGER";
		case GL_BGR_INTEGER:		return "GL_BGR_INTEGER";
		case GL_BGRA_INTEGER:		return "GL_BGRA_INTEGER";
		case GL_DEPTH_COMPONENT:	return "GL_DEPTH_COMPONENT";
		case GL_DEPTH_STENCIL:		return "GL_DEPTH_STENCIL";
		case GL_RGBA32F:			return "GL_RGBA32F";
		case GL_RGBA32I:			return "GL_RGBA32I";
		case GL_RGBA32UI:			return "GL_RGBA32UI";
		case GL_RGBA16:				return "GL_RGBA16";
		case GL_RGBA16_SNORM:		return "GL_RGBA16_SNORM";
		case GL_RGBA16F:			return "GL_RGBA16F";
		case GL_RGBA16I:			return "GL_RGBA16I";
		case GL_RGBA16UI:			return "GL_RGBA16UI";
		case GL_RGBA8:				return "GL_RGBA8";
		case GL_RGBA8I:				return "GL_RGBA8I";
		case GL_RGBA8UI:			return "GL_RGBA8UI";
		case GL_SRGB8_ALPHA8:		return "GL_SRGB8_ALPHA8";
		case GL_RGB10_A2:			return "GL_RGB10_A2";
		case GL_RGB10_A2UI:			return "GL_RGB10_A2UI";
		case GL_RGBA8_SNORM:		return "GL_RGBA8_SNORM";
		case GL_RGB8:				return "GL_RGB8";
		case GL_R11F_G11F_B10F:		return "GL_R11F_G11F_B10F";
		case GL_RGB32F:				return "GL_RGB32F";
		case GL_RGB32I:				return "GL_RGB32I";
		case GL_RGB32UI:			return "GL_RGB32UI";
		case GL_RGB16:				return "GL_RGB16";
		case GL_RGB16_SNORM:		return "GL_RGB16_SNORM";
		case GL_RGB16F:				return "GL_RGB16F";
		case GL_RGB16I:				return "GL_RGB16I";
		case GL_RGB16UI:			return "GL_RGB16UI";
		case GL_RGB8_SNORM:			return "GL_RGB8_SNORM";
		case GL_RGB8I:				return "GL_RGB8I";
		case GL_RGB8UI:				return "GL_RGB8UI";
		case GL_SRGB8:				return "GL_SRGB8";
		case GL_RGB9_E5:			return "GL_RGB9_E5";
		case GL_RG32F:				return "GL_RG32F";
		case GL_RG32I:				return "GL_RG32I";
		case GL_RG32UI:				return "GL_RG32UI";
		case GL_RG16:				return "GL_RG16";
		case GL_RG16_SNORM:			return "GL_RG16_SNORM";
		case GL_RG16F:				return "GL_RG16F";
		case GL_RG16I:				return "GL_RG16I";
		case GL_RG16UI:				return "GL_RG16UI";
		case GL_RG8:				return "GL_RG8";
		case GL_RG8I:				return "GL_RG8I";
		case GL_RG8UI:				return "GL_RG8UI";
		case GL_RG8_SNORM:			return "GL_RG8_SNORM";
		case GL_R32F:				return "GL_R32F";
		case GL_R32I:				return "GL_R32I";
		case GL_R32UI:				return "GL_R32UI";
		case GL_R16:				return "GL_R16";
		case GL_R16_SNORM:			return "GL_R16_SNORM";
		case GL_R16F:				return "GL_R16F";
		case GL_R16I:				return "GL_R16I";
		case GL_R16UI:				return "GL_R16UI";
		case GL_R8:					return "GL_R8";
		case GL_R8I:				return "GL_R8I";
		case GL_R8UI:				return "GL_R8UI";
		case GL_R8_SNORM:			return "GL_R8_SNORM";
		case GL_DEPTH_COMPONENT32F:	return "GL_DEPTH_COMPONENT32F";
		case GL_DEPTH_COMPONENT24:	return "GL_DEPTH_COMPONENT24";
		case GL_DEPTH32F_STENCIL8:	return "GL_DEPTH32F_STENCIL8";
		case GL_DEPTH24_STENCIL8:	return "GL_DEPTH24_STENCIL8";
		case GL_RGB10:				return "GL_RGB10";
		case GL_DEPTH_COMPONENT32:	return "GL_DEPTH_COMPONENT32";
		case GL_SRGB:				return "GL_SRGB";
		case GL_SRGB_ALPHA:			return "GL_SRGB_ALPHA";
		case GL_SR8_EXT:			return "GL_SR8_EXT";
		case GL_SRG8_EXT:			return "GL_SRG8_EXT";
		case GL_BGRA:				return "GL_BGRA";
		case GL_BGRA8_EXT:			return "GL_BGRA8_EXT";
		case GL_BGR:				return "GL_BGR";
		case GL_RED_SNORM:			return "GL_RED_SNORM";
		case GL_RG_SNORM:			return "GL_RG_SNORM";
		case GL_RGB_SNORM:			return "GL_RGB_SNORM";
		case GL_RGBA_SNORM:			return "GL_RGBA_SNORM";
		default:					return nullptr;
	}
}

const char* getCompressedTextureFormatName (int value)
{
	switch (value)
	{
		case GL_COMPRESSED_R11_EAC:							return "GL_COMPRESSED_R11_EAC";
		case GL_COMPRESSED_SIGNED_R11_EAC:					return "GL_COMPRESSED_SIGNED_R11_EAC";
		case GL_COMPRESSED_RG11_EAC:						return "GL_COMPRESSED_RG11_EAC";
		case GL_COMPRESSED_SIGNED_RG11_EAC:					return "GL_COMPRESSED_SIGNED_RG11_EAC";
		case GL_COMPRESSED_RGB8_ETC2:						return "GL_COMPRESSED_RGB8_ETC2";
		case GL_COMPRESSED_SRGB8_ETC2:						return "GL_COMPRESSED_SRGB8_ETC2";
		case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:	return "GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2";
		case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:	return "GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2";
		case GL_COMPRESSED_RGBA8_ETC2_EAC:					return "GL_COMPRESSED_RGBA8_ETC2_EAC";
		case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:			return "GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC";
		case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:				return "GL_COMPRESSED_RGBA_ASTC_4x4_KHR";
		case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:				return "GL_COMPRESSED_RGBA_ASTC_5x4_KHR";
		case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:				return "GL_COMPRESSED_RGBA_ASTC_5x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:				return "GL_COMPRESSED_RGBA_ASTC_6x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:				return "GL_COMPRESSED_RGBA_ASTC_6x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:				return "GL_COMPRESSED_RGBA_ASTC_8x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:				return "GL_COMPRESSED_RGBA_ASTC_8x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:				return "GL_COMPRESSED_RGBA_ASTC_8x8_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:				return "GL_COMPRESSED_RGBA_ASTC_10x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:				return "GL_COMPRESSED_RGBA_ASTC_10x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:				return "GL_COMPRESSED_RGBA_ASTC_10x8_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:				return "GL_COMPRESSED_RGBA_ASTC_10x10_KHR";
		case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:				return "GL_COMPRESSED_RGBA_ASTC_12x10_KHR";
		case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:				return "GL_COMPRESSED_RGBA_ASTC_12x12_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR";
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:				return "GL_COMPRESSED_RGB_S3TC_DXT1_EXT";
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:				return "GL_COMPRESSED_RGBA_S3TC_DXT1_EXT";
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:				return "GL_COMPRESSED_RGBA_S3TC_DXT3_EXT";
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:				return "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT";
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:			return "GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG";
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:			return "GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG";
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:			return "GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG";
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:			return "GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG";
		case GL_COMPRESSED_RED:								return "GL_COMPRESSED_RED";
		case GL_COMPRESSED_RG:								return "GL_COMPRESSED_RG";
		case GL_COMPRESSED_RGB:								return "GL_COMPRESSED_RGB";
		case GL_COMPRESSED_RGBA:							return "GL_COMPRESSED_RGBA";
		case GL_COMPRESSED_SRGB:							return "GL_COMPRESSED_SRGB";
		case GL_COMPRESSED_SRGB_ALPHA:						return "GL_COMPRESSED_SRGB_ALPHA";
		case GL_COMPRESSED_RED_RGTC1:						return "GL_COMPRESSED_RED_RGTC1";
		case GL_COMPRESSED_SIGNED_RED_RGTC1:				return "GL_COMPRESSED_SIGNED_RED_RGTC1";
		case GL_COMPRESSED_RG_RGTC2:						return "GL_COMPRESSED_RG_RGTC2";
		case GL_COMPRESSED_SIGNED_RG_RGTC2:					return "GL_COMPRESSED_SIGNED_RG_RGTC2";
		case GL_COMPRESSED_RGBA_BPTC_UNORM:					return "GL_COMPRESSED_RGBA_BPTC_UNORM";
		case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:			return "GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM";
		case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:			return "GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT";
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:			return "GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT";
		default:											return nullptr;
	}
}

const char* getShaderVarTypeName (int value)
{
	switch (value)
	{
		case GL_FLOAT:										return "GL_FLOAT";
		case GL_FLOAT_VEC2:									return "GL_FLOAT_VEC2";
		case GL_FLOAT_VEC3:									return "GL_FLOAT_VEC3";
		case GL_FLOAT_VEC4:									return "GL_FLOAT_VEC4";
		case GL_INT:										return "GL_INT";
		case GL_INT_VEC2:									return "GL_INT_VEC2";
		case GL_INT_VEC3:									return "GL_INT_VEC3";
		case GL_INT_VEC4:									return "GL_INT_VEC4";
		case GL_UNSIGNED_INT:								return "GL_UNSIGNED_INT";
		case GL_UNSIGNED_INT_VEC2:							return "GL_UNSIGNED_INT_VEC2";
		case GL_UNSIGNED_INT_VEC3:							return "GL_UNSIGNED_INT_VEC3";
		case GL_UNSIGNED_INT_VEC4:							return "GL_UNSIGNED_INT_VEC4";
		case GL_BOOL:										return "GL_BOOL";
		case GL_BOOL_VEC2:									return "GL_BOOL_VEC2";
		case GL_BOOL_VEC3:									return "GL_BOOL_VEC3";
		case GL_BOOL_VEC4:									return "GL_BOOL_VEC4";
		case GL_FLOAT_MAT2:									return "GL_FLOAT_MAT2";
		case GL_FLOAT_MAT3:									return "GL_FLOAT_MAT3";
		case GL_FLOAT_MAT4:									return "GL_FLOAT_MAT4";
		case GL_FLOAT_MAT2x3:								return "GL_FLOAT_MAT2x3";
		case GL_FLOAT_MAT2x4:								return "GL_FLOAT_MAT2x4";
		case GL_FLOAT_MAT3x2:								return "GL_FLOAT_MAT3x2";
		case GL_FLOAT_MAT3x4:								return "GL_FLOAT_MAT3x4";
		case GL_FLOAT_MAT4x2:								return "GL_FLOAT_MAT4x2";
		case GL_FLOAT_MAT4x3:								return "GL_FLOAT_MAT4x3";
		case GL_SAMPLER_1D:									return "GL_SAMPLER_1D";
		case GL_SAMPLER_1D_ARRAY:							return "GL_SAMPLER_1D_ARRAY";
		case GL_SAMPLER_1D_SHADOW:							return "GL_SAMPLER_1D_SHADOW";
		case GL_SAMPLER_2D:									return "GL_SAMPLER_2D";
		case GL_SAMPLER_2D_RECT:							return "GL_SAMPLER_2D_RECT";
		case GL_SAMPLER_2D_RECT_SHADOW:						return "GL_SAMPLER_2D_RECT_SHADOW";
		case GL_SAMPLER_3D:									return "GL_SAMPLER_3D";
		case GL_SAMPLER_CUBE:								return "GL_SAMPLER_CUBE";
		case GL_SAMPLER_1D_ARRAY_SHADOW:					return "GL_SAMPLER_1D_ARRAY_SHADOW";
		case GL_SAMPLER_2D_SHADOW:							return "GL_SAMPLER_2D_SHADOW";
		case GL_SAMPLER_2D_ARRAY:							return "GL_SAMPLER_2D_ARRAY";
		case GL_SAMPLER_2D_ARRAY_SHADOW:					return "GL_SAMPLER_2D_ARRAY_SHADOW";
		case GL_SAMPLER_CUBE_SHADOW:						return "GL_SAMPLER_CUBE_SHADOW";
		case GL_INT_SAMPLER_1D:								return "GL_INT_SAMPLER_1D";
		case GL_INT_SAMPLER_2D:								return "GL_INT_SAMPLER_2D";
		case GL_INT_SAMPLER_2D_RECT:						return "GL_INT_SAMPLER_2D_RECT";
		case GL_INT_SAMPLER_3D:								return "GL_INT_SAMPLER_3D";
		case GL_INT_SAMPLER_CUBE:							return "GL_INT_SAMPLER_CUBE";
		case GL_INT_SAMPLER_2D_ARRAY:						return "GL_INT_SAMPLER_2D_ARRAY";
		case GL_INT_SAMPLER_1D_ARRAY:						return "GL_INT_SAMPLER_1D_ARRAY";
		case GL_UNSIGNED_INT_SAMPLER_1D:					return "GL_UNSIGNED_INT_SAMPLER_1D";
		case GL_UNSIGNED_INT_SAMPLER_2D:					return "GL_UNSIGNED_INT_SAMPLER_2D";
		case GL_UNSIGNED_INT_SAMPLER_2D_RECT:				return "GL_UNSIGNED_INT_SAMPLER_2D_RECT";
		case GL_UNSIGNED_INT_SAMPLER_3D:					return "GL_UNSIGNED_INT_SAMPLER_3D";
		case GL_UNSIGNED_INT_SAMPLER_CUBE:					return "GL_UNSIGNED_INT_SAMPLER_CUBE";
		case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:				return "GL_UNSIGNED_INT_SAMPLER_1D_ARRAY";
		case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:				return "GL_UNSIGNED_INT_SAMPLER_2D_ARRAY";
		case GL_SAMPLER_2D_MULTISAMPLE:						return "GL_SAMPLER_2D_MULTISAMPLE";
		case GL_INT_SAMPLER_2D_MULTISAMPLE:					return "GL_INT_SAMPLER_2D_MULTISAMPLE";
		case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:		return "GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE";
		case GL_IMAGE_2D:									return "GL_IMAGE_2D";
		case GL_IMAGE_3D:									return "GL_IMAGE_3D";
		case GL_IMAGE_CUBE:									return "GL_IMAGE_CUBE";
		case GL_IMAGE_2D_ARRAY:								return "GL_IMAGE_2D_ARRAY";
		case GL_INT_IMAGE_2D:								return "GL_INT_IMAGE_2D";
		case GL_INT_IMAGE_3D:								return "GL_INT_IMAGE_3D";
		case GL_INT_IMAGE_CUBE:								return "GL_INT_IMAGE_CUBE";
		case GL_INT_IMAGE_2D_ARRAY:							return "GL_INT_IMAGE_2D_ARRAY";
		case GL_UNSIGNED_INT_IMAGE_2D:						return "GL_UNSIGNED_INT_IMAGE_2D";
		case GL_UNSIGNED_INT_IMAGE_3D:						return "GL_UNSIGNED_INT_IMAGE_3D";
		case GL_UNSIGNED_INT_IMAGE_CUBE:					return "GL_UNSIGNED_INT_IMAGE_CUBE";
		case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:				return "GL_UNSIGNED_INT_IMAGE_2D_ARRAY";
		case GL_UNSIGNED_INT_ATOMIC_COUNTER:				return "GL_UNSIGNED_INT_ATOMIC_COUNTER";
		case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:				return "GL_SAMPLER_2D_MULTISAMPLE_ARRAY";
		case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:			return "GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY";
		case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:	return "GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY";
		case GL_SAMPLER_BUFFER:								return "GL_SAMPLER_BUFFER";
		case GL_INT_SAMPLER_BUFFER:							return "GL_INT_SAMPLER_BUFFER";
		case GL_UNSIGNED_INT_SAMPLER_BUFFER:				return "GL_UNSIGNED_INT_SAMPLER_BUFFER";
		case GL_IMAGE_BUFFER:								return "GL_IMAGE_BUFFER";
		case GL_INT_IMAGE_BUFFER:							return "GL_INT_IMAGE_BUFFER";
		case GL_UNSIGNED_INT_IMAGE_BUFFER:					return "GL_UNSIGNED_INT_IMAGE_BUFFER";
		case GL_SAMPLER_CUBE_MAP_ARRAY:						return "GL_SAMPLER_CUBE_MAP_ARRAY";
		case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:				return "GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW";
		case GL_INT_SAMPLER_CUBE_MAP_ARRAY:					return "GL_INT_SAMPLER_CUBE_MAP_ARRAY";
		case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:		return "GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY";
		case GL_IMAGE_CUBE_MAP_ARRAY:						return "GL_IMAGE_CUBE_MAP_ARRAY";
		case GL_INT_IMAGE_CUBE_MAP_ARRAY:					return "GL_INT_IMAGE_CUBE_MAP_ARRAY";
		case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:			return "GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY";
		default:											return nullptr;
	}
}

const char* getShaderParamName (int value)
{
	switch (value)
	{
		case GL_SHADER_TYPE:			return "GL_SHADER_TYPE";
		case GL_DELETE_STATUS:			return "GL_DELETE_STATUS";
		case GL_COMPILE_STATUS:			return "GL_COMPILE_STATUS";
		case GL_INFO_LOG_LENGTH:		return "GL_INFO_LOG_LENGTH";
		case GL_SHADER_SOURCE_LENGTH:	return "GL_SHADER_SOURCE_LENGTH";
		default:						return nullptr;
	}
}

const char* getVertexAttribParameterNameName (int value)
{
	switch (value)
	{
		case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:	return "GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING";
		case GL_VERTEX_ATTRIB_ARRAY_ENABLED:		return "GL_VERTEX_ATTRIB_ARRAY_ENABLED";
		case GL_VERTEX_ATTRIB_ARRAY_SIZE:			return "GL_VERTEX_ATTRIB_ARRAY_SIZE";
		case GL_VERTEX_ATTRIB_ARRAY_STRIDE:			return "GL_VERTEX_ATTRIB_ARRAY_STRIDE";
		case GL_VERTEX_ATTRIB_ARRAY_TYPE:			return "GL_VERTEX_ATTRIB_ARRAY_TYPE";
		case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:		return "GL_VERTEX_ATTRIB_ARRAY_NORMALIZED";
		case GL_VERTEX_ATTRIB_ARRAY_INTEGER:		return "GL_VERTEX_ATTRIB_ARRAY_INTEGER";
		case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:		return "GL_VERTEX_ATTRIB_ARRAY_DIVISOR";
		case GL_CURRENT_VERTEX_ATTRIB:				return "GL_CURRENT_VERTEX_ATTRIB";
		case GL_VERTEX_ATTRIB_BINDING:				return "GL_VERTEX_ATTRIB_BINDING";
		case GL_VERTEX_ATTRIB_RELATIVE_OFFSET:		return "GL_VERTEX_ATTRIB_RELATIVE_OFFSET";
		case GL_VERTEX_ATTRIB_ARRAY_POINTER:		return "GL_VERTEX_ATTRIB_ARRAY_POINTER";
		default:									return nullptr;
	}
}

const char* getBooleanName (int value)
{
	switch (value)
	{
		case GL_FALSE:	return "GL_FALSE";
		case GL_TRUE:	return "GL_TRUE";
		default:		return nullptr;
	}
}

const char* getGettableStateName (int value)
{
	switch (value)
	{
		case GL_ELEMENT_ARRAY_BUFFER_BINDING:						return "GL_ELEMENT_ARRAY_BUFFER_BINDING";
		case GL_ARRAY_BUFFER_BINDING:								return "GL_ARRAY_BUFFER_BINDING";
		case GL_VERTEX_ARRAY_BINDING:								return "GL_VERTEX_ARRAY_BINDING";
		case GL_VIEWPORT:											return "GL_VIEWPORT";
		case GL_DEPTH_RANGE:										return "GL_DEPTH_RANGE";
		case GL_TRANSFORM_FEEDBACK_BINDING:							return "GL_TRANSFORM_FEEDBACK_BINDING";
		case GL_LINE_WIDTH:											return "GL_LINE_WIDTH";
		case GL_CULL_FACE_MODE:										return "GL_CULL_FACE_MODE";
		case GL_FRONT_FACE:											return "GL_FRONT_FACE";
		case GL_POLYGON_OFFSET_FACTOR:								return "GL_POLYGON_OFFSET_FACTOR";
		case GL_POLYGON_OFFSET_UNITS:								return "GL_POLYGON_OFFSET_UNITS";
		case GL_SAMPLE_COVERAGE_VALUE:								return "GL_SAMPLE_COVERAGE_VALUE";
		case GL_SAMPLE_COVERAGE_INVERT:								return "GL_SAMPLE_COVERAGE_INVERT";
		case GL_ACTIVE_TEXTURE:										return "GL_ACTIVE_TEXTURE";
		case GL_TEXTURE_BINDING_1D:									return "GL_TEXTURE_BINDING_1D";
		case GL_TEXTURE_BINDING_1D_ARRAY:							return "GL_TEXTURE_BINDING_1D_ARRAY";
		case GL_TEXTURE_BINDING_2D:									return "GL_TEXTURE_BINDING_2D";
		case GL_TEXTURE_BINDING_RECTANGLE:							return "GL_TEXTURE_BINDING_RECTANGLE";
		case GL_TEXTURE_BINDING_3D:									return "GL_TEXTURE_BINDING_3D";
		case GL_TEXTURE_BINDING_2D_ARRAY:							return "GL_TEXTURE_BINDING_2D_ARRAY";
		case GL_TEXTURE_BINDING_CUBE_MAP:							return "GL_TEXTURE_BINDING_CUBE_MAP";
		case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY:						return "GL_TEXTURE_BINDING_CUBE_MAP_ARRAY";
		case GL_SAMPLER_BINDING:									return "GL_SAMPLER_BINDING";
		case GL_SCISSOR_BOX:										return "GL_SCISSOR_BOX";
		case GL_STENCIL_FUNC:										return "GL_STENCIL_FUNC";
		case GL_STENCIL_VALUE_MASK:									return "GL_STENCIL_VALUE_MASK";
		case GL_STENCIL_REF:										return "GL_STENCIL_REF";
		case GL_STENCIL_FAIL:										return "GL_STENCIL_FAIL";
		case GL_STENCIL_PASS_DEPTH_FAIL:							return "GL_STENCIL_PASS_DEPTH_FAIL";
		case GL_STENCIL_PASS_DEPTH_PASS:							return "GL_STENCIL_PASS_DEPTH_PASS";
		case GL_STENCIL_BACK_FUNC:									return "GL_STENCIL_BACK_FUNC";
		case GL_STENCIL_BACK_VALUE_MASK:							return "GL_STENCIL_BACK_VALUE_MASK";
		case GL_STENCIL_BACK_REF:									return "GL_STENCIL_BACK_REF";
		case GL_STENCIL_BACK_FAIL:									return "GL_STENCIL_BACK_FAIL";
		case GL_STENCIL_BACK_PASS_DEPTH_FAIL:						return "GL_STENCIL_BACK_PASS_DEPTH_FAIL";
		case GL_STENCIL_BACK_PASS_DEPTH_PASS:						return "GL_STENCIL_BACK_PASS_DEPTH_PASS";
		case GL_DEPTH_FUNC:											return "GL_DEPTH_FUNC";
		case GL_BLEND_SRC_RGB:										return "GL_BLEND_SRC_RGB";
		case GL_BLEND_SRC_ALPHA:									return "GL_BLEND_SRC_ALPHA";
		case GL_BLEND_DST_RGB:										return "GL_BLEND_DST_RGB";
		case GL_BLEND_DST_ALPHA:									return "GL_BLEND_DST_ALPHA";
		case GL_BLEND_EQUATION_RGB:									return "GL_BLEND_EQUATION_RGB";
		case GL_BLEND_EQUATION_ALPHA:								return "GL_BLEND_EQUATION_ALPHA";
		case GL_BLEND_COLOR:										return "GL_BLEND_COLOR";
		case GL_COLOR_WRITEMASK:									return "GL_COLOR_WRITEMASK";
		case GL_DEPTH_WRITEMASK:									return "GL_DEPTH_WRITEMASK";
		case GL_STENCIL_WRITEMASK:									return "GL_STENCIL_WRITEMASK";
		case GL_STENCIL_BACK_WRITEMASK:								return "GL_STENCIL_BACK_WRITEMASK";
		case GL_COLOR_CLEAR_VALUE:									return "GL_COLOR_CLEAR_VALUE";
		case GL_DEPTH_CLEAR_VALUE:									return "GL_DEPTH_CLEAR_VALUE";
		case GL_STENCIL_CLEAR_VALUE:								return "GL_STENCIL_CLEAR_VALUE";
		case GL_DRAW_FRAMEBUFFER_BINDING:							return "GL_DRAW_FRAMEBUFFER_BINDING";
		case GL_READ_FRAMEBUFFER_BINDING:							return "GL_READ_FRAMEBUFFER_BINDING";
		case GL_RENDERBUFFER_BINDING:								return "GL_RENDERBUFFER_BINDING";
		case GL_DRAW_BUFFER0:										return "GL_DRAW_BUFFER0";
		case GL_DRAW_BUFFER1:										return "GL_DRAW_BUFFER1";
		case GL_DRAW_BUFFER2:										return "GL_DRAW_BUFFER2";
		case GL_DRAW_BUFFER3:										return "GL_DRAW_BUFFER3";
		case GL_DRAW_BUFFER4:										return "GL_DRAW_BUFFER4";
		case GL_DRAW_BUFFER5:										return "GL_DRAW_BUFFER5";
		case GL_DRAW_BUFFER6:										return "GL_DRAW_BUFFER6";
		case GL_DRAW_BUFFER7:										return "GL_DRAW_BUFFER7";
		case GL_DRAW_BUFFER8:										return "GL_DRAW_BUFFER8";
		case GL_DRAW_BUFFER9:										return "GL_DRAW_BUFFER9";
		case GL_DRAW_BUFFER10:										return "GL_DRAW_BUFFER10";
		case GL_DRAW_BUFFER11:										return "GL_DRAW_BUFFER11";
		case GL_DRAW_BUFFER12:										return "GL_DRAW_BUFFER12";
		case GL_DRAW_BUFFER13:										return "GL_DRAW_BUFFER13";
		case GL_DRAW_BUFFER14:										return "GL_DRAW_BUFFER14";
		case GL_DRAW_BUFFER15:										return "GL_DRAW_BUFFER15";
		case GL_READ_BUFFER:										return "GL_READ_BUFFER";
		case GL_DRAW_BUFFER:										return "GL_DRAW_BUFFER";
		case GL_UNPACK_IMAGE_HEIGHT:								return "GL_UNPACK_IMAGE_HEIGHT";
		case GL_UNPACK_SKIP_IMAGES:									return "GL_UNPACK_SKIP_IMAGES";
		case GL_UNPACK_ROW_LENGTH:									return "GL_UNPACK_ROW_LENGTH";
		case GL_UNPACK_SKIP_ROWS:									return "GL_UNPACK_SKIP_ROWS";
		case GL_UNPACK_SKIP_PIXELS:									return "GL_UNPACK_SKIP_PIXELS";
		case GL_UNPACK_ALIGNMENT:									return "GL_UNPACK_ALIGNMENT";
		case GL_PACK_ROW_LENGTH:									return "GL_PACK_ROW_LENGTH";
		case GL_PACK_SKIP_ROWS:										return "GL_PACK_SKIP_ROWS";
		case GL_PACK_SKIP_PIXELS:									return "GL_PACK_SKIP_PIXELS";
		case GL_PACK_ALIGNMENT:										return "GL_PACK_ALIGNMENT";
		case GL_PIXEL_PACK_BUFFER_BINDING:							return "GL_PIXEL_PACK_BUFFER_BINDING";
		case GL_PIXEL_UNPACK_BUFFER_BINDING:						return "GL_PIXEL_UNPACK_BUFFER_BINDING";
		case GL_CURRENT_PROGRAM:									return "GL_CURRENT_PROGRAM";
		case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:					return "GL_TRANSFORM_FEEDBACK_BUFFER_BINDING";
		case GL_TRANSFORM_FEEDBACK_PAUSED:							return "GL_TRANSFORM_FEEDBACK_PAUSED";
		case GL_TRANSFORM_FEEDBACK_ACTIVE:							return "GL_TRANSFORM_FEEDBACK_ACTIVE";
		case GL_UNIFORM_BUFFER_BINDING:								return "GL_UNIFORM_BUFFER_BINDING";
		case GL_GENERATE_MIPMAP_HINT:								return "GL_GENERATE_MIPMAP_HINT";
		case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:					return "GL_FRAGMENT_SHADER_DERIVATIVE_HINT";
		case GL_MAX_ELEMENT_INDEX:									return "GL_MAX_ELEMENT_INDEX";
		case GL_SUBPIXEL_BITS:										return "GL_SUBPIXEL_BITS";
		case GL_MAX_3D_TEXTURE_SIZE:								return "GL_MAX_3D_TEXTURE_SIZE";
		case GL_MAX_TEXTURE_SIZE:									return "GL_MAX_TEXTURE_SIZE";
		case GL_MAX_ARRAY_TEXTURE_LAYERS:							return "GL_MAX_ARRAY_TEXTURE_LAYERS";
		case GL_MAX_TEXTURE_LOD_BIAS:								return "GL_MAX_TEXTURE_LOD_BIAS";
		case GL_MAX_CUBE_MAP_TEXTURE_SIZE:							return "GL_MAX_CUBE_MAP_TEXTURE_SIZE";
		case GL_MAX_RECTANGLE_TEXTURE_SIZE:							return "GL_MAX_RECTANGLE_TEXTURE_SIZE";
		case GL_MAX_RENDERBUFFER_SIZE:								return "GL_MAX_RENDERBUFFER_SIZE";
		case GL_MAX_DRAW_BUFFERS:									return "GL_MAX_DRAW_BUFFERS";
		case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS:						return "GL_MAX_DUAL_SOURCE_DRAW_BUFFERS";
		case GL_MAX_COLOR_ATTACHMENTS:								return "GL_MAX_COLOR_ATTACHMENTS";
		case GL_MAX_VIEWPORT_DIMS:									return "GL_MAX_VIEWPORT_DIMS";
		case GL_ALIASED_POINT_SIZE_RANGE:							return "GL_ALIASED_POINT_SIZE_RANGE";
		case GL_ALIASED_LINE_WIDTH_RANGE:							return "GL_ALIASED_LINE_WIDTH_RANGE";
		case GL_MAX_ELEMENTS_INDICES:								return "GL_MAX_ELEMENTS_INDICES";
		case GL_MAX_ELEMENTS_VERTICES:								return "GL_MAX_ELEMENTS_VERTICES";
		case GL_COMPRESSED_TEXTURE_FORMATS:							return "GL_COMPRESSED_TEXTURE_FORMATS";
		case GL_NUM_COMPRESSED_TEXTURE_FORMATS:						return "GL_NUM_COMPRESSED_TEXTURE_FORMATS";
		case GL_PROGRAM_BINARY_FORMATS:								return "GL_PROGRAM_BINARY_FORMATS";
		case GL_NUM_PROGRAM_BINARY_FORMATS:							return "GL_NUM_PROGRAM_BINARY_FORMATS";
		case GL_SHADER_BINARY_FORMATS:								return "GL_SHADER_BINARY_FORMATS";
		case GL_NUM_SHADER_BINARY_FORMATS:							return "GL_NUM_SHADER_BINARY_FORMATS";
		case GL_SHADER_COMPILER:									return "GL_SHADER_COMPILER";
		case GL_MAX_SERVER_WAIT_TIMEOUT:							return "GL_MAX_SERVER_WAIT_TIMEOUT";
		case GL_NUM_EXTENSIONS:										return "GL_NUM_EXTENSIONS";
		case GL_MAJOR_VERSION:										return "GL_MAJOR_VERSION";
		case GL_MINOR_VERSION:										return "GL_MINOR_VERSION";
		case GL_MAX_VERTEX_ATTRIBS:									return "GL_MAX_VERTEX_ATTRIBS";
		case GL_MAX_VERTEX_UNIFORM_COMPONENTS:						return "GL_MAX_VERTEX_UNIFORM_COMPONENTS";
		case GL_MAX_VERTEX_UNIFORM_VECTORS:							return "GL_MAX_VERTEX_UNIFORM_VECTORS";
		case GL_MAX_VERTEX_UNIFORM_BLOCKS:							return "GL_MAX_VERTEX_UNIFORM_BLOCKS";
		case GL_MAX_VERTEX_OUTPUT_COMPONENTS:						return "GL_MAX_VERTEX_OUTPUT_COMPONENTS";
		case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:						return "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS";
		case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:					return "GL_MAX_FRAGMENT_UNIFORM_COMPONENTS";
		case GL_MAX_FRAGMENT_UNIFORM_VECTORS:						return "GL_MAX_FRAGMENT_UNIFORM_VECTORS";
		case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:						return "GL_MAX_FRAGMENT_UNIFORM_BLOCKS";
		case GL_MAX_FRAGMENT_INPUT_COMPONENTS:						return "GL_MAX_FRAGMENT_INPUT_COMPONENTS";
		case GL_MAX_TEXTURE_IMAGE_UNITS:							return "GL_MAX_TEXTURE_IMAGE_UNITS";
		case GL_MIN_PROGRAM_TEXEL_OFFSET:							return "GL_MIN_PROGRAM_TEXEL_OFFSET";
		case GL_MAX_PROGRAM_TEXEL_OFFSET:							return "GL_MAX_PROGRAM_TEXEL_OFFSET";
		case GL_MAX_UNIFORM_BUFFER_BINDINGS:						return "GL_MAX_UNIFORM_BUFFER_BINDINGS";
		case GL_MAX_UNIFORM_BLOCK_SIZE:								return "GL_MAX_UNIFORM_BLOCK_SIZE";
		case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:					return "GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT";
		case GL_MAX_COMBINED_UNIFORM_BLOCKS:						return "GL_MAX_COMBINED_UNIFORM_BLOCKS";
		case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:				return "GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS";
		case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:			return "GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS";
		case GL_MAX_VARYING_COMPONENTS:								return "GL_MAX_VARYING_COMPONENTS";
		case GL_MAX_VARYING_VECTORS:								return "GL_MAX_VARYING_VECTORS";
		case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:					return "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS";
		case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:		return "GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS";
		case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:			return "GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS";
		case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:			return "GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS";
		case GL_INTERLEAVED_ATTRIBS:								return "GL_INTERLEAVED_ATTRIBS";
		case GL_SEPARATE_ATTRIBS:									return "GL_SEPARATE_ATTRIBS";
		case GL_SAMPLE_BUFFERS:										return "GL_SAMPLE_BUFFERS";
		case GL_SAMPLES:											return "GL_SAMPLES";
		case GL_MAX_SAMPLES:										return "GL_MAX_SAMPLES";
		case GL_DEPTH_BITS:											return "GL_DEPTH_BITS";
		case GL_STENCIL_BITS:										return "GL_STENCIL_BITS";
		case GL_IMPLEMENTATION_COLOR_READ_TYPE:						return "GL_IMPLEMENTATION_COLOR_READ_TYPE";
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT:					return "GL_IMPLEMENTATION_COLOR_READ_FORMAT";
		case GL_COPY_READ_BUFFER_BINDING:							return "GL_COPY_READ_BUFFER_BINDING";
		case GL_COPY_WRITE_BUFFER_BINDING:							return "GL_COPY_WRITE_BUFFER_BINDING";
		case GL_RED_BITS:											return "GL_RED_BITS";
		case GL_GREEN_BITS:											return "GL_GREEN_BITS";
		case GL_BLUE_BITS:											return "GL_BLUE_BITS";
		case GL_ALPHA_BITS:											return "GL_ALPHA_BITS";
		case GL_MAX_COLOR_TEXTURE_SAMPLES:							return "GL_MAX_COLOR_TEXTURE_SAMPLES";
		case GL_TIMESTAMP:											return "GL_TIMESTAMP";
		case GL_MAX_DEPTH_TEXTURE_SAMPLES:							return "GL_MAX_DEPTH_TEXTURE_SAMPLES";
		case GL_MAX_INTEGER_SAMPLES:								return "GL_MAX_INTEGER_SAMPLES";
		case GL_TEXTURE_BINDING_2D_MULTISAMPLE:						return "GL_TEXTURE_BINDING_2D_MULTISAMPLE";
		case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY:				return "GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY";
		case GL_MAX_VERTEX_ATTRIB_STRIDE:							return "GL_MAX_VERTEX_ATTRIB_STRIDE";
		case GL_MAX_VERTEX_ATTRIB_BINDINGS:							return "GL_MAX_VERTEX_ATTRIB_BINDINGS";
		case GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET:					return "GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET";
		case GL_MIN_SAMPLE_SHADING_VALUE:							return "GL_MIN_SAMPLE_SHADING_VALUE";
		case GL_FRAGMENT_INTERPOLATION_OFFSET_BITS:					return "GL_FRAGMENT_INTERPOLATION_OFFSET_BITS";
		case GL_MAX_FRAGMENT_INTERPOLATION_OFFSET:					return "GL_MAX_FRAGMENT_INTERPOLATION_OFFSET";
		case GL_MIN_FRAGMENT_INTERPOLATION_OFFSET:					return "GL_MIN_FRAGMENT_INTERPOLATION_OFFSET";
		case GL_MAX_GEOMETRY_SHADER_INVOCATIONS:					return "GL_MAX_GEOMETRY_SHADER_INVOCATIONS";
		case GL_LAYER_PROVOKING_VERTEX:								return "GL_LAYER_PROVOKING_VERTEX";
		case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS:					return "GL_MAX_GEOMETRY_UNIFORM_COMPONENTS";
		case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS:					return "GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS";
		case GL_MAX_GEOMETRY_UNIFORM_BLOCKS:						return "GL_MAX_GEOMETRY_UNIFORM_BLOCKS";
		case GL_MAX_GEOMETRY_INPUT_COMPONENTS:						return "GL_MAX_GEOMETRY_INPUT_COMPONENTS";
		case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS:						return "GL_MAX_GEOMETRY_OUTPUT_COMPONENTS";
		case GL_MAX_GEOMETRY_IMAGE_UNIFORMS:						return "GL_MAX_GEOMETRY_IMAGE_UNIFORMS";
		case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS:					return "GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS";
		case GL_MAX_GEOMETRY_OUTPUT_VERTICES:						return "GL_MAX_GEOMETRY_OUTPUT_VERTICES";
		case GL_MAX_GEOMETRY_ATOMIC_COUNTERS:						return "GL_MAX_GEOMETRY_ATOMIC_COUNTERS";
		case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS:				return "GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS:				return "GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS";
		case GL_MAX_FRAMEBUFFER_LAYERS:								return "GL_MAX_FRAMEBUFFER_LAYERS";
		case GL_PROVOKING_VERTEX:									return "GL_PROVOKING_VERTEX";
		case GL_PRIMITIVE_RESTART_INDEX:							return "GL_PRIMITIVE_RESTART_INDEX";
		case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:				return "GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:						return "GL_MAX_FRAGMENT_ATOMIC_COUNTERS";
		case GL_MAX_FRAGMENT_IMAGE_UNIFORMS:						return "GL_MAX_FRAGMENT_IMAGE_UNIFORMS";
		case GL_MAX_COMPUTE_UNIFORM_BLOCKS:							return "GL_MAX_COMPUTE_UNIFORM_BLOCKS";
		case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:					return "GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS";
		case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:						return "GL_MAX_COMPUTE_UNIFORM_COMPONENTS";
		case GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS:					return "GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_COMPUTE_ATOMIC_COUNTERS:						return "GL_MAX_COMPUTE_ATOMIC_COUNTERS";
		case GL_MAX_COMPUTE_IMAGE_UNIFORMS:							return "GL_MAX_COMPUTE_IMAGE_UNIFORMS";
		case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:					return "GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS";
		case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:					return "GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS";
		case GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS:			return "GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS";
		case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:					return "GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_VERTEX_ATOMIC_COUNTERS:							return "GL_MAX_VERTEX_ATOMIC_COUNTERS";
		case GL_MAX_VERTEX_IMAGE_UNIFORMS:							return "GL_MAX_VERTEX_IMAGE_UNIFORMS";
		case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:					return "GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS";
		case GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES:				return "GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES";
		case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:					return "GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS";
		case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:						return "GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE";
		case GL_MAX_IMAGE_UNITS:									return "GL_MAX_IMAGE_UNITS";
		case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:				return "GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_COMBINED_IMAGE_UNIFORMS:						return "GL_MAX_COMBINED_IMAGE_UNIFORMS";
		case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:					return "GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS";
		case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:						return "GL_MAX_SHADER_STORAGE_BLOCK_SIZE";
		case GL_MAX_COMBINED_ATOMIC_COUNTERS:						return "GL_MAX_COMBINED_ATOMIC_COUNTERS";
		case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:					return "GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS";
		case GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT:				return "GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT";
		case GL_PRIMITIVE_BOUNDING_BOX_EXT:							return "GL_PRIMITIVE_BOUNDING_BOX_EXT";
		case GL_DRAW_INDIRECT_BUFFER_BINDING:						return "GL_DRAW_INDIRECT_BUFFER_BINDING";
		case GL_MAX_SAMPLE_MASK_WORDS:								return "GL_MAX_SAMPLE_MASK_WORDS";
		case GL_PROGRAM_PIPELINE_BINDING:							return "GL_PROGRAM_PIPELINE_BINDING";
		case GL_ATOMIC_COUNTER_BUFFER_BINDING:						return "GL_ATOMIC_COUNTER_BUFFER_BINDING";
		case GL_SHADER_STORAGE_BUFFER_BINDING:						return "GL_SHADER_STORAGE_BUFFER_BINDING";
		case GL_DISPATCH_INDIRECT_BUFFER_BINDING:					return "GL_DISPATCH_INDIRECT_BUFFER_BINDING";
		case GL_MAX_FRAMEBUFFER_WIDTH:								return "GL_MAX_FRAMEBUFFER_WIDTH";
		case GL_MAX_FRAMEBUFFER_HEIGHT:								return "GL_MAX_FRAMEBUFFER_HEIGHT";
		case GL_MAX_FRAMEBUFFER_SAMPLES:							return "GL_MAX_FRAMEBUFFER_SAMPLES";
		case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:						return "GL_MAX_COMPUTE_SHARED_MEMORY_SIZE";
		case GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET:					return "GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET";
		case GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET:					return "GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET";
		case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:					return "GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS";
		case GL_MAX_UNIFORM_LOCATIONS:								return "GL_MAX_UNIFORM_LOCATIONS";
		case GL_MAX_DEBUG_MESSAGE_LENGTH:							return "GL_MAX_DEBUG_MESSAGE_LENGTH";
		case GL_MAX_DEBUG_LOGGED_MESSAGES:							return "GL_MAX_DEBUG_LOGGED_MESSAGES";
		case GL_MAX_DEBUG_GROUP_STACK_DEPTH:						return "GL_MAX_DEBUG_GROUP_STACK_DEPTH";
		case GL_MAX_LABEL_LENGTH:									return "GL_MAX_LABEL_LENGTH";
		case GL_CONTEXT_FLAGS:										return "GL_CONTEXT_FLAGS";
		case GL_DEBUG_LOGGED_MESSAGES:								return "GL_DEBUG_LOGGED_MESSAGES";
		case GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:					return "GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH";
		case GL_DEBUG_GROUP_STACK_DEPTH:							return "GL_DEBUG_GROUP_STACK_DEPTH";
		case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS:			return "GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS";
		case GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT:					return "GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT";
		case GL_TEXTURE_BUFFER_BINDING:								return "GL_TEXTURE_BUFFER_BINDING";
		case GL_TEXTURE_BINDING_BUFFER:								return "GL_TEXTURE_BINDING_BUFFER";
		case GL_MAX_TEXTURE_BUFFER_SIZE:							return "GL_MAX_TEXTURE_BUFFER_SIZE";
		case GL_MAX_PATCH_VERTICES:									return "GL_MAX_PATCH_VERTICES";
		case GL_MAX_TESS_GEN_LEVEL:									return "GL_MAX_TESS_GEN_LEVEL";
		case GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS:				return "GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS";
		case GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS:				return "GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS";
		case GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS:				return "GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS";
		case GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS:			return "GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS";
		case GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS:					return "GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS";
		case GL_MAX_TESS_PATCH_COMPONENTS:							return "GL_MAX_TESS_PATCH_COMPONENTS";
		case GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS:			return "GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS";
		case GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS:				return "GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS";
		case GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS:					return "GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS";
		case GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS:					return "GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS";
		case GL_MAX_TESS_CONTROL_INPUT_COMPONENTS:					return "GL_MAX_TESS_CONTROL_INPUT_COMPONENTS";
		case GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS:				return "GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS";
		case GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS:			return "GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS:			return "GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS";
		case GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS:					return "GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS";
		case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS:				return "GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS";
		case GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS:					return "GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS";
		case GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS:					return "GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS";
		case GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS:	return "GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS";
		case GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS:			return "GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS";
		case GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS:		return "GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS";
		case GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS:				return "GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS";
		case GL_PATCH_VERTICES:										return "GL_PATCH_VERTICES";
		case GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED:			return "GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED";
		case GL_BUFFER_MAP_POINTER:									return "GL_BUFFER_MAP_POINTER";
		case GL_POINT_SIZE:											return "GL_POINT_SIZE";
		case GL_POINT_SIZE_RANGE:									return "GL_POINT_SIZE_RANGE";
		case GL_POINT_SIZE_GRANULARITY:								return "GL_POINT_SIZE_GRANULARITY";
		case GL_BLEND_DST:											return "GL_BLEND_DST";
		case GL_BLEND_SRC:											return "GL_BLEND_SRC";
		case GL_LOGIC_OP_MODE:										return "GL_LOGIC_OP_MODE";
		case GL_COLOR_LOGIC_OP:										return "GL_COLOR_LOGIC_OP";
		case GL_TEXTURE_LOD_BIAS:									return "GL_TEXTURE_LOD_BIAS";
		case GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION:			return "GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION";
		case GL_MAX_SUBROUTINES:									return "GL_MAX_SUBROUTINES";
		case GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS:					return "GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS";
		case GL_NUM_COMPATIBLE_SUBROUTINES:							return "GL_NUM_COMPATIBLE_SUBROUTINES";
		case GL_COMPATIBLE_SUBROUTINES:								return "GL_COMPATIBLE_SUBROUTINES";
		case GL_PATCH_DEFAULT_INNER_LEVEL:							return "GL_PATCH_DEFAULT_INNER_LEVEL";
		case GL_PATCH_DEFAULT_OUTER_LEVEL:							return "GL_PATCH_DEFAULT_OUTER_LEVEL";
		case GL_MAX_TRANSFORM_FEEDBACK_BUFFERS:						return "GL_MAX_TRANSFORM_FEEDBACK_BUFFERS";
		case GL_TEXTURE_2D:											return "GL_TEXTURE_2D";
		case GL_CULL_FACE:											return "GL_CULL_FACE";
		case GL_BLEND:												return "GL_BLEND";
		case GL_DITHER:												return "GL_DITHER";
		case GL_STENCIL_TEST:										return "GL_STENCIL_TEST";
		case GL_DEPTH_TEST:											return "GL_DEPTH_TEST";
		case GL_SCISSOR_TEST:										return "GL_SCISSOR_TEST";
		case GL_POLYGON_OFFSET_FILL:								return "GL_POLYGON_OFFSET_FILL";
		case GL_SAMPLE_ALPHA_TO_COVERAGE:							return "GL_SAMPLE_ALPHA_TO_COVERAGE";
		case GL_SAMPLE_COVERAGE:									return "GL_SAMPLE_COVERAGE";
		case GL_STEREO:												return "GL_STEREO";
		case GL_PRIMITIVE_RESTART_FIXED_INDEX:						return "GL_PRIMITIVE_RESTART_FIXED_INDEX";
		case GL_RASTERIZER_DISCARD:									return "GL_RASTERIZER_DISCARD";
		case GL_FRAMEBUFFER_SRGB:									return "GL_FRAMEBUFFER_SRGB";
		case GL_SAMPLE_SHADING:										return "GL_SAMPLE_SHADING";
		case GL_DEPTH_CLAMP:										return "GL_DEPTH_CLAMP";
		case GL_PRIMITIVE_RESTART:									return "GL_PRIMITIVE_RESTART";
		case GL_SAMPLE_MASK:										return "GL_SAMPLE_MASK";
		case GL_DEBUG_OUTPUT:										return "GL_DEBUG_OUTPUT";
		case GL_DEBUG_OUTPUT_SYNCHRONOUS:							return "GL_DEBUG_OUTPUT_SYNCHRONOUS";
		case GL_BLEND_ADVANCED_COHERENT_KHR:						return "GL_BLEND_ADVANCED_COHERENT_KHR";
		case GL_MULTISAMPLE:										return "GL_MULTISAMPLE";
		case GL_SAMPLE_ALPHA_TO_ONE:								return "GL_SAMPLE_ALPHA_TO_ONE";
		case GL_VERTEX_PROGRAM_POINT_SIZE:							return "GL_VERTEX_PROGRAM_POINT_SIZE";
		case GL_TEXTURE_CUBE_MAP_SEAMLESS:							return "GL_TEXTURE_CUBE_MAP_SEAMLESS";
		default:													return nullptr;
	}
}

const char* getGettableIndexedStateName (int value)
{
	switch (value)
	{
		case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:					return "GL_TRANSFORM_FEEDBACK_BUFFER_BINDING";
		case GL_UNIFORM_BUFFER_BINDING:								return "GL_UNIFORM_BUFFER_BINDING";
		case GL_TRANSFORM_FEEDBACK_BUFFER_START:					return "GL_TRANSFORM_FEEDBACK_BUFFER_START";
		case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:						return "GL_TRANSFORM_FEEDBACK_BUFFER_SIZE";
		case GL_UNIFORM_BUFFER_START:								return "GL_UNIFORM_BUFFER_START";
		case GL_UNIFORM_BUFFER_SIZE:								return "GL_UNIFORM_BUFFER_SIZE";
		case GL_SAMPLE_MASK_VALUE:									return "GL_SAMPLE_MASK_VALUE";
		case GL_VERTEX_BINDING_OFFSET:								return "GL_VERTEX_BINDING_OFFSET";
		case GL_VERTEX_BINDING_STRIDE:								return "GL_VERTEX_BINDING_STRIDE";
		case GL_VERTEX_BINDING_DIVISOR:								return "GL_VERTEX_BINDING_DIVISOR";
		case GL_VERTEX_BINDING_BUFFER:								return "GL_VERTEX_BINDING_BUFFER";
		case GL_MAX_COMPUTE_WORK_GROUP_COUNT:						return "GL_MAX_COMPUTE_WORK_GROUP_COUNT";
		case GL_MAX_COMPUTE_WORK_GROUP_SIZE:						return "GL_MAX_COMPUTE_WORK_GROUP_SIZE";
		case GL_ATOMIC_COUNTER_BUFFER_BINDING:						return "GL_ATOMIC_COUNTER_BUFFER_BINDING";
		case GL_ATOMIC_COUNTER_BUFFER_START:						return "GL_ATOMIC_COUNTER_BUFFER_START";
		case GL_ATOMIC_COUNTER_BUFFER_SIZE:							return "GL_ATOMIC_COUNTER_BUFFER_SIZE";
		case GL_SHADER_STORAGE_BUFFER_BINDING:						return "GL_SHADER_STORAGE_BUFFER_BINDING";
		case GL_SHADER_STORAGE_BUFFER_START:						return "GL_SHADER_STORAGE_BUFFER_START";
		case GL_SHADER_STORAGE_BUFFER_SIZE:							return "GL_SHADER_STORAGE_BUFFER_SIZE";
		case GL_IMAGE_BINDING_NAME:									return "GL_IMAGE_BINDING_NAME";
		case GL_IMAGE_BINDING_LEVEL:								return "GL_IMAGE_BINDING_LEVEL";
		case GL_IMAGE_BINDING_LAYERED:								return "GL_IMAGE_BINDING_LAYERED";
		case GL_IMAGE_BINDING_LAYER:								return "GL_IMAGE_BINDING_LAYER";
		case GL_IMAGE_BINDING_ACCESS:								return "GL_IMAGE_BINDING_ACCESS";
		case GL_IMAGE_BINDING_FORMAT:								return "GL_IMAGE_BINDING_FORMAT";
		case GL_BLEND_EQUATION_RGB:									return "GL_BLEND_EQUATION_RGB";
		case GL_BLEND_EQUATION_ALPHA:								return "GL_BLEND_EQUATION_ALPHA";
		case GL_BLEND_SRC_RGB:										return "GL_BLEND_SRC_RGB";
		case GL_BLEND_SRC_ALPHA:									return "GL_BLEND_SRC_ALPHA";
		case GL_BLEND_DST_RGB:										return "GL_BLEND_DST_RGB";
		case GL_BLEND_DST_ALPHA:									return "GL_BLEND_DST_ALPHA";
		case GL_COLOR_WRITEMASK:									return "GL_COLOR_WRITEMASK";
		case GL_UNIFORM_BLOCK_BINDING:								return "GL_UNIFORM_BLOCK_BINDING";
		case GL_UNIFORM_BLOCK_DATA_SIZE:							return "GL_UNIFORM_BLOCK_DATA_SIZE";
		case GL_UNIFORM_BLOCK_NAME_LENGTH:							return "GL_UNIFORM_BLOCK_NAME_LENGTH";
		case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:						return "GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS";
		case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:				return "GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES";
		case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:			return "GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER";
		case GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER:	return "GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER";
		case GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER:	return "GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER";
		case GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER:		return "GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER";
		case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:		return "GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER";
		case GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER:			return "GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER";
		case GL_OBJECT_TYPE:										return "GL_OBJECT_TYPE";
		case GL_SYNC_STATUS:										return "GL_SYNC_STATUS";
		case GL_SYNC_CONDITION:										return "GL_SYNC_CONDITION";
		case GL_SYNC_FLAGS:											return "GL_SYNC_FLAGS";
		case GL_SYNC_FENCE:											return "GL_SYNC_FENCE";
		case GL_SYNC_GPU_COMMANDS_COMPLETE:							return "GL_SYNC_GPU_COMMANDS_COMPLETE";
		case GL_SIGNALED:											return "GL_SIGNALED";
		case GL_UNSIGNALED:											return "GL_UNSIGNALED";
		case GL_PROGRAM_BINARY_LENGTH:								return "GL_PROGRAM_BINARY_LENGTH";
		case GL_CONTEXT_PROFILE_MASK:								return "GL_CONTEXT_PROFILE_MASK";
		case GL_FRACTIONAL_EVEN:									return "GL_FRACTIONAL_EVEN";
		case GL_FRACTIONAL_ODD:										return "GL_FRACTIONAL_ODD";
		default:													return nullptr;
	}
}

const char* getGettableStringName (int value)
{
	switch (value)
	{
		case GL_EXTENSIONS:					return "GL_EXTENSIONS";
		case GL_RENDERER:					return "GL_RENDERER";
		case GL_SHADING_LANGUAGE_VERSION:	return "GL_SHADING_LANGUAGE_VERSION";
		case GL_VENDOR:						return "GL_VENDOR";
		case GL_VERSION:					return "GL_VERSION";
		default:							return nullptr;
	}
}

const char* getGettablePackStateName (int value)
{
	switch (value)
	{
		case GL_PACK_SWAP_BYTES:		return "GL_PACK_SWAP_BYTES";
		case GL_PACK_LSB_FIRST:			return "GL_PACK_LSB_FIRST";
		case GL_PACK_ROW_LENGTH:		return "GL_PACK_ROW_LENGTH";
		case GL_PACK_IMAGE_HEIGHT:		return "GL_PACK_IMAGE_HEIGHT";
		case GL_PACK_SKIP_ROWS:			return "GL_PACK_SKIP_ROWS";
		case GL_PACK_SKIP_PIXELS:		return "GL_PACK_SKIP_PIXELS";
		case GL_PACK_SKIP_IMAGES:		return "GL_PACK_SKIP_IMAGES";
		case GL_PACK_ALIGNMENT:			return "GL_PACK_ALIGNMENT";
		case GL_UNPACK_SWAP_BYTES:		return "GL_UNPACK_SWAP_BYTES";
		case GL_UNPACK_LSB_FIRST:		return "GL_UNPACK_LSB_FIRST";
		case GL_UNPACK_ROW_LENGTH:		return "GL_UNPACK_ROW_LENGTH";
		case GL_UNPACK_IMAGE_HEIGHT:	return "GL_UNPACK_IMAGE_HEIGHT";
		case GL_UNPACK_SKIP_ROWS:		return "GL_UNPACK_SKIP_ROWS";
		case GL_UNPACK_SKIP_PIXELS:		return "GL_UNPACK_SKIP_PIXELS";
		case GL_UNPACK_SKIP_IMAGES:		return "GL_UNPACK_SKIP_IMAGES";
		case GL_UNPACK_ALIGNMENT:		return "GL_UNPACK_ALIGNMENT";
		default:						return nullptr;
	}
}

const char* getPointerStateName (int value)
{
	switch (value)
	{
		case GL_DEBUG_CALLBACK_FUNCTION:	return "GL_DEBUG_CALLBACK_FUNCTION";
		case GL_DEBUG_CALLBACK_USER_PARAM:	return "GL_DEBUG_CALLBACK_USER_PARAM";
		default:							return nullptr;
	}
}

const char* getInternalFormatParameterName (int value)
{
	switch (value)
	{
		case GL_NUM_SAMPLE_COUNTS:	return "GL_NUM_SAMPLE_COUNTS";
		case GL_SAMPLES:			return "GL_SAMPLES";
		default:					return nullptr;
	}
}

const char* getInternalFormatTargetName (int value)
{
	switch (value)
	{
		case GL_RENDERBUFFER:					return "GL_RENDERBUFFER";
		case GL_TEXTURE_2D_MULTISAMPLE:			return "GL_TEXTURE_2D_MULTISAMPLE";
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:	return "GL_TEXTURE_2D_MULTISAMPLE_ARRAY";
		case GL_TEXTURE_2D:						return "GL_TEXTURE_2D";
		case GL_TEXTURE_3D:						return "GL_TEXTURE_3D";
		case GL_TEXTURE_2D_ARRAY:				return "GL_TEXTURE_2D_ARRAY";
		case GL_TEXTURE_CUBE_MAP:				return "GL_TEXTURE_CUBE_MAP";
		default:								return nullptr;
	}
}

const char* getMultisampleParameterName (int value)
{
	switch (value)
	{
		case GL_SAMPLE_POSITION:	return "GL_SAMPLE_POSITION";
		default:					return nullptr;
	}
}

const char* getQueryTargetName (int value)
{
	switch (value)
	{
		case GL_SAMPLES_PASSED:							return "GL_SAMPLES_PASSED";
		case GL_ANY_SAMPLES_PASSED:						return "GL_ANY_SAMPLES_PASSED";
		case GL_PRIMITIVES_GENERATED:					return "GL_PRIMITIVES_GENERATED";
		case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:	return "GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN";
		case GL_TIME_ELAPSED:							return "GL_TIME_ELAPSED";
		case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:		return "GL_ANY_SAMPLES_PASSED_CONSERVATIVE";
		default:										return nullptr;
	}
}

const char* getQueryParamName (int value)
{
	switch (value)
	{
		case GL_CURRENT_QUERY:		return "GL_CURRENT_QUERY";
		case GL_QUERY_COUNTER_BITS:	return "GL_QUERY_COUNTER_BITS";
		default:					return nullptr;
	}
}

const char* getQueryObjectParamName (int value)
{
	switch (value)
	{
		case GL_QUERY_RESULT:			return "GL_QUERY_RESULT";
		case GL_QUERY_RESULT_AVAILABLE:	return "GL_QUERY_RESULT_AVAILABLE";
		case GL_MAX_VERTEX_STREAMS:		return "GL_MAX_VERTEX_STREAMS";
		default:						return nullptr;
	}
}

const char* getImageAccessName (int value)
{
	switch (value)
	{
		case GL_READ_ONLY:	return "GL_READ_ONLY";
		case GL_WRITE_ONLY:	return "GL_WRITE_ONLY";
		case GL_READ_WRITE:	return "GL_READ_WRITE";
		default:			return nullptr;
	}
}

const char* getProgramInterfaceName (int value)
{
	switch (value)
	{
		case GL_UNIFORM:					return "GL_UNIFORM";
		case GL_UNIFORM_BLOCK:				return "GL_UNIFORM_BLOCK";
		case GL_ATOMIC_COUNTER_BUFFER:		return "GL_ATOMIC_COUNTER_BUFFER";
		case GL_PROGRAM_INPUT:				return "GL_PROGRAM_INPUT";
		case GL_PROGRAM_OUTPUT:				return "GL_PROGRAM_OUTPUT";
		case GL_TRANSFORM_FEEDBACK_VARYING:	return "GL_TRANSFORM_FEEDBACK_VARYING";
		case GL_BUFFER_VARIABLE:			return "GL_BUFFER_VARIABLE";
		case GL_SHADER_STORAGE_BLOCK:		return "GL_SHADER_STORAGE_BLOCK";
		default:							return nullptr;
	}
}

const char* getProgramResourcePropertyName (int value)
{
	switch (value)
	{
		case GL_ACTIVE_VARIABLES:						return "GL_ACTIVE_VARIABLES";
		case GL_BUFFER_BINDING:							return "GL_BUFFER_BINDING";
		case GL_NUM_ACTIVE_VARIABLES:					return "GL_NUM_ACTIVE_VARIABLES";
		case GL_ARRAY_SIZE:								return "GL_ARRAY_SIZE";
		case GL_ARRAY_STRIDE:							return "GL_ARRAY_STRIDE";
		case GL_BLOCK_INDEX:							return "GL_BLOCK_INDEX";
		case GL_IS_ROW_MAJOR:							return "GL_IS_ROW_MAJOR";
		case GL_MATRIX_STRIDE:							return "GL_MATRIX_STRIDE";
		case GL_ATOMIC_COUNTER_BUFFER_INDEX:			return "GL_ATOMIC_COUNTER_BUFFER_INDEX";
		case GL_BUFFER_DATA_SIZE:						return "GL_BUFFER_DATA_SIZE";
		case GL_LOCATION:								return "GL_LOCATION";
		case GL_NAME_LENGTH:							return "GL_NAME_LENGTH";
		case GL_OFFSET:									return "GL_OFFSET";
		case GL_REFERENCED_BY_VERTEX_SHADER:			return "GL_REFERENCED_BY_VERTEX_SHADER";
		case GL_REFERENCED_BY_FRAGMENT_SHADER:			return "GL_REFERENCED_BY_FRAGMENT_SHADER";
		case GL_REFERENCED_BY_COMPUTE_SHADER:			return "GL_REFERENCED_BY_COMPUTE_SHADER";
		case GL_REFERENCED_BY_GEOMETRY_SHADER:			return "GL_REFERENCED_BY_GEOMETRY_SHADER";
		case GL_REFERENCED_BY_TESS_CONTROL_SHADER:		return "GL_REFERENCED_BY_TESS_CONTROL_SHADER";
		case GL_REFERENCED_BY_TESS_EVALUATION_SHADER:	return "GL_REFERENCED_BY_TESS_EVALUATION_SHADER";
		case GL_TOP_LEVEL_ARRAY_SIZE:					return "GL_TOP_LEVEL_ARRAY_SIZE";
		case GL_TOP_LEVEL_ARRAY_STRIDE:					return "GL_TOP_LEVEL_ARRAY_STRIDE";
		case GL_TYPE:									return "GL_TYPE";
		case GL_IS_PER_PATCH:							return "GL_IS_PER_PATCH";
		default:										return nullptr;
	}
}

const char* getPrecisionFormatTypeName (int value)
{
	switch (value)
	{
		case GL_LOW_FLOAT:		return "GL_LOW_FLOAT";
		case GL_MEDIUM_FLOAT:	return "GL_MEDIUM_FLOAT";
		case GL_HIGH_FLOAT:		return "GL_HIGH_FLOAT";
		case GL_LOW_INT:		return "GL_LOW_INT";
		case GL_MEDIUM_INT:		return "GL_MEDIUM_INT";
		case GL_HIGH_INT:		return "GL_HIGH_INT";
		default:				return nullptr;
	}
}

const char* getTransformFeedbackTargetName (int value)
{
	switch (value)
	{
		case GL_TRANSFORM_FEEDBACK:	return "GL_TRANSFORM_FEEDBACK";
		default:					return nullptr;
	}
}

const char* getClampColorTargetName (int value)
{
	switch (value)
	{
		case GL_CLAMP_READ_COLOR:	return "GL_CLAMP_READ_COLOR";
		case GL_FIXED_ONLY:			return "GL_FIXED_ONLY";
		default:					return nullptr;
	}
}

const char* getProvokingVertexName (int value)
{
	switch (value)
	{
		case GL_FIRST_VERTEX_CONVENTION:	return "GL_FIRST_VERTEX_CONVENTION";
		case GL_LAST_VERTEX_CONVENTION:		return "GL_LAST_VERTEX_CONVENTION";
		case GL_UNDEFINED_VERTEX:			return "GL_UNDEFINED_VERTEX";
		default:							return nullptr;
	}
}

const char* getDebugMessageSourceName (int value)
{
	switch (value)
	{
		case GL_DEBUG_SOURCE_API:				return "GL_DEBUG_SOURCE_API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:		return "GL_DEBUG_SOURCE_WINDOW_SYSTEM";
		case GL_DEBUG_SOURCE_SHADER_COMPILER:	return "GL_DEBUG_SOURCE_SHADER_COMPILER";
		case GL_DEBUG_SOURCE_THIRD_PARTY:		return "GL_DEBUG_SOURCE_THIRD_PARTY";
		case GL_DEBUG_SOURCE_APPLICATION:		return "GL_DEBUG_SOURCE_APPLICATION";
		case GL_DEBUG_SOURCE_OTHER:				return "GL_DEBUG_SOURCE_OTHER";
		case GL_DONT_CARE:						return "GL_DONT_CARE";
		default:								return nullptr;
	}
}

const char* getDebugMessageTypeName (int value)
{
	switch (value)
	{
		case GL_DEBUG_TYPE_ERROR:				return "GL_DEBUG_TYPE_ERROR";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:	return "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:	return "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR";
		case GL_DEBUG_TYPE_PORTABILITY:			return "GL_DEBUG_TYPE_PORTABILITY";
		case GL_DEBUG_TYPE_PERFORMANCE:			return "GL_DEBUG_TYPE_PERFORMANCE";
		case GL_DEBUG_TYPE_OTHER:				return "GL_DEBUG_TYPE_OTHER";
		case GL_DEBUG_TYPE_MARKER:				return "GL_DEBUG_TYPE_MARKER";
		case GL_DEBUG_TYPE_PUSH_GROUP:			return "GL_DEBUG_TYPE_PUSH_GROUP";
		case GL_DEBUG_TYPE_POP_GROUP:			return "GL_DEBUG_TYPE_POP_GROUP";
		case GL_DONT_CARE:						return "GL_DONT_CARE";
		default:								return nullptr;
	}
}

const char* getDebugMessageSeverityName (int value)
{
	switch (value)
	{
		case GL_DEBUG_SEVERITY_HIGH:			return "GL_DEBUG_SEVERITY_HIGH";
		case GL_DEBUG_SEVERITY_MEDIUM:			return "GL_DEBUG_SEVERITY_MEDIUM";
		case GL_DEBUG_SEVERITY_LOW:				return "GL_DEBUG_SEVERITY_LOW";
		case GL_DEBUG_SEVERITY_NOTIFICATION:	return "GL_DEBUG_SEVERITY_NOTIFICATION";
		case GL_DONT_CARE:						return "GL_DONT_CARE";
		default:								return nullptr;
	}
}

const char* getPipelineParamName (int value)
{
	switch (value)
	{
		case GL_ACTIVE_PROGRAM:			return "GL_ACTIVE_PROGRAM";
		case GL_INFO_LOG_LENGTH:		return "GL_INFO_LOG_LENGTH";
		case GL_VALIDATE_STATUS:		return "GL_VALIDATE_STATUS";
		case GL_VERTEX_SHADER:			return "GL_VERTEX_SHADER";
		case GL_FRAGMENT_SHADER:		return "GL_FRAGMENT_SHADER";
		case GL_COMPUTE_SHADER:			return "GL_COMPUTE_SHADER";
		case GL_GEOMETRY_SHADER:		return "GL_GEOMETRY_SHADER";
		case GL_TESS_CONTROL_SHADER:	return "GL_TESS_CONTROL_SHADER";
		case GL_TESS_EVALUATION_SHADER:	return "GL_TESS_EVALUATION_SHADER";
		default:						return nullptr;
	}
}

const char* getPatchParamName (int value)
{
	switch (value)
	{
		case GL_PATCH_VERTICES:	return "GL_PATCH_VERTICES";
		default:				return nullptr;
	}
}

const char* getTextureFormatName (int value)
{
	switch (value)
	{
		case GL_LUMINANCE:									return "GL_LUMINANCE";
		case GL_LUMINANCE_ALPHA:							return "GL_LUMINANCE_ALPHA";
		case GL_ALPHA:										return "GL_ALPHA";
		case GL_RGB:										return "GL_RGB";
		case GL_RGBA:										return "GL_RGBA";
		case GL_RGBA4:										return "GL_RGBA4";
		case GL_RGB4:										return "GL_RGB4";
		case GL_RGB5_A1:									return "GL_RGB5_A1";
		case GL_RGB565:										return "GL_RGB565";
		case GL_R3_G3_B2:									return "GL_R3_G3_B2";
		case GL_RGB5:										return "GL_RGB5";
		case GL_RGB12:										return "GL_RGB12";
		case GL_RGBA2:										return "GL_RGBA2";
		case GL_RGBA12:										return "GL_RGBA12";
		case GL_DEPTH_COMPONENT16:							return "GL_DEPTH_COMPONENT16";
		case GL_STENCIL_INDEX1:								return "GL_STENCIL_INDEX1";
		case GL_STENCIL_INDEX4:								return "GL_STENCIL_INDEX4";
		case GL_STENCIL_INDEX8:								return "GL_STENCIL_INDEX8";
		case GL_STENCIL_INDEX16:							return "GL_STENCIL_INDEX16";
		case GL_RG:											return "GL_RG";
		case GL_RED:										return "GL_RED";
		case GL_RGBA_INTEGER:								return "GL_RGBA_INTEGER";
		case GL_RGB_INTEGER:								return "GL_RGB_INTEGER";
		case GL_RG_INTEGER:									return "GL_RG_INTEGER";
		case GL_RED_INTEGER:								return "GL_RED_INTEGER";
		case GL_GREEN_INTEGER:								return "GL_GREEN_INTEGER";
		case GL_BLUE_INTEGER:								return "GL_BLUE_INTEGER";
		case GL_BGR_INTEGER:								return "GL_BGR_INTEGER";
		case GL_BGRA_INTEGER:								return "GL_BGRA_INTEGER";
		case GL_DEPTH_COMPONENT:							return "GL_DEPTH_COMPONENT";
		case GL_DEPTH_STENCIL:								return "GL_DEPTH_STENCIL";
		case GL_RGBA32F:									return "GL_RGBA32F";
		case GL_RGBA32I:									return "GL_RGBA32I";
		case GL_RGBA32UI:									return "GL_RGBA32UI";
		case GL_RGBA16:										return "GL_RGBA16";
		case GL_RGBA16_SNORM:								return "GL_RGBA16_SNORM";
		case GL_RGBA16F:									return "GL_RGBA16F";
		case GL_RGBA16I:									return "GL_RGBA16I";
		case GL_RGBA16UI:									return "GL_RGBA16UI";
		case GL_RGBA8:										return "GL_RGBA8";
		case GL_RGBA8I:										return "GL_RGBA8I";
		case GL_RGBA8UI:									return "GL_RGBA8UI";
		case GL_SRGB8_ALPHA8:								return "GL_SRGB8_ALPHA8";
		case GL_RGB10_A2:									return "GL_RGB10_A2";
		case GL_RGB10_A2UI:									return "GL_RGB10_A2UI";
		case GL_RGBA8_SNORM:								return "GL_RGBA8_SNORM";
		case GL_RGB8:										return "GL_RGB8";
		case GL_R11F_G11F_B10F:								return "GL_R11F_G11F_B10F";
		case GL_RGB32F:										return "GL_RGB32F";
		case GL_RGB32I:										return "GL_RGB32I";
		case GL_RGB32UI:									return "GL_RGB32UI";
		case GL_RGB16:										return "GL_RGB16";
		case GL_RGB16_SNORM:								return "GL_RGB16_SNORM";
		case GL_RGB16F:										return "GL_RGB16F";
		case GL_RGB16I:										return "GL_RGB16I";
		case GL_RGB16UI:									return "GL_RGB16UI";
		case GL_RGB8_SNORM:									return "GL_RGB8_SNORM";
		case GL_RGB8I:										return "GL_RGB8I";
		case GL_RGB8UI:										return "GL_RGB8UI";
		case GL_SRGB8:										return "GL_SRGB8";
		case GL_RGB9_E5:									return "GL_RGB9_E5";
		case GL_RG32F:										return "GL_RG32F";
		case GL_RG32I:										return "GL_RG32I";
		case GL_RG32UI:										return "GL_RG32UI";
		case GL_RG16:										return "GL_RG16";
		case GL_RG16_SNORM:									return "GL_RG16_SNORM";
		case GL_RG16F:										return "GL_RG16F";
		case GL_RG16I:										return "GL_RG16I";
		case GL_RG16UI:										return "GL_RG16UI";
		case GL_RG8:										return "GL_RG8";
		case GL_RG8I:										return "GL_RG8I";
		case GL_RG8UI:										return "GL_RG8UI";
		case GL_RG8_SNORM:									return "GL_RG8_SNORM";
		case GL_R32F:										return "GL_R32F";
		case GL_R32I:										return "GL_R32I";
		case GL_R32UI:										return "GL_R32UI";
		case GL_R16:										return "GL_R16";
		case GL_R16_SNORM:									return "GL_R16_SNORM";
		case GL_R16F:										return "GL_R16F";
		case GL_R16I:										return "GL_R16I";
		case GL_R16UI:										return "GL_R16UI";
		case GL_R8:											return "GL_R8";
		case GL_R8I:										return "GL_R8I";
		case GL_R8UI:										return "GL_R8UI";
		case GL_R8_SNORM:									return "GL_R8_SNORM";
		case GL_DEPTH_COMPONENT32F:							return "GL_DEPTH_COMPONENT32F";
		case GL_DEPTH_COMPONENT24:							return "GL_DEPTH_COMPONENT24";
		case GL_DEPTH32F_STENCIL8:							return "GL_DEPTH32F_STENCIL8";
		case GL_DEPTH24_STENCIL8:							return "GL_DEPTH24_STENCIL8";
		case GL_RGB10:										return "GL_RGB10";
		case GL_DEPTH_COMPONENT32:							return "GL_DEPTH_COMPONENT32";
		case GL_SRGB:										return "GL_SRGB";
		case GL_SRGB_ALPHA:									return "GL_SRGB_ALPHA";
		case GL_SR8_EXT:									return "GL_SR8_EXT";
		case GL_SRG8_EXT:									return "GL_SRG8_EXT";
		case GL_BGRA:										return "GL_BGRA";
		case GL_BGRA8_EXT:									return "GL_BGRA8_EXT";
		case GL_BGR:										return "GL_BGR";
		case GL_RED_SNORM:									return "GL_RED_SNORM";
		case GL_RG_SNORM:									return "GL_RG_SNORM";
		case GL_RGB_SNORM:									return "GL_RGB_SNORM";
		case GL_RGBA_SNORM:									return "GL_RGBA_SNORM";
		case GL_COMPRESSED_R11_EAC:							return "GL_COMPRESSED_R11_EAC";
		case GL_COMPRESSED_SIGNED_R11_EAC:					return "GL_COMPRESSED_SIGNED_R11_EAC";
		case GL_COMPRESSED_RG11_EAC:						return "GL_COMPRESSED_RG11_EAC";
		case GL_COMPRESSED_SIGNED_RG11_EAC:					return "GL_COMPRESSED_SIGNED_RG11_EAC";
		case GL_COMPRESSED_RGB8_ETC2:						return "GL_COMPRESSED_RGB8_ETC2";
		case GL_COMPRESSED_SRGB8_ETC2:						return "GL_COMPRESSED_SRGB8_ETC2";
		case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:	return "GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2";
		case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:	return "GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2";
		case GL_COMPRESSED_RGBA8_ETC2_EAC:					return "GL_COMPRESSED_RGBA8_ETC2_EAC";
		case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:			return "GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC";
		case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:				return "GL_COMPRESSED_RGBA_ASTC_4x4_KHR";
		case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:				return "GL_COMPRESSED_RGBA_ASTC_5x4_KHR";
		case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:				return "GL_COMPRESSED_RGBA_ASTC_5x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:				return "GL_COMPRESSED_RGBA_ASTC_6x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:				return "GL_COMPRESSED_RGBA_ASTC_6x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:				return "GL_COMPRESSED_RGBA_ASTC_8x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:				return "GL_COMPRESSED_RGBA_ASTC_8x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:				return "GL_COMPRESSED_RGBA_ASTC_8x8_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:				return "GL_COMPRESSED_RGBA_ASTC_10x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:				return "GL_COMPRESSED_RGBA_ASTC_10x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:				return "GL_COMPRESSED_RGBA_ASTC_10x8_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:				return "GL_COMPRESSED_RGBA_ASTC_10x10_KHR";
		case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:				return "GL_COMPRESSED_RGBA_ASTC_12x10_KHR";
		case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:				return "GL_COMPRESSED_RGBA_ASTC_12x12_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:		return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR";
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:				return "GL_COMPRESSED_RGB_S3TC_DXT1_EXT";
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:				return "GL_COMPRESSED_RGBA_S3TC_DXT1_EXT";
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:				return "GL_COMPRESSED_RGBA_S3TC_DXT3_EXT";
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:				return "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT";
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:			return "GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG";
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:			return "GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG";
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:			return "GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG";
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:			return "GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG";
		case GL_COMPRESSED_RED:								return "GL_COMPRESSED_RED";
		case GL_COMPRESSED_RG:								return "GL_COMPRESSED_RG";
		case GL_COMPRESSED_RGB:								return "GL_COMPRESSED_RGB";
		case GL_COMPRESSED_RGBA:							return "GL_COMPRESSED_RGBA";
		case GL_COMPRESSED_SRGB:							return "GL_COMPRESSED_SRGB";
		case GL_COMPRESSED_SRGB_ALPHA:						return "GL_COMPRESSED_SRGB_ALPHA";
		case GL_COMPRESSED_RED_RGTC1:						return "GL_COMPRESSED_RED_RGTC1";
		case GL_COMPRESSED_SIGNED_RED_RGTC1:				return "GL_COMPRESSED_SIGNED_RED_RGTC1";
		case GL_COMPRESSED_RG_RGTC2:						return "GL_COMPRESSED_RG_RGTC2";
		case GL_COMPRESSED_SIGNED_RG_RGTC2:					return "GL_COMPRESSED_SIGNED_RG_RGTC2";
		case GL_COMPRESSED_RGBA_BPTC_UNORM:					return "GL_COMPRESSED_RGBA_BPTC_UNORM";
		case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:			return "GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM";
		case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:			return "GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT";
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:			return "GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT";
		default:											return nullptr;
	}
}

const char* getGraphicsResetStatusName (int value)
{
	switch (value)
	{
		case GL_NO_ERROR:				return "GL_NO_ERROR";
		case GL_GUILTY_CONTEXT_RESET:	return "GL_GUILTY_CONTEXT_RESET";
		case GL_INNOCENT_CONTEXT_RESET:	return "GL_INNOCENT_CONTEXT_RESET";
		case GL_UNKNOWN_CONTEXT_RESET:	return "GL_UNKNOWN_CONTEXT_RESET";
		default:						return nullptr;
	}
}

const char* getClipDistanceParamName (int value)
{
	switch (value)
	{
		case GL_CLIP_DISTANCE0:							return "GL_CLIP_DISTANCE0";
		case GL_CLIP_DISTANCE1:							return "GL_CLIP_DISTANCE1";
		case GL_CLIP_DISTANCE2:							return "GL_CLIP_DISTANCE2";
		case GL_CLIP_DISTANCE3:							return "GL_CLIP_DISTANCE3";
		case GL_CLIP_DISTANCE4:							return "GL_CLIP_DISTANCE4";
		case GL_CLIP_DISTANCE5:							return "GL_CLIP_DISTANCE5";
		case GL_CLIP_DISTANCE6:							return "GL_CLIP_DISTANCE6";
		case GL_CLIP_DISTANCE7:							return "GL_CLIP_DISTANCE7";
		case GL_MAX_CLIP_DISTANCES:						return "GL_MAX_CLIP_DISTANCES";
		case GL_MAX_CULL_DISTANCES:						return "GL_MAX_CULL_DISTANCES";
		case GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES:	return "GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES";
		default:										return nullptr;
	}
}

const char* getConditionalRenderParamName (int value)
{
	switch (value)
	{
		case GL_QUERY_WAIT:							return "GL_QUERY_WAIT";
		case GL_QUERY_NO_WAIT:						return "GL_QUERY_NO_WAIT";
		case GL_QUERY_BY_REGION_WAIT:				return "GL_QUERY_BY_REGION_WAIT";
		case GL_QUERY_BY_REGION_NO_WAIT:			return "GL_QUERY_BY_REGION_NO_WAIT";
		case GL_QUERY_WAIT_INVERTED:				return "GL_QUERY_WAIT_INVERTED";
		case GL_QUERY_NO_WAIT_INVERTED:				return "GL_QUERY_NO_WAIT_INVERTED";
		case GL_QUERY_BY_REGION_WAIT_INVERTED:		return "GL_QUERY_BY_REGION_WAIT_INVERTED";
		case GL_QUERY_BY_REGION_NO_WAIT_INVERTED:	return "GL_QUERY_BY_REGION_NO_WAIT_INVERTED";
		default:									return nullptr;
	}
}

const char* getWaitEnumName (int value)
{
	switch (value)
	{
		case GL_ALREADY_SIGNALED:		return "GL_ALREADY_SIGNALED";
		case GL_TIMEOUT_EXPIRED:		return "GL_TIMEOUT_EXPIRED";
		case GL_CONDITION_SATISFIED:	return "GL_CONDITION_SATISFIED";
		case GL_WAIT_FAILED:			return "GL_WAIT_FAILED";
		default:						return nullptr;
	}
}

const char* getLogicOpParamsName (int value)
{
	switch (value)
	{
		case GL_CLEAR:			return "GL_CLEAR";
		case GL_SET:			return "GL_SET";
		case GL_COPY:			return "GL_COPY";
		case GL_COPY_INVERTED:	return "GL_COPY_INVERTED";
		case GL_NOOP:			return "GL_NOOP";
		case GL_INVERT:			return "GL_INVERT";
		case GL_AND:			return "GL_AND";
		case GL_NAND:			return "GL_NAND";
		case GL_OR:				return "GL_OR";
		case GL_NOR:			return "GL_NOR";
		case GL_XOR:			return "GL_XOR";
		case GL_EQUIV:			return "GL_EQUIV";
		case GL_AND_REVERSE:	return "GL_AND_REVERSE";
		case GL_AND_INVERTED:	return "GL_AND_INVERTED";
		case GL_OR_REVERSE:		return "GL_OR_REVERSE";
		case GL_OR_INVERTED:	return "GL_OR_INVERTED";
		default:				return nullptr;
	}
}

const char* getPolygonModeName (int value)
{
	switch (value)
	{
		case GL_LINE:	return "GL_LINE";
		case GL_POINT:	return "GL_POINT";
		case GL_FILL:	return "GL_FILL";
		default:		return nullptr;
	}
}

const char* getPrimSizeParamName (int value)
{
	switch (value)
	{
		case GL_ALIASED_POINT_SIZE_RANGE:		return "GL_ALIASED_POINT_SIZE_RANGE";
		case GL_POINT_SIZE:						return "GL_POINT_SIZE";
		case GL_POINT_FADE_THRESHOLD_SIZE:		return "GL_POINT_FADE_THRESHOLD_SIZE";
		case GL_SMOOTH_POINT_SIZE_RANGE:		return "GL_SMOOTH_POINT_SIZE_RANGE";
		case GL_SMOOTH_POINT_SIZE_GRANULARITY:	return "GL_SMOOTH_POINT_SIZE_GRANULARITY";
		case GL_POINT_SPRITE_COORD_ORIGIN:		return "GL_POINT_SPRITE_COORD_ORIGIN";
		case GL_SMOOTH_LINE_WIDTH_RANGE:		return "GL_SMOOTH_LINE_WIDTH_RANGE";
		case GL_SMOOTH_LINE_WIDTH_GRANULARITY:	return "GL_SMOOTH_LINE_WIDTH_GRANULARITY";
		case GL_ALIASED_LINE_WIDTH_RANGE:		return "GL_ALIASED_LINE_WIDTH_RANGE";
		default:								return nullptr;
	}
}

const char* getActiveTextureParamName (int value)
{
	switch (value)
	{
		case GL_TEXTURE0:	return "GL_TEXTURE0";
		case GL_TEXTURE1:	return "GL_TEXTURE1";
		case GL_TEXTURE2:	return "GL_TEXTURE2";
		case GL_TEXTURE3:	return "GL_TEXTURE3";
		case GL_TEXTURE4:	return "GL_TEXTURE4";
		case GL_TEXTURE5:	return "GL_TEXTURE5";
		case GL_TEXTURE6:	return "GL_TEXTURE6";
		case GL_TEXTURE7:	return "GL_TEXTURE7";
		case GL_TEXTURE8:	return "GL_TEXTURE8";
		case GL_TEXTURE9:	return "GL_TEXTURE9";
		case GL_TEXTURE10:	return "GL_TEXTURE10";
		case GL_TEXTURE11:	return "GL_TEXTURE11";
		case GL_TEXTURE12:	return "GL_TEXTURE12";
		case GL_TEXTURE13:	return "GL_TEXTURE13";
		case GL_TEXTURE14:	return "GL_TEXTURE14";
		case GL_TEXTURE15:	return "GL_TEXTURE15";
		case GL_TEXTURE16:	return "GL_TEXTURE16";
		case GL_TEXTURE17:	return "GL_TEXTURE17";
		case GL_TEXTURE18:	return "GL_TEXTURE18";
		case GL_TEXTURE19:	return "GL_TEXTURE19";
		case GL_TEXTURE20:	return "GL_TEXTURE20";
		case GL_TEXTURE21:	return "GL_TEXTURE21";
		case GL_TEXTURE22:	return "GL_TEXTURE22";
		case GL_TEXTURE23:	return "GL_TEXTURE23";
		case GL_TEXTURE24:	return "GL_TEXTURE24";
		case GL_TEXTURE25:	return "GL_TEXTURE25";
		case GL_TEXTURE26:	return "GL_TEXTURE26";
		case GL_TEXTURE27:	return "GL_TEXTURE27";
		case GL_TEXTURE28:	return "GL_TEXTURE28";
		case GL_TEXTURE29:	return "GL_TEXTURE29";
		case GL_TEXTURE30:	return "GL_TEXTURE30";
		case GL_TEXTURE31:	return "GL_TEXTURE31";
		default:			return nullptr;
	}
}

const char* getClipControlParamName (int value)
{
	switch (value)
	{
		case GL_LOWER_LEFT:				return "GL_LOWER_LEFT";
		case GL_UPPER_LEFT:				return "GL_UPPER_LEFT";
		case GL_NEGATIVE_ONE_TO_ONE:	return "GL_NEGATIVE_ONE_TO_ONE";
		case GL_ZERO_TO_ONE:			return "GL_ZERO_TO_ONE";
		default:						return nullptr;
	}
}

const char* getUniformSubroutinesParamName (int value)
{
	switch (value)
	{
		case GL_ACTIVE_SUBROUTINES:						return "GL_ACTIVE_SUBROUTINES";
		case GL_ACTIVE_SUBROUTINE_UNIFORMS:				return "GL_ACTIVE_SUBROUTINE_UNIFORMS";
		case GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS:	return "GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS";
		case GL_ACTIVE_SUBROUTINE_MAX_LENGTH:			return "GL_ACTIVE_SUBROUTINE_MAX_LENGTH";
		case GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH:	return "GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH";
		default:										return nullptr;
	}
}

tcu::Format::Bitfield<16> getBufferMaskStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(GL_DEPTH_BUFFER_BIT,	"GL_DEPTH_BUFFER_BIT"),
		tcu::Format::BitDesc(GL_STENCIL_BUFFER_BIT,	"GL_STENCIL_BUFFER_BIT"),
		tcu::Format::BitDesc(GL_COLOR_BUFFER_BIT,	"GL_COLOR_BUFFER_BIT"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}

tcu::Format::Bitfield<16> getBufferMapFlagsStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(GL_MAP_READ_BIT,				"GL_MAP_READ_BIT"),
		tcu::Format::BitDesc(GL_MAP_WRITE_BIT,				"GL_MAP_WRITE_BIT"),
		tcu::Format::BitDesc(GL_MAP_INVALIDATE_RANGE_BIT,	"GL_MAP_INVALIDATE_RANGE_BIT"),
		tcu::Format::BitDesc(GL_MAP_INVALIDATE_BUFFER_BIT,	"GL_MAP_INVALIDATE_BUFFER_BIT"),
		tcu::Format::BitDesc(GL_MAP_FLUSH_EXPLICIT_BIT,		"GL_MAP_FLUSH_EXPLICIT_BIT"),
		tcu::Format::BitDesc(GL_MAP_UNSYNCHRONIZED_BIT,		"GL_MAP_UNSYNCHRONIZED_BIT"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}

tcu::Format::Bitfield<16> getMemoryBarrierFlagsStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT,	"GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_ELEMENT_ARRAY_BARRIER_BIT,			"GL_ELEMENT_ARRAY_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_UNIFORM_BARRIER_BIT,				"GL_UNIFORM_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_TEXTURE_FETCH_BARRIER_BIT,			"GL_TEXTURE_FETCH_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,	"GL_SHADER_IMAGE_ACCESS_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_COMMAND_BARRIER_BIT,				"GL_COMMAND_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_PIXEL_BUFFER_BARRIER_BIT,			"GL_PIXEL_BUFFER_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_TEXTURE_UPDATE_BARRIER_BIT,			"GL_TEXTURE_UPDATE_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_BUFFER_UPDATE_BARRIER_BIT,			"GL_BUFFER_UPDATE_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_FRAMEBUFFER_BARRIER_BIT,			"GL_FRAMEBUFFER_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_TRANSFORM_FEEDBACK_BARRIER_BIT,		"GL_TRANSFORM_FEEDBACK_BARRIER_BIT"),
		tcu::Format::BitDesc(GL_ATOMIC_COUNTER_BARRIER_BIT,			"GL_ATOMIC_COUNTER_BARRIER_BIT"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}

tcu::Format::Bitfield<16> getShaderTypeMaskStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(GL_VERTEX_SHADER_BIT,			"GL_VERTEX_SHADER_BIT"),
		tcu::Format::BitDesc(GL_FRAGMENT_SHADER_BIT,		"GL_FRAGMENT_SHADER_BIT"),
		tcu::Format::BitDesc(GL_COMPUTE_SHADER_BIT,			"GL_COMPUTE_SHADER_BIT"),
		tcu::Format::BitDesc(GL_GEOMETRY_SHADER_BIT,		"GL_GEOMETRY_SHADER_BIT"),
		tcu::Format::BitDesc(GL_TESS_CONTROL_SHADER_BIT,	"GL_TESS_CONTROL_SHADER_BIT"),
		tcu::Format::BitDesc(GL_TESS_EVALUATION_SHADER_BIT,	"GL_TESS_EVALUATION_SHADER_BIT"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}

tcu::Format::Bitfield<16> getContextMaskStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(GL_CONTEXT_FLAG_NO_ERROR_BIT,				"GL_CONTEXT_FLAG_NO_ERROR_BIT"),
		tcu::Format::BitDesc(GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT,			"GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT"),
		tcu::Format::BitDesc(GL_CONTEXT_FLAG_DEBUG_BIT,					"GL_CONTEXT_FLAG_DEBUG_BIT"),
		tcu::Format::BitDesc(GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT,	"GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}

tcu::Format::Bitfield<16> getClientWaitMaskStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(GL_SYNC_FLUSH_COMMANDS_BIT,	"GL_SYNC_FLUSH_COMMANDS_BIT"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}

tcu::Format::Bitfield<16> getContextProfileMaskStr (int value)
{
	static const tcu::Format::BitDesc s_desc[] =
	{
		tcu::Format::BitDesc(GL_CONTEXT_CORE_PROFILE_BIT,			"GL_CONTEXT_CORE_PROFILE_BIT"),
		tcu::Format::BitDesc(GL_CONTEXT_COMPATIBILITY_PROFILE_BIT,	"GL_CONTEXT_COMPATIBILITY_PROFILE_BIT"),
	};
	return tcu::Format::Bitfield<16>(value, &s_desc[0], &s_desc[DE_LENGTH_OF_ARRAY(s_desc)]);
}
