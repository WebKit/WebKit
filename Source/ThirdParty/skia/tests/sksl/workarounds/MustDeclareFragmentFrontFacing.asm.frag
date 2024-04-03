               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %sk_FragColor %sk_Clockwise
               OpExecutionMode %main OriginUpperLeft
               OpName %sk_FragColor "sk_FragColor"
               OpName %sk_Clockwise "sk_Clockwise"
               OpName %main "main"
               OpDecorate %sk_FragColor RelaxedPrecision
               OpDecorate %sk_FragColor Location 0
               OpDecorate %sk_FragColor Index 0
               OpDecorate %sk_Clockwise BuiltIn FrontFacing
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%sk_FragColor = OpVariable %_ptr_Output_v4float Output
       %bool = OpTypeBool
%_ptr_Input_bool = OpTypePointer Input %bool
%sk_Clockwise = OpVariable %_ptr_Input_bool Input
       %void = OpTypeVoid
         %11 = OpTypeFunction %void
    %float_1 = OpConstant %float 1
         %14 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
       %main = OpFunction %void None %11
         %12 = OpLabel
               OpStore %sk_FragColor %14
               OpReturn
               OpFunctionEnd
