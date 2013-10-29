/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "DefaultWebThemeEngine.h"

#include "public/Color.h"
#include "public/Rect.h"
#include "public/Size.h"
#include <algorithm>
#include <cairo/cairo.h>
#include <cmath>

const double BGColor1 = 0xF6 / 255.0;
const double BGColor2 = 0xDE / 255.0;
const double BorderColor = 0xA4 / 255.0;
const double BorderOnHoverColor = 0x7A / 255.0;
const double CheckColor = 0x66 / 255.0;
const double TextFieldDarkBorderColor = 0x9A / 255.0;
const double TextFieldLightBorderColor = 0xEE / 255.0;

const int MenuListBorder = 5;
const int MenuListArrowSize = 6;

const int InnerSpinButtonBorder = 3;
const int InnerSpinButtonArrowSize = 2;

const Nix::Color TapHighLightColor(0x66000000);

namespace Nix {

Color DefaultWebThemeEngine::activeSelectionBackgroundColor() const
{
    return Color::blue;
}

Color DefaultWebThemeEngine::activeSelectionForegroundColor() const
{
    return Color::white;
}

Color DefaultWebThemeEngine::inactiveSelectionBackgroundColor() const
{
    return Color(176, 176, 176);
}

Color DefaultWebThemeEngine::inactiveSelectionForegroundColor() const
{
    return Color::black;
}

Color DefaultWebThemeEngine::activeListBoxSelectionBackgroundColor() const
{
    return activeSelectionBackgroundColor();
}

Color DefaultWebThemeEngine::activeListBoxSelectionForegroundColor() const
{
    return activeSelectionForegroundColor();
}

Color DefaultWebThemeEngine::inactiveListBoxSelectionBackgroundColor() const
{
    return inactiveSelectionBackgroundColor();
}

Color DefaultWebThemeEngine::inactiveListBoxSelectionForegroundColor() const
{
    return inactiveSelectionForegroundColor();
}

Color DefaultWebThemeEngine::activeTextSearchHighlightColor() const
{
    return Color(255, 150, 50); // Orange.
}

Color DefaultWebThemeEngine::inactiveTextSearchHighlightColor() const
{
    return Color(255, 255, 0); // Yellow.
}

Color DefaultWebThemeEngine::focusRingColor() const
{
    return Color::black;
}

Color DefaultWebThemeEngine::tapHighlightColor() const
{
    return TapHighLightColor;
}

static void gradientFill(cairo_t* cairo, double yStart, double yLength, bool inverted = false)
{
    double gradStartColor = BGColor1;
    double gradEndColor = BGColor2;
    if (inverted)
        std::swap(gradStartColor, gradEndColor);

    cairo_pattern_t* gradient = cairo_pattern_create_linear(0, yStart, 0, yStart + yLength);
    cairo_pattern_add_color_stop_rgb(gradient, 0, gradStartColor, gradStartColor, gradStartColor);
    cairo_pattern_add_color_stop_rgb(gradient, 1, gradEndColor, gradEndColor, gradEndColor);
    cairo_set_source(cairo, gradient);
    cairo_fill(cairo);
    cairo_pattern_destroy(gradient);
}

static void setupBorder(cairo_t * cairo, ThemeEngine::State state)
{
    double borderColor = state == ThemeEngine::StateHover ? BorderOnHoverColor : BorderColor;
    cairo_set_source_rgb(cairo, borderColor, borderColor, borderColor);
    cairo_set_line_width(cairo, 1);
}

void DefaultWebThemeEngine::paintButton(Canvas* canvas, State state, const Rect& rect, const ButtonExtraParams&) const
{
    cairo_save(canvas);
    setupBorder(canvas, state);
    // Cairo uses a coordinate system not based on pixel coordinates, so
    // we need to add  0.5 to x and y coord or the line will stay between
    // two pixels instead of in the middle of a pixel.
    cairo_rectangle(canvas, rect.x + 0.5, rect.y + 0.5, rect.width, rect.height);
    cairo_stroke_preserve(canvas);

    gradientFill(canvas, rect.y, rect.height, state == StatePressed);
    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintTextField(Canvas* canvas, State, const Rect& rect) const
{
    cairo_save(canvas);

    const double lineWidth = 2;
    const double correction = lineWidth / 2.0;

    cairo_set_line_width(canvas, lineWidth);
    cairo_set_source_rgb(canvas, TextFieldDarkBorderColor, TextFieldDarkBorderColor, TextFieldDarkBorderColor);
    cairo_move_to(canvas, rect.x + correction, rect.y + correction + rect.height);
    cairo_rel_line_to(canvas, 0, -rect.height);
    cairo_rel_line_to(canvas, rect.width, 0);
    cairo_stroke(canvas);

    cairo_set_source_rgb(canvas, TextFieldLightBorderColor, TextFieldLightBorderColor, TextFieldLightBorderColor);
    cairo_move_to(canvas, rect.x + correction + rect.width, rect.y + correction);
    cairo_rel_line_to(canvas, 0, rect.height);
    cairo_rel_line_to(canvas, -rect.width, 0);
    cairo_stroke(canvas);

    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintTextArea(Canvas* canvas, State state, const Rect& rect) const
{
    paintTextField(canvas, state, rect);
}

Size DefaultWebThemeEngine::getCheckboxSize() const
{
    return Size(13, 13);
}

void DefaultWebThemeEngine::paintCheckbox(Canvas* canvas, State state, const Rect& rect, const ButtonExtraParams& param) const
{
    cairo_save(canvas);
    setupBorder(canvas, state);
    cairo_rectangle(canvas, rect.x + 0.5, rect.y + 0.5, rect.width, rect.height);
    cairo_stroke_preserve(canvas);

    gradientFill(canvas, rect.y, rect.height, state == StatePressed);

    if (param.checked) {
        const double border = 3;
        cairo_set_line_width(canvas, 2);
        cairo_set_source_rgb(canvas, CheckColor, CheckColor, CheckColor);
        cairo_move_to(canvas, rect.x + 0.5 + border, rect.y + 0.5 + rect.height - border);
        cairo_rel_line_to(canvas, rect.width - border * 2, -rect.height + border * 2);
        cairo_move_to(canvas, rect.x + 0.5 + border, rect.y + 0.5 + border);
        cairo_rel_line_to(canvas, rect.width - border * 2, rect.height - border * 2);
        cairo_stroke(canvas);
    }

    cairo_restore(canvas);
}

Size DefaultWebThemeEngine::getRadioSize() const
{
    return Size(13, 13);
}

void DefaultWebThemeEngine::paintRadio(Canvas* canvas, State state, const Rect& rect, const ButtonExtraParams& param) const
{
    cairo_save(canvas);
    setupBorder(canvas, state);
    cairo_arc(canvas, rect.x + rect.width / 2.0, rect.y + rect.height / 2.0, rect.width / 2.0, 0, 2 * M_PI);
    cairo_stroke_preserve(canvas);

    gradientFill(canvas, rect.y, rect.height);

    if (param.checked) {
        cairo_set_source_rgb(canvas, CheckColor, CheckColor, CheckColor);
        cairo_arc(canvas, rect.x + rect.width / 2.0, rect.y + rect.height / 2.0, rect.width / 4.0, 0, 2 * M_PI);
        cairo_fill(canvas);
    }
    cairo_restore(canvas);
}

void DefaultWebThemeEngine::getMenuListPadding(int& paddingTop, int& paddingLeft, int& paddingBottom, int& paddingRight) const
{
    paddingTop = MenuListBorder;
    paddingLeft = MenuListBorder;
    paddingBottom = MenuListBorder;
    paddingRight = 2 * MenuListBorder + MenuListArrowSize;
}

void DefaultWebThemeEngine::paintMenuList(Canvas* canvas, State state, const Rect& rect) const
{
    cairo_save(canvas);
    setupBorder(canvas, state);
    cairo_rectangle(canvas, rect.x + 0.5, rect.y + 0.5, rect.width, rect.height);
    cairo_stroke_preserve(canvas);

    gradientFill(canvas, rect.y, rect.height, state == StatePressed);

    cairo_move_to(canvas, rect.x + rect.width - MenuListArrowSize - MenuListBorder, rect.y + 1 + rect.height / 2 - MenuListArrowSize / 2);
    cairo_set_source_rgb(canvas, CheckColor, CheckColor, CheckColor);
    cairo_rel_line_to(canvas, MenuListArrowSize, 0);
    cairo_rel_line_to(canvas, -MenuListArrowSize / 2, MenuListArrowSize);
    cairo_close_path(canvas);
    cairo_fill(canvas);

    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintProgressBar(Canvas* canvas, State state, const Rect& rect, const ProgressBarExtraParams& params) const
{
    cairo_save(canvas);

    if (params.isDeterminate) {
        // Background
        setupBorder(canvas, state);
        cairo_rectangle(canvas, rect.x, rect.y, rect.width, rect.height);
        cairo_fill(canvas);
        // Progress
        cairo_rectangle(canvas, rect.x, rect.y, rect.width * params.position, rect.height);
        gradientFill(canvas, rect.y, rect.height);
    } else {
        cairo_rectangle(canvas, rect.x, rect.y, rect.width, rect.height);
        gradientFill(canvas, rect.y, rect.height, true);
    }

    cairo_restore(canvas);
}

static const int progressAnimationFrames = 10;
static const double progressAnimationInterval = 0.125;

double DefaultWebThemeEngine::getAnimationRepeatIntervalForProgressBar() const
{
    return progressAnimationInterval;
}

double DefaultWebThemeEngine::getAnimationDurationForProgressBar() const
{
    return progressAnimationInterval * progressAnimationFrames * 2; // "2" for back and forth.
}

void DefaultWebThemeEngine::getInnerSpinButtonPadding(int& paddingTop, int& paddingLeft, int& paddingBottom, int& paddingRight) const
{
    paddingTop = InnerSpinButtonBorder;
    paddingLeft = InnerSpinButtonBorder;
    paddingBottom = InnerSpinButtonBorder;
    paddingRight = 2 * InnerSpinButtonBorder + InnerSpinButtonArrowSize;
}

void DefaultWebThemeEngine::paintInnerSpinButton(Canvas* canvas, State state, const Rect& rect, const InnerSpinButtonExtraParams& param) const
{
    double rectHalfHeight = rect.height / 2;

    cairo_save(canvas);
    setupBorder(canvas, state);
    cairo_rectangle(canvas, rect.x + 0.5, rect.y + 1.5, rect.width, rectHalfHeight);
    cairo_stroke_preserve(canvas);

    gradientFill(canvas, rect.y, rectHalfHeight, state == StatePressed && param.spinUp);

    setupBorder(canvas, state);
    cairo_rectangle(canvas, rect.x + 0.5, rect.y + 0.5 + rectHalfHeight, rect.width, rectHalfHeight);
    cairo_stroke_preserve(canvas);

    gradientFill(canvas, rect.y + rectHalfHeight, rectHalfHeight, state == StatePressed && !param.spinUp);

    cairo_move_to(canvas, rect.x + rect.width - InnerSpinButtonArrowSize - InnerSpinButtonBorder * 2, rect.y + rectHalfHeight + rect.height / 4 - InnerSpinButtonArrowSize + 1.5);
    cairo_set_source_rgb(canvas, CheckColor, CheckColor, CheckColor);
    cairo_rel_line_to(canvas, MenuListArrowSize, 0);
    cairo_rel_line_to(canvas, -MenuListArrowSize / 2, MenuListArrowSize);
    cairo_close_path(canvas);

    cairo_move_to(canvas, rect.x + rect.width - InnerSpinButtonArrowSize - InnerSpinButtonBorder * 2, rect.y + rect.height / 4 + InnerSpinButtonArrowSize);
    cairo_set_source_rgb(canvas, CheckColor, CheckColor, CheckColor);
    cairo_rel_line_to(canvas, MenuListArrowSize, 0);
    cairo_rel_line_to(canvas, -MenuListArrowSize / 2, -MenuListArrowSize);
    cairo_close_path(canvas);

    cairo_fill(canvas);
}

void DefaultWebThemeEngine::paintMeter(Canvas* canvas, State state, const Rect& rect, const MeterExtraParams& params) const
{
    cairo_save(canvas);

    // Background.
    setupBorder(canvas, state);
    cairo_rectangle(canvas, rect.x, rect.y, rect.width, rect.height);
    cairo_fill(canvas);
    // Meter.
    cairo_rectangle(canvas, rect.x, rect.y, rect.width * params.valueRatio(), rect.height);
    gradientFill(canvas, rect.y, rect.height);

    cairo_restore(canvas);
}

const int SliderTrackHeight = 6;

void DefaultWebThemeEngine::paintSliderTrack(Canvas* canvas, State, const Rect& rect) const
{
    cairo_save(canvas);
    cairo_rectangle(canvas, rect.x, rect.y + (rect.height - SliderTrackHeight) / 2.0, rect.width, SliderTrackHeight);
    gradientFill(canvas, rect.y, rect.height, true);
    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintSliderThumb(Canvas* canvas, State state, const Rect& rect) const
{
    cairo_save(canvas);
    setupBorder(canvas, state);
    cairo_rectangle(canvas, rect.x, rect.y, rect.width, rect.height);
    cairo_stroke_preserve(canvas);
    gradientFill(canvas, rect.y, rect.height, state == StatePressed);
    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintMediaPlayButton(Canvas* canvas, MediaPlayerState state, const Rect& rect) const
{
    cairo_save(canvas);
    cairo_set_source_rgb(canvas, 1, 1, 1);
    if (state == StatePaused) {
        cairo_move_to(canvas, rect.x + (rect.width * 0.3), rect.y + (rect.height * 0.25));
        cairo_line_to(canvas, rect.x + (rect.width * 0.75), rect.y + (rect.height * 0.5));
        cairo_line_to(canvas, rect.x + (rect.width * 0.3), rect.y + (rect.height * 0.75));
        cairo_close_path(canvas);
        cairo_fill(canvas);
    } else { // If state is StatePlaying.
        cairo_rectangle(canvas, rect.x + (rect.width * 0.3), rect.y + (rect.height * 0.25), (rect.width * 0.15), (rect.height * 0.5));
        cairo_fill(canvas);
        cairo_rectangle(canvas, rect.x + (rect.width * 0.55), rect.y + (rect.height * 0.25), (rect.width * 0.15), (rect.height * 0.5));
        cairo_fill(canvas);
    }
    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintMediaMuteButton(Canvas* canvas, MediaPlayerState state, const Rect& rect) const
{
    cairo_save(canvas);
    if (state == StateMuted) {
        cairo_set_line_width(canvas, 1);
        cairo_set_source_rgb(canvas, 139, 0, 0);

        cairo_move_to(canvas, rect.x + (rect.width * 0.59), rect.y + (rect.height * 0.67));
        cairo_line_to(canvas, rect.x + (rect.width * 0.87), rect.y + (rect.height * 0.34));
        cairo_stroke(canvas);

        cairo_move_to(canvas, rect.x + (rect.width * 0.87), rect.y + (rect.height * 0.67));
        cairo_line_to(canvas, rect.x + (rect.width * 0.59), rect.y + (rect.height * 0.34));
        cairo_stroke(canvas);
    } else { // If state is StateNotMuted
        cairo_set_line_width(canvas, 5);
        cairo_set_source_rgb(canvas, 1, 1, 1);

        cairo_arc(canvas, rect.x + (rect.width * 0.3), rect.y + (rect.height * 0.5), rect.width * 0.34, 11 * (M_PI / 6), M_PI / 6);
        cairo_fill(canvas);
        cairo_arc(canvas, rect.x + (rect.width * 0.3), rect.y + (rect.height * 0.5), rect.width * 0.45, 11 * (M_PI / 6), M_PI / 6);
        cairo_fill(canvas);
        cairo_arc(canvas, rect.x + (rect.width * 0.3), rect.y + (rect.height * 0.5), rect.width * 0.57, 11 * (M_PI / 6), M_PI / 6);
        cairo_fill(canvas);
    }

    // Speaker
    cairo_move_to(canvas, rect.x + (rect.width * 0.52), rect.y + (rect.height * 0.18));
    cairo_line_to(canvas, rect.x + (rect.width * 0.29), rect.y + (rect.height * 0.38));
    cairo_line_to(canvas, rect.x + (rect.width * 0.08), rect.y + (rect.height * 0.38));
    cairo_line_to(canvas, rect.x + (rect.width * 0.08), rect.y + (rect.height * 0.62));
    cairo_line_to(canvas, rect.x + (rect.width * 0.29), rect.y + (rect.height * 0.62));
    cairo_line_to(canvas, rect.x + (rect.width * 0.52), rect.y + (rect.height * 0.82));
    cairo_close_path(canvas);
    cairo_fill(canvas);

    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintMediaSeekBackButton(Canvas* canvas, const Rect& rect) const
{
    cairo_save(canvas);
    cairo_set_source_rgb(canvas, 1, 1, 1);
    cairo_move_to(canvas, rect.x + (rect.width * 0.90), rect.y + (rect.height * 0.70));
    cairo_line_to(canvas, rect.x + (rect.width * 0.58), rect.y + (rect.height * 0.51));
    cairo_line_to(canvas, rect.x + (rect.width * 0.58), rect.y + (rect.height * 0.70));

    cairo_line_to(canvas, rect.x + (rect.width * 0.23), rect.y + (rect.height * 0.51));
    cairo_line_to(canvas, rect.x + (rect.width * 0.23), rect.y + (rect.height * 0.70));
    cairo_line_to(canvas, rect.x + (rect.width * 0.11), rect.y + (rect.height * 0.70));
    cairo_line_to(canvas, rect.x + (rect.width * 0.11), rect.y + (rect.height * 0.29));
    cairo_line_to(canvas, rect.x + (rect.width * 0.23), rect.y + (rect.height * 0.29));
    cairo_line_to(canvas, rect.x + (rect.width * 0.23), rect.y + (rect.height * 0.48));

    cairo_line_to(canvas, rect.x + (rect.width * 0.58), rect.y + (rect.height * 0.29));
    cairo_line_to(canvas, rect.x + (rect.width * 0.58), rect.y + (rect.height * 0.48));
    cairo_line_to(canvas, rect.x + (rect.width * 0.90), rect.y + (rect.height * 0.29));
    cairo_close_path(canvas);
    cairo_fill(canvas);

    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintMediaSeekForwardButton(Canvas* canvas, const Rect& rect) const
{
    cairo_save(canvas);
    cairo_set_source_rgb(canvas, 1, 1, 1);
    cairo_move_to(canvas, rect.x + (rect.width * 0.10), rect.y + (rect.height * 0.70));
    cairo_line_to(canvas, rect.x + (rect.width * 0.42), rect.y + (rect.height * 0.51));
    cairo_line_to(canvas, rect.x + (rect.width * 0.42), rect.y + (rect.height * 0.70));

    cairo_line_to(canvas, rect.x + (rect.width * 0.77), rect.y + (rect.height * 0.51));
    cairo_line_to(canvas, rect.x + (rect.width * 0.77), rect.y + (rect.height * 0.70));
    cairo_line_to(canvas, rect.x + (rect.width * 0.89), rect.y + (rect.height * 0.70));
    cairo_line_to(canvas, rect.x + (rect.width * 0.89), rect.y + (rect.height * 0.29));
    cairo_line_to(canvas, rect.x + (rect.width * 0.77), rect.y + (rect.height * 0.29));
    cairo_line_to(canvas, rect.x + (rect.width * 0.77), rect.y + (rect.height * 0.48));

    cairo_line_to(canvas, rect.x + (rect.width * 0.42), rect.y + (rect.height * 0.29));
    cairo_line_to(canvas, rect.x + (rect.width * 0.42), rect.y + (rect.height * 0.48));
    cairo_line_to(canvas, rect.x + (rect.width * 0.10), rect.y + (rect.height * 0.29));
    cairo_close_path(canvas);
    cairo_fill(canvas);

    cairo_restore(canvas);
}

void DefaultWebThemeEngine::paintMediaRewindButton(Canvas* canvas, const Rect& rect) const
{
    cairo_save(canvas);
    cairo_set_source_rgb(canvas, 1, 1, 1);
    cairo_move_to(canvas, rect.x + (rect.width * 0.84), rect.y + (rect.height * 0.70));
    cairo_line_to(canvas, rect.x + (rect.width * 0.52), rect.y + (rect.height * 0.51));
    cairo_line_to(canvas, rect.x + (rect.width * 0.52), rect.y + (rect.height * 0.70));
    cairo_line_to(canvas, rect.x + (rect.width * 0.17), rect.y + (rect.height * 0.50));
    cairo_line_to(canvas, rect.x + (rect.width * 0.52), rect.y + (rect.height * 0.29));
    cairo_line_to(canvas, rect.x + (rect.width * 0.52), rect.y + (rect.height * 0.48));
    cairo_line_to(canvas, rect.x + (rect.width * 0.84), rect.y + (rect.height * 0.29));
    cairo_close_path(canvas);
    cairo_fill(canvas);

    cairo_restore(canvas);
}

}
