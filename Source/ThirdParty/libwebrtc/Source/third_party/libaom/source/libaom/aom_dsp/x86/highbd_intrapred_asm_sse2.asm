;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;

%include "third_party/x86inc/x86inc.asm"

SECTION_RODATA
pw_4:  times 8 dw 4
pw_8:  times 8 dw 8
pw_16: times 4 dd 16
pw_32: times 4 dd 32

SECTION .text
INIT_XMM sse2
cglobal highbd_dc_predictor_4x4, 4, 5, 4, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  movq                  m0, [aboveq]
  movq                  m2, [leftq]
  paddw                 m0, m2
  pshuflw               m1, m0, 0xe
  paddw                 m0, m1
  pshuflw               m1, m0, 0x1
  paddw                 m0, m1
  paddw                 m0, [GLOBAL(pw_4)]
  psraw                 m0, 3
  pshuflw               m0, m0, 0x0
  movq    [dstq          ], m0
  movq    [dstq+strideq*2], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq*2], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal highbd_dc_predictor_8x8, 4, 5, 4, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [aboveq]
  mova                  m2, [leftq]
  DEFINE_ARGS dst, stride, stride3, one
  mov                 oned, 0x00010001
  lea             stride3q, [strideq*3]
  movd                  m3, oned
  pshufd                m3, m3, 0x0
  paddw                 m0, m2
  pmaddwd               m0, m3
  packssdw              m0, m1
  pmaddwd               m0, m3
  packssdw              m0, m1
  pmaddwd               m0, m3
  paddw                 m0, [GLOBAL(pw_8)]
  psrlw                 m0, 4
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  mova   [dstq           ], m0
  mova   [dstq+strideq*2 ], m0
  mova   [dstq+strideq*4 ], m0
  mova   [dstq+stride3q*2], m0
  lea                 dstq, [dstq+strideq*8]
  mova   [dstq           ], m0
  mova   [dstq+strideq*2 ], m0
  mova   [dstq+strideq*4 ], m0
  mova   [dstq+stride3q*2], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal highbd_dc_predictor_16x16, 4, 5, 5, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [aboveq]
  mova                  m3, [aboveq+16]
  mova                  m2, [leftq]
  mova                  m4, [leftq+16]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 4
  paddw                 m0, m2
  paddw                 m0, m3
  paddw                 m0, m4
  movhlps               m2, m0
  paddw                 m0, m2
  punpcklwd             m0, m1
  movhlps               m2, m0
  paddd                 m0, m2
  punpckldq             m0, m1
  movhlps               m2, m0
  paddd                 m0, m2
  paddd                 m0, [GLOBAL(pw_16)]
  psrad                 m0, 5
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
.loop:
  mova   [dstq              ], m0
  mova   [dstq           +16], m0
  mova   [dstq+strideq*2    ], m0
  mova   [dstq+strideq*2 +16], m0
  mova   [dstq+strideq*4    ], m0
  mova   [dstq+strideq*4 +16], m0
  mova   [dstq+stride3q*2   ], m0
  mova   [dstq+stride3q*2+16], m0
  lea                 dstq, [dstq+strideq*8]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_XMM sse2
cglobal highbd_dc_predictor_32x32, 4, 5, 7, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  mova                  m0, [aboveq]
  mova                  m2, [aboveq+16]
  mova                  m3, [aboveq+32]
  mova                  m4, [aboveq+48]
  paddw                 m0, m2
  paddw                 m3, m4
  mova                  m2, [leftq]
  mova                  m4, [leftq+16]
  mova                  m5, [leftq+32]
  mova                  m6, [leftq+48]
  paddw                 m2, m4
  paddw                 m5, m6
  paddw                 m0, m3
  paddw                 m2, m5
  pxor                  m1, m1
  paddw                 m0, m2
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 8
  movhlps               m2, m0
  paddw                 m0, m2
  punpcklwd             m0, m1
  movhlps               m2, m0
  paddd                 m0, m2
  punpckldq             m0, m1
  movhlps               m2, m0
  paddd                 m0, m2
  paddd                 m0, [GLOBAL(pw_32)]
  psrad                 m0, 6
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
.loop:
  mova [dstq               ], m0
  mova [dstq          +16  ], m0
  mova [dstq          +32  ], m0
  mova [dstq          +48  ], m0
  mova [dstq+strideq*2     ], m0
  mova [dstq+strideq*2+16  ], m0
  mova [dstq+strideq*2+32  ], m0
  mova [dstq+strideq*2+48  ], m0
  mova [dstq+strideq*4     ], m0
  mova [dstq+strideq*4+16  ], m0
  mova [dstq+strideq*4+32  ], m0
  mova [dstq+strideq*4+48  ], m0
  mova [dstq+stride3q*2    ], m0
  mova [dstq+stride3q*2 +16], m0
  mova [dstq+stride3q*2 +32], m0
  mova [dstq+stride3q*2 +48], m0
  lea                 dstq, [dstq+strideq*8]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_XMM sse2
cglobal highbd_v_predictor_4x4, 3, 3, 1, dst, stride, above
  movq                  m0, [aboveq]
  movq    [dstq          ], m0
  movq    [dstq+strideq*2], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq*2], m0
  RET

INIT_XMM sse2
cglobal highbd_v_predictor_8x8, 3, 3, 1, dst, stride, above
  mova                  m0, [aboveq]
  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  mova   [dstq           ], m0
  mova   [dstq+strideq*2 ], m0
  mova   [dstq+strideq*4 ], m0
  mova   [dstq+stride3q*2], m0
  lea                 dstq, [dstq+strideq*8]
  mova   [dstq           ], m0
  mova   [dstq+strideq*2 ], m0
  mova   [dstq+strideq*4 ], m0
  mova   [dstq+stride3q*2], m0
  RET

INIT_XMM sse2
cglobal highbd_v_predictor_16x16, 3, 4, 2, dst, stride, above
  mova                  m0, [aboveq]
  mova                  m1, [aboveq+16]
  DEFINE_ARGS dst, stride, stride3, nlines4
  lea             stride3q, [strideq*3]
  mov              nlines4d, 4
.loop:
  mova    [dstq              ], m0
  mova    [dstq           +16], m1
  mova    [dstq+strideq*2    ], m0
  mova    [dstq+strideq*2 +16], m1
  mova    [dstq+strideq*4    ], m0
  mova    [dstq+strideq*4 +16], m1
  mova    [dstq+stride3q*2   ], m0
  mova    [dstq+stride3q*2+16], m1
  lea                 dstq, [dstq+strideq*8]
  dec             nlines4d
  jnz .loop
  REP_RET

INIT_XMM sse2
cglobal highbd_v_predictor_32x32, 3, 4, 4, dst, stride, above
  mova                  m0, [aboveq]
  mova                  m1, [aboveq+16]
  mova                  m2, [aboveq+32]
  mova                  m3, [aboveq+48]
  DEFINE_ARGS dst, stride, stride3, nlines4
  lea             stride3q, [strideq*3]
  mov              nlines4d, 8
.loop:
  mova [dstq               ], m0
  mova [dstq            +16], m1
  mova [dstq            +32], m2
  mova [dstq            +48], m3
  mova [dstq+strideq*2     ], m0
  mova [dstq+strideq*2  +16], m1
  mova [dstq+strideq*2  +32], m2
  mova [dstq+strideq*2  +48], m3
  mova [dstq+strideq*4     ], m0
  mova [dstq+strideq*4  +16], m1
  mova [dstq+strideq*4  +32], m2
  mova [dstq+strideq*4  +48], m3
  mova [dstq+stride3q*2    ], m0
  mova [dstq+stride3q*2 +16], m1
  mova [dstq+stride3q*2 +32], m2
  mova [dstq+stride3q*2 +48], m3
  lea                 dstq, [dstq+strideq*8]
  dec             nlines4d
  jnz .loop
  REP_RET
