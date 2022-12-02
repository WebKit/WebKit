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
pb_1: times 16 db 1
pw_4:  times 8 dw 4
pw_8:  times 8 dw 8
pw_16: times 8 dw 16
pw_32: times 8 dw 32
dc_128: times 16 db 128
pw2_4:  times 8 dw 2
pw2_8:  times 8 dw 4
pw2_16:  times 8 dw 8
pw2_32:  times 8 dw 16

SECTION .text

INIT_XMM sse2
cglobal dc_predictor_4x4, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  movd                  m2, [leftq]
  movd                  m0, [aboveq]
  pxor                  m1, m1
  punpckldq             m0, m2
  psadbw                m0, m1
  paddw                 m0, [GLOBAL(pw_4)]
  psraw                 m0, 3
  pshuflw               m0, m0, 0x0
  packuswb              m0, m0
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0
  lea                 dstq, [dstq+strideq*2]
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_left_predictor_4x4, 2, 5, 2, dst, stride, above, left, goffset
  movifnidn          leftq, leftmp
  GET_GOT     goffsetq

  pxor                  m1, m1
  movd                  m0, [leftq]
  psadbw                m0, m1
  paddw                 m0, [GLOBAL(pw2_4)]
  psraw                 m0, 2
  pshuflw               m0, m0, 0x0
  packuswb              m0, m0
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0
  lea                 dstq, [dstq+strideq*2]
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_top_predictor_4x4, 3, 5, 2, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  movd                  m0, [aboveq]
  psadbw                m0, m1
  paddw                 m0, [GLOBAL(pw2_4)]
  psraw                 m0, 2
  pshuflw               m0, m0, 0x0
  packuswb              m0, m0
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0
  lea                 dstq, [dstq+strideq*2]
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_predictor_8x8, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  movq                  m0, [aboveq]
  movq                  m2, [leftq]
  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  psadbw                m0, m1
  psadbw                m2, m1
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw_8)]
  psraw                 m0, 4
  punpcklbw             m0, m0
  pshuflw               m0, m0, 0x0
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_top_predictor_8x8, 3, 5, 2, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  movq                  m0, [aboveq]
  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  psadbw                m0, m1
  paddw                 m0, [GLOBAL(pw2_8)]
  psraw                 m0, 3
  punpcklbw             m0, m0
  pshuflw               m0, m0, 0x0
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_left_predictor_8x8, 2, 5, 2, dst, stride, above, left, goffset
  movifnidn          leftq, leftmp
  GET_GOT     goffsetq

  pxor                  m1, m1
  movq                  m0, [leftq]
  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  psadbw                m0, m1
  paddw                 m0, [GLOBAL(pw2_8)]
  psraw                 m0, 3
  punpcklbw             m0, m0
  pshuflw               m0, m0, 0x0
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_128_predictor_4x4, 2, 5, 1, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  movd     m0,        [GLOBAL(dc_128)]
  movd    [dstq          ], m0
  movd    [dstq+strideq  ], m0
  movd    [dstq+strideq*2], m0
  movd    [dstq+stride3q ], m0
  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_128_predictor_8x8, 2, 5, 1, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  movq    m0,        [GLOBAL(dc_128)]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_predictor_16x16, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [aboveq]
  mova                  m2, [leftq]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 4
  psadbw                m0, m1
  psadbw                m2, m1
  paddw                 m0, m2
  movhlps               m2, m0
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw_16)]
  psraw                 m0, 5
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  packuswb              m0, m0
.loop:
  mova    [dstq          ], m0
  mova    [dstq+strideq  ], m0
  mova    [dstq+strideq*2], m0
  mova    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET


INIT_XMM sse2
cglobal dc_top_predictor_16x16, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [aboveq]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 4
  psadbw                m0, m1
  movhlps               m2, m0
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw2_16)]
  psraw                 m0, 4
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  packuswb              m0, m0
.loop:
  mova    [dstq          ], m0
  mova    [dstq+strideq  ], m0
  mova    [dstq+strideq*2], m0
  mova    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_XMM sse2
cglobal dc_left_predictor_16x16, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [leftq]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 4
  psadbw                m0, m1
  movhlps               m2, m0
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw2_16)]
  psraw                 m0, 4
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  packuswb              m0, m0
.loop:
  mova    [dstq          ], m0
  mova    [dstq+strideq  ], m0
  mova    [dstq+strideq*2], m0
  mova    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_XMM sse2
cglobal dc_128_predictor_16x16, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 4
  mova    m0,        [GLOBAL(dc_128)]
.loop:
  mova    [dstq          ], m0
  mova    [dstq+strideq  ], m0
  mova    [dstq+strideq*2], m0
  mova    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop
  RESTORE_GOT
  RET


INIT_XMM sse2
cglobal dc_predictor_32x32, 4, 5, 5, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [aboveq]
  mova                  m2, [aboveq+16]
  mova                  m3, [leftq]
  mova                  m4, [leftq+16]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 8
  psadbw                m0, m1
  psadbw                m2, m1
  psadbw                m3, m1
  psadbw                m4, m1
  paddw                 m0, m2
  paddw                 m0, m3
  paddw                 m0, m4
  movhlps               m2, m0
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw_32)]
  psraw                 m0, 6
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  packuswb              m0, m0
.loop:
  mova [dstq             ], m0
  mova [dstq          +16], m0
  mova [dstq+strideq     ], m0
  mova [dstq+strideq  +16], m0
  mova [dstq+strideq*2   ], m0
  mova [dstq+strideq*2+16], m0
  mova [dstq+stride3q    ], m0
  mova [dstq+stride3q +16], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_XMM sse2
cglobal dc_top_predictor_32x32, 4, 5, 5, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [aboveq]
  mova                  m2, [aboveq+16]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 8
  psadbw                m0, m1
  psadbw                m2, m1
  paddw                 m0, m2
  movhlps               m2, m0
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw2_32)]
  psraw                 m0, 5
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  packuswb              m0, m0
.loop:
  mova [dstq             ], m0
  mova [dstq          +16], m0
  mova [dstq+strideq     ], m0
  mova [dstq+strideq  +16], m0
  mova [dstq+strideq*2   ], m0
  mova [dstq+strideq*2+16], m0
  mova [dstq+stride3q    ], m0
  mova [dstq+stride3q +16], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_XMM sse2
cglobal dc_left_predictor_32x32, 4, 5, 5, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [leftq]
  mova                  m2, [leftq+16]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 8
  psadbw                m0, m1
  psadbw                m2, m1
  paddw                 m0, m2
  movhlps               m2, m0
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw2_32)]
  psraw                 m0, 5
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  packuswb              m0, m0
.loop:
  mova [dstq             ], m0
  mova [dstq          +16], m0
  mova [dstq+strideq     ], m0
  mova [dstq+strideq  +16], m0
  mova [dstq+strideq*2   ], m0
  mova [dstq+strideq*2+16], m0
  mova [dstq+stride3q    ], m0
  mova [dstq+stride3q +16], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_XMM sse2
cglobal dc_128_predictor_32x32, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 8
  mova    m0,        [GLOBAL(dc_128)]
.loop:
  mova [dstq             ], m0
  mova [dstq          +16], m0
  mova [dstq+strideq     ], m0
  mova [dstq+strideq  +16], m0
  mova [dstq+strideq*2   ], m0
  mova [dstq+strideq*2+16], m0
  mova [dstq+stride3q    ], m0
  mova [dstq+stride3q +16], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop
  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal v_predictor_4x4, 3, 3, 1, dst, stride, above
  movd                  m0, [aboveq]
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0
  lea                 dstq, [dstq+strideq*2]
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0
  RET

INIT_XMM sse2
cglobal v_predictor_8x8, 3, 3, 1, dst, stride, above
  movq                  m0, [aboveq]
  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  RET

INIT_XMM sse2
cglobal v_predictor_16x16, 3, 4, 1, dst, stride, above
  mova                  m0, [aboveq]
  DEFINE_ARGS dst, stride, stride3, nlines4
  lea             stride3q, [strideq*3]
  mov              nlines4d, 4
.loop:
  mova    [dstq          ], m0
  mova    [dstq+strideq  ], m0
  mova    [dstq+strideq*2], m0
  mova    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  dec             nlines4d
  jnz .loop
  REP_RET

INIT_XMM sse2
cglobal v_predictor_32x32, 3, 4, 2, dst, stride, above
  mova                  m0, [aboveq]
  mova                  m1, [aboveq+16]
  DEFINE_ARGS dst, stride, stride3, nlines4
  lea             stride3q, [strideq*3]
  mov              nlines4d, 8
.loop:
  mova [dstq             ], m0
  mova [dstq          +16], m1
  mova [dstq+strideq     ], m0
  mova [dstq+strideq  +16], m1
  mova [dstq+strideq*2   ], m0
  mova [dstq+strideq*2+16], m1
  mova [dstq+stride3q    ], m0
  mova [dstq+stride3q +16], m1
  lea                 dstq, [dstq+strideq*4]
  dec             nlines4d
  jnz .loop
  REP_RET

INIT_XMM sse2
cglobal h_predictor_4x4, 2, 4, 4, dst, stride, line, left
  movifnidn          leftq, leftmp
  movd                  m0, [leftq]
  punpcklbw             m0, m0
  punpcklbw             m0, m0
  pshufd                m1, m0, 0x1
  movd      [dstq        ], m0
  movd      [dstq+strideq], m1
  pshufd                m2, m0, 0x2
  lea                 dstq, [dstq+strideq*2]
  pshufd                m3, m0, 0x3
  movd      [dstq        ], m2
  movd      [dstq+strideq], m3
  RET

INIT_XMM sse2
cglobal h_predictor_8x8, 2, 5, 3, dst, stride, line, left
  movifnidn          leftq, leftmp
  mov                lineq, -2
  DEFINE_ARGS  dst, stride, line, left, stride3
  lea             stride3q, [strideq*3]
  movq                  m0, [leftq    ]
  punpcklbw             m0, m0              ; l1 l1 l2 l2 ... l8 l8
.loop:
  pshuflw               m1, m0, 0x0         ; l1 l1 l1 l1 l1 l1 l1 l1
  pshuflw               m2, m0, 0x55        ; l2 l2 l2 l2 l2 l2 l2 l2
  movq      [dstq        ], m1
  movq      [dstq+strideq], m2
  pshuflw               m1, m0, 0xaa
  pshuflw               m2, m0, 0xff
  movq    [dstq+strideq*2], m1
  movq    [dstq+stride3q ], m2
  pshufd                m0, m0, 0xe         ; [63:0] l5 l5 l6 l6 l7 l7 l8 l8
  inc                lineq
  lea                 dstq, [dstq+strideq*4]
  jnz .loop
  REP_RET

INIT_XMM sse2
cglobal h_predictor_16x16, 2, 5, 3, dst, stride, line, left
  movifnidn          leftq, leftmp
  mov                lineq, -4
  DEFINE_ARGS dst, stride, line, left, stride3
  lea             stride3q, [strideq*3]
.loop:
  movd                  m0, [leftq]
  punpcklbw             m0, m0
  punpcklbw             m0, m0              ; l1 to l4 each repeated 4 times
  pshufd            m1, m0, 0x0             ; l1 repeated 16 times
  pshufd            m2, m0, 0x55            ; l2 repeated 16 times
  mova    [dstq          ], m1
  mova    [dstq+strideq  ], m2
  pshufd            m1, m0, 0xaa
  pshufd            m2, m0, 0xff
  mova    [dstq+strideq*2], m1
  mova    [dstq+stride3q ], m2
  inc                lineq
  lea                leftq, [leftq+4       ]
  lea                 dstq, [dstq+strideq*4]
  jnz .loop
  REP_RET

INIT_XMM sse2
cglobal h_predictor_32x32, 2, 5, 3, dst, stride, line, left
  movifnidn              leftq, leftmp
  mov                    lineq, -8
  DEFINE_ARGS dst, stride, line, left, stride3
  lea                 stride3q, [strideq*3]
.loop:
  movd                      m0, [leftq]
  punpcklbw                 m0, m0
  punpcklbw                 m0, m0              ; l1 to l4 each repeated 4 times
  pshufd                m1, m0, 0x0             ; l1 repeated 16 times
  pshufd                m2, m0, 0x55            ; l2 repeated 16 times
  mova     [dstq             ], m1
  mova     [dstq+16          ], m1
  mova     [dstq+strideq     ], m2
  mova     [dstq+strideq+16  ], m2
  pshufd                m1, m0, 0xaa
  pshufd                m2, m0, 0xff
  mova     [dstq+strideq*2   ], m1
  mova     [dstq+strideq*2+16], m1
  mova     [dstq+stride3q    ], m2
  mova     [dstq+stride3q+16 ], m2
  inc                    lineq
  lea                    leftq, [leftq+4       ]
  lea                     dstq, [dstq+strideq*4]
  jnz .loop
  REP_RET
