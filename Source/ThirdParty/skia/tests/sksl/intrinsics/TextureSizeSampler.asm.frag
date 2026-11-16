               OpCapability ImageQuery
               OpCapability Shader
          %5 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %sk_FragColor
               OpExecutionMode %main OriginUpperLeft

               ; Debug Information
               OpName %sk_FragColor "sk_FragColor"  ; id %7
               OpName %t "t"                        ; id %11
               OpName %main "main"                  ; id %6
               OpName %dims "dims"                  ; id %18

               ; Annotations
               OpDecorate %sk_FragColor RelaxedPrecision
               OpDecorate %sk_FragColor Location 0
               OpDecorate %sk_FragColor Index 0
               OpDecorate %t RelaxedPrecision
               OpDecorate %t Binding 0
               OpDecorate %t DescriptorSet 0
               OpDecorate %23 RelaxedPrecision
               OpDecorate %26 RelaxedPrecision
               OpDecorate %27 RelaxedPrecision
               OpDecorate %29 RelaxedPrecision
               OpDecorate %31 RelaxedPrecision
               OpDecorate %33 RelaxedPrecision

               ; Types, variables and constants
        %int = OpTypeInt 32 1
%_ptr_Input_int = OpTypePointer Input %int
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%sk_FragColor = OpVariable %_ptr_Output_v4float Output  ; RelaxedPrecision, Location 0, Index 0
         %12 = OpTypeImage %float 2D 0 0 0 1 Unknown
         %13 = OpTypeSampledImage %12
%_ptr_UniformConstant_13 = OpTypePointer UniformConstant %13
          %t = OpVariable %_ptr_UniformConstant_13 UniformConstant  ; RelaxedPrecision, Binding 0, DescriptorSet 0
       %void = OpTypeVoid
         %16 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
     %v2uint = OpTypeVector %uint 2
%_ptr_Function_v2uint = OpTypePointer Function %v2uint
      %int_0 = OpConstant %int 0
    %v2float = OpTypeVector %float 2


               ; Function main
       %main = OpFunction %void None %16

         %17 = OpLabel
       %dims =   OpVariable %_ptr_Function_v2uint Function
         %23 =   OpLoad %13 %t                      ; RelaxedPrecision
         %24 =   OpImage %12 %23
         %22 =   OpImageQuerySizeLod %v2uint %24 %int_0
                 OpStore %dims %22
         %27 =   OpLoad %13 %t                      ; RelaxedPrecision
         %28 =   OpCompositeExtract %uint %22 0
         %29 =   OpConvertUToF %float %28           ; RelaxedPrecision
         %30 =   OpCompositeExtract %uint %22 1
         %31 =   OpConvertUToF %float %30           ; RelaxedPrecision
         %33 =   OpCompositeConstruct %v2float %29 %31  ; RelaxedPrecision
         %26 =   OpImageSampleImplicitLod %v4float %27 %33  ; RelaxedPrecision
                 OpStore %sk_FragColor %26
                 OpReturn
               OpFunctionEnd
