               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %sk_FragColor
               OpExecutionMode %main OriginUpperLeft
               OpName %sk_FragColor "sk_FragColor"
               OpName %_UniformBuffer "_UniformBuffer"
               OpMemberName %_UniformBuffer 0 "src"
               OpMemberName %_UniformBuffer 1 "dst"
               OpName %main "main"
               OpDecorate %sk_FragColor RelaxedPrecision
               OpDecorate %sk_FragColor Location 0
               OpDecorate %sk_FragColor Index 0
               OpMemberDecorate %_UniformBuffer 0 Offset 0
               OpMemberDecorate %_UniformBuffer 0 RelaxedPrecision
               OpMemberDecorate %_UniformBuffer 1 Offset 16
               OpMemberDecorate %_UniformBuffer 1 RelaxedPrecision
               OpDecorate %_UniformBuffer Block
               OpDecorate %7 Binding 0
               OpDecorate %7 DescriptorSet 0
               OpDecorate %17 RelaxedPrecision
               OpDecorate %20 RelaxedPrecision
               OpDecorate %22 RelaxedPrecision
               OpDecorate %25 RelaxedPrecision
               OpDecorate %26 RelaxedPrecision
               OpDecorate %27 RelaxedPrecision
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%sk_FragColor = OpVariable %_ptr_Output_v4float Output
%_UniformBuffer = OpTypeStruct %v4float %v4float
%_ptr_Uniform__UniformBuffer = OpTypePointer Uniform %_UniformBuffer
          %7 = OpVariable %_ptr_Uniform__UniformBuffer Uniform
       %void = OpTypeVoid
         %11 = OpTypeFunction %void
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
    %float_1 = OpConstant %float 1
         %21 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
      %int_1 = OpConstant %int 1
       %main = OpFunction %void None %11
         %12 = OpLabel
         %13 = OpAccessChain %_ptr_Uniform_v4float %7 %int_0
         %17 = OpLoad %v4float %13
         %19 = OpAccessChain %_ptr_Uniform_v4float %7 %int_0
         %20 = OpLoad %v4float %19
         %22 = OpFSub %v4float %21 %20
         %23 = OpAccessChain %_ptr_Uniform_v4float %7 %int_1
         %25 = OpLoad %v4float %23
         %26 = OpFMul %v4float %22 %25
         %27 = OpFAdd %v4float %17 %26
               OpStore %sk_FragColor %27
               OpReturn
               OpFunctionEnd
