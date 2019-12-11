			section .data align=16

			align 16

			mmx_yuy2_00ff	dw 000ffh, 000ffh, 000ffh, 000ffh

			section .text

; -- Import/Export ------------------------------------------------------

			global mb_yuy2yuv_mmx

			struc macroblock_yuy2_param
			.dsty:		resd 1
			.dstu:		resd 1
			.dstv:		resd 1
			.dypitch:	resd 1
			.dcpitch:	resd 1
			.src:		resd 1
			.spitch:	resd 1
			endstruc

; -----------------------------------------------------------------------
;  -= mb_yuy2yuv_mmx =-
;  
;  extern "C" int __fastcall mb_yuy2yuv_mmx( macroblock_param *param );
; -----------------------------------------------------------------------

			align 16

mb_yuy2yuv_mmx:

			push		ebx
			push		edx

			push		esi
			push		edi
			push		ebp

			mov			esi, [ecx + macroblock_yuy2_param.src]
			mov			edx, [ecx + macroblock_yuy2_param.spitch]

			mov			edi, [ecx + macroblock_yuy2_param.dsty]
			mov			eax, [ecx + macroblock_yuy2_param.dstu]
			mov			ebx, [ecx + macroblock_yuy2_param.dstv]

			mov			ebp, [ecx + macroblock_yuy2_param.dypitch]
			mov			ecx, [ecx + macroblock_yuy2_param.dcpitch]

			movq		mm6, [mmx_yuy2_00ff]

			pop			ebp
			pop			edi
			pop			esi

			pop			edx
			pop			ebx

			retn

