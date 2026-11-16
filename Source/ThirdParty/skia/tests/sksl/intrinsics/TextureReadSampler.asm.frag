               OpCapability Shader
          %5 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %sk_FragColor
               OpExecutionMode %main OriginUpperLeft

               ; Debug Information
               OpName %sk_FragColor "sk_FragColor"  ; id %7
               OpName %t "t"                        ; id %11
               OpName %main "main"                  ; id %6

               ; Annotations
               OpDecorate %sk_FragColor RelaxedPrecision
               OpDecorate %sk_FragColor Location 0
               OpDecorate %sk_FragColor Index 0
               OpDecorate %t RelaxedPrecision
               OpDecorate %t Binding 0
               OpDecorate %t DescriptorSet 0
               OpDecorate %18 RelaxedPrecision
               OpDecorate %19 RelaxedPrecision

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
     %uint_0 = OpConstant %uint 0
     %v2uint = OpTypeVector %uint 2
         %23 = OpConstantComposite %v2uint %uint_0 %uint_0
      %int_0 = OpConstant %int 0


               ; Function main
       %main = OpFunction %void None %16

         %17 = OpLabel
         %19 =   OpLoad %13 %t                      ; RelaxedPrecision
         %24 =   OpImage %12 %19
         %18 =   OpImageFetch %v4float %24 %23 Lod %int_0   ; RelaxedPrecision
                 OpStore %sk_FragColor %18
                 OpReturn
               OpFunctionEnd
