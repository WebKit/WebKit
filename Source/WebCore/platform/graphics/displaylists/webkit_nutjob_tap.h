/**
 * Nutjob command tap — inserted into GraphicsContextCG to capture drawing
 * commands alongside the normal CoreGraphics rendering.
 *
 * When NUTJOB_TAP_FD environment variable is set, opens that fd and writes
 * nutjob protocol commands for every drawing operation. When not set, does nothing.
 *
 * Usage:
 *   1. Apply the nutjob patch to GraphicsContextCG.cpp
 *   2. Rebuild WebKit
 *   3. Run with: NUTJOB_TAP_FD=1 (stdout) or pipe to a named pipe
 *
 * This is a "tee" — WebKit still renders normally via CG, but we also
 * capture the drawing commands for replay through nutjob's Java renderer.
 */

#ifndef WEBKIT_NUTJOB_TAP_H
#define WEBKIT_NUTJOB_TAP_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* Nutjob protocol opcodes (must match nutjob_protocol.h) */
#define NJT_SAVE              0x01
#define NJT_RESTORE           0x02
#define NJT_SET_FILL_COLOR    0x03
#define NJT_SET_STROKE_COLOR  0x04
#define NJT_SET_LINE_WIDTH    0x05
#define NJT_SET_ALPHA         0x06
#define NJT_SET_ANTIALIAS     0x07
#define NJT_TRANSLATE         0x10
#define NJT_ROTATE            0x11
#define NJT_SCALE             0x12
#define NJT_SET_CTM           0x13
#define NJT_CLIP_RECT         0x20
#define NJT_RESET_CLIP        0x22
#define NJT_FILL_RECT         0x30
#define NJT_FILL_RECT_COLOR   0x31
#define NJT_FILL_PATH         0x32
#define NJT_FILL_ROUNDED_RECT 0x33
#define NJT_CLEAR_RECT        0x34
#define NJT_STROKE_RECT       0x40
#define NJT_STROKE_PATH       0x41
#define NJT_DRAW_LINE         0x42
#define NJT_BEGIN_TRANSPARENCY 0x80
#define NJT_END_TRANSPARENCY  0x81
#define NJT_FRAME_BEGIN       0xF0
#define NJT_FRAME_END         0xF1

/* Path commands */
#define NJT_PATH_MOVE  0
#define NJT_PATH_LINE  1
#define NJT_PATH_QUAD  2
#define NJT_PATH_CUBIC 3
#define NJT_PATH_CLOSE 4

static FILE* njt_file = nullptr;
static bool njt_initialized = false;

static inline FILE* njt_get() {
    if (!njt_initialized) {
        njt_initialized = true;
        /* Debug: leave a breadcrumb so we know this code was reached */
        FILE* dbg = fopen("/tmp/nutjob_tap_debug.txt", "a");
        if (dbg) { fprintf(dbg, "njt_get() called from pid %d\n", getpid()); fclose(dbg); }
        /* Try env var first, fall back to well-known pipe path.
         * The hardcoded fallback is needed because WebKit's GPU/web processes
         * don't inherit the parent process's environment on macOS. */
        const char* path = getenv("NUTJOB_TAP_PIPE");
        if (!path) path = "/tmp/nutjob_pipe";
        /* Only open if the pipe/file actually exists */
        if (access(path, F_OK) == 0) {
            njt_file = fopen(path, "wb");
            if (njt_file) {
                setvbuf(njt_file, NULL, _IONBF, 0); /* unbuffered */
                /* Write frame begin */
                uint8_t cmd = NJT_FRAME_BEGIN;
                fwrite(&cmd, 1, 1, njt_file);
                uint16_t w = 800, h = 600;
                fwrite(&w, 2, 1, njt_file);
                fwrite(&h, 2, 1, njt_file);
            }
        }
    }
    return njt_file;
}

static inline bool njt_active() { return njt_get() != nullptr; }

/* Writers */
static inline void njt_u8(uint8_t v) { fwrite(&v, 1, 1, njt_file); }
static inline void njt_u16(uint16_t v) { fwrite(&v, 2, 1, njt_file); }
static inline void njt_u32(uint32_t v) { fwrite(&v, 4, 1, njt_file); }
static inline void njt_f32(float v) { fwrite(&v, 4, 1, njt_file); }

static inline uint32_t njt_color(float r, float g, float b, float a) {
    uint8_t ri = (uint8_t)(r * 255);
    uint8_t gi = (uint8_t)(g * 255);
    uint8_t bi = (uint8_t)(b * 255);
    uint8_t ai = (uint8_t)(a * 255);
    return ((uint32_t)ai << 24) | ((uint32_t)ri << 16) | ((uint32_t)gi << 8) | bi;
}

/* Convert a WebCore Color to packed ARGB using the inline SRGBA<uint8_t> path */
#ifdef __cplusplus
namespace NutjobTapDetail {
    template<typename ColorT>
    static inline uint32_t colorToARGB(const ColorT& color) {
        auto srgba = color.template toColorTypeLossy<WebCore::SRGBA<uint8_t>>();
        auto resolved = srgba.resolved();
        return ((uint32_t)resolved.alpha << 24) | ((uint32_t)resolved.red << 16) |
               ((uint32_t)resolved.green << 8) | (uint32_t)resolved.blue;
    }
}
#define NJT_ARGB(color) NutjobTapDetail::colorToARGB(color)
#endif

/* Command emitters */
static inline void njt_save() { if (!njt_active()) return; njt_u8(NJT_SAVE); }
static inline void njt_restore() { if (!njt_active()) return; njt_u8(NJT_RESTORE); }

static inline void njt_fill_rect_color(float x, float y, float w, float h, uint32_t argb) {
    if (!njt_active()) return;
    njt_u8(NJT_FILL_RECT_COLOR);
    njt_f32(x); njt_f32(y); njt_f32(w); njt_f32(h);
    njt_u32(argb);
}

static inline void njt_fill_rect(float x, float y, float w, float h) {
    if (!njt_active()) return;
    njt_u8(NJT_FILL_RECT);
    njt_f32(x); njt_f32(y); njt_f32(w); njt_f32(h);
}

static inline void njt_stroke_rect(float x, float y, float w, float h) {
    if (!njt_active()) return;
    njt_u8(NJT_STROKE_RECT);
    njt_f32(x); njt_f32(y); njt_f32(w); njt_f32(h);
}

static inline void njt_clear_rect(float x, float y, float w, float h) {
    if (!njt_active()) return;
    njt_u8(NJT_CLEAR_RECT);
    njt_f32(x); njt_f32(y); njt_f32(w); njt_f32(h);
}

static inline void njt_draw_line(float x0, float y0, float x1, float y1) {
    if (!njt_active()) return;
    njt_u8(NJT_DRAW_LINE);
    njt_f32(x0); njt_f32(y0); njt_f32(x1); njt_f32(y1);
}

static inline void njt_set_fill_color(uint32_t argb) {
    if (!njt_active()) return;
    njt_u8(NJT_SET_FILL_COLOR);
    njt_u32(argb);
}

static inline void njt_set_stroke_color(uint32_t argb) {
    if (!njt_active()) return;
    njt_u8(NJT_SET_STROKE_COLOR);
    njt_u32(argb);
}

static inline void njt_set_line_width(float w) {
    if (!njt_active()) return;
    njt_u8(NJT_SET_LINE_WIDTH);
    njt_f32(w);
}

static inline void njt_set_alpha(float a) {
    if (!njt_active()) return;
    njt_u8(NJT_SET_ALPHA);
    njt_f32(a);
}

static inline void njt_translate(float x, float y) {
    if (!njt_active()) return;
    njt_u8(NJT_TRANSLATE);
    njt_f32(x); njt_f32(y);
}

static inline void njt_rotate(float r) {
    if (!njt_active()) return;
    njt_u8(NJT_ROTATE);
    njt_f32(r);
}

static inline void njt_scale(float sx, float sy) {
    if (!njt_active()) return;
    njt_u8(NJT_SCALE);
    njt_f32(sx); njt_f32(sy);
}

static inline void njt_clip_rect(float x, float y, float w, float h) {
    if (!njt_active()) return;
    njt_u8(NJT_CLIP_RECT);
    njt_f32(x); njt_f32(y); njt_f32(w); njt_f32(h);
}

static inline void njt_reset_clip() {
    if (!njt_active()) return;
    njt_u8(NJT_RESET_CLIP);
}

static inline void njt_begin_transparency(float opacity) {
    if (!njt_active()) return;
    njt_u8(NJT_BEGIN_TRANSPARENCY);
    njt_f32(opacity);
}

static inline void njt_end_transparency() {
    if (!njt_active()) return;
    njt_u8(NJT_END_TRANSPARENCY);
}

static inline void njt_fill_rounded_rect(float x, float y, float w, float h, float r) {
    if (!njt_active()) return;
    njt_u8(NJT_FILL_ROUNDED_RECT);
    njt_f32(x); njt_f32(y); njt_f32(w); njt_f32(h);
    njt_f32(r);
}

static inline void njt_frame_end() {
    if (!njt_active()) return;
    njt_u8(NJT_FRAME_END);
    fflush(njt_file);
}

#pragma clang diagnostic pop

#endif /* WEBKIT_NUTJOB_TAP_H */
