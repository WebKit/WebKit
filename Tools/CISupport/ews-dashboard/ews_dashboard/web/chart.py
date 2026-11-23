# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Geometry for the trend chart, computed here and rendered as inline SVG.

There is no JavaScript on any page of this dashboard. A month of daily points does not need a
charting library, and one `<title>` per point gets hover text from the browser itself, so the chart
costs no CDN dependency, no build step and no vendored bundle. If this ever needs zooming or a
crosshair, uPlot is the escape hatch — vendor it then, not now.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Optional

from ews_dashboard.analysis import trend

# Every coordinate here is a percentage of the svg's own box, which the template writes out with a
# trailing '%'. The svg carries no viewBox, so the box is whatever the card gives it and nothing is
# scaled: the plot fills the card on both axes, and a font size in the stylesheet is the size it
# renders at. Offsets that must not grow with the card — the gap between a gridline and its label, a
# label's baseline — are px dx and dy in the template.
PLOT_LEFT = 6.5
PLOT_RIGHT = 98.5
PLOT_TOP = 5.0
PLOT_BOTTOM = 87.0
DAY_LABEL_Y = 96.0

GRIDLINE_COUNT = 4
SMALLEST_Y_MAX_PCT = 10
DAY_LABELS_WANTED = 8

PLOT_WIDTH = PLOT_RIGHT - PLOT_LEFT
PLOT_HEIGHT = PLOT_BOTTOM - PLOT_TOP


@dataclass(frozen=True)
class Dot:
    x: float
    y: float
    title: str


@dataclass(frozen=True)
class Gridline:
    y: float
    label: str


@dataclass(frozen=True)
class DayLabel:
    x: float
    text: str


@dataclass(frozen=True)
class DeploymentMark:
    x: float
    label: str
    title: str


@dataclass(frozen=True)
class Segment:
    x1: float
    y1: float
    x2: float
    y2: float


@dataclass(frozen=True)
class Chart:
    y_max: int
    dots: tuple
    rolling_segments: tuple
    gridlines: tuple
    day_labels: tuple
    deployments: tuple

    plot_left: float = PLOT_LEFT
    plot_right: float = PLOT_RIGHT
    plot_top: float = PLOT_TOP
    plot_bottom: float = PLOT_BOTTOM
    day_label_y: float = DAY_LABEL_Y

    @property
    def empty(self) -> bool:
        return not self.dots


def _y(percent: float, y_max: int) -> float:
    return round(PLOT_TOP + PLOT_HEIGHT * (1 - percent / y_max), 2)


def _x_of_fraction(fraction: float) -> float:
    return round(PLOT_LEFT + PLOT_WIDTH * fraction, 2)


def _y_max(points: list) -> int:
    observed = [
        percent
        for point in points
        for percent in (point.daily_pct, point.rolling_pct)
        if percent is not None
    ]
    highest = max(observed) if observed else 0
    return max(SMALLEST_Y_MAX_PCT, 10 * math.ceil(highest / 10))


def _dot_title(point: trend.Point) -> str:
    classifiable = point.counts.classifiable
    blamed = point.counts.partial_fp + point.counts.false_red
    return (f'{point.day.isoformat()}: {point.daily_pct}% — '
            f'{blamed} of {classifiable} builds blamed an author for noise')


def _rolling_segments(points: list, y_max: int) -> tuple:
    """One line per pair of consecutive averaged days, so a day with nothing to average reads as a gap.

    A `<path>` would take fewer elements but its `d` cannot hold percentages, which is what keeps the
    plot unscaled.
    """
    segments = []
    previous = None
    for index, point in enumerate(points):
        if point.rolling_pct is None:
            previous = None
            continue
        here = (_x_of_day(index, len(points)), _y(point.rolling_pct, y_max))
        if previous is not None:
            segments.append(Segment(previous[0], previous[1], here[0], here[1]))
        previous = here
    return tuple(segments)


def _x_of_day(index: int, count: int) -> float:
    return _x_of_fraction((index + 0.5) / count)


def _label_stride(count: int) -> int:
    """Days between dated labels, so a 90-day span reads as densely as a 14-day one."""
    return max(1, math.ceil(count / DAY_LABELS_WANTED))


def _deployment_marks(points: list, deployments: list) -> tuple:
    first = trend.day_bounds(points[0].day)[0]
    last = trend.day_bounds(points[-1].day)[1]
    span = last - first
    marks = []
    for deployment in deployments:
        at = int(deployment.at.timestamp())
        marks.append(DeploymentMark(
            x=_x_of_fraction((at - first) / span),
            label=deployment.label,
            title=f'{deployment.at.strftime("%Y-%m-%d %H:%M")} UTC — {deployment.detail}',
        ))
    return tuple(marks)


def of_trend(points: list, deployments: Optional[list] = None) -> Chart:
    if not points:
        return Chart(SMALLEST_Y_MAX_PCT, (), (), (), (), ())

    y_max = _y_max(points)
    count = len(points)
    return Chart(
        y_max=y_max,
        dots=tuple(
            Dot(_x_of_day(index, count), _y(point.daily_pct, y_max), _dot_title(point))
            for index, point in enumerate(points)
            if point.daily_pct is not None
        ),
        rolling_segments=_rolling_segments(points, y_max),
        gridlines=tuple(
            Gridline(_y(y_max * step / GRIDLINE_COUNT, y_max), f'{round(y_max * step / GRIDLINE_COUNT)}%')
            for step in range(GRIDLINE_COUNT + 1)
        ),
        day_labels=tuple(
            DayLabel(_x_of_day(index, count), points[index].day.strftime('%b %d'))
            for index in reversed(range(count - 1, -1, -_label_stride(count)))
        ),
        deployments=_deployment_marks(points, deployments or []),
    )
