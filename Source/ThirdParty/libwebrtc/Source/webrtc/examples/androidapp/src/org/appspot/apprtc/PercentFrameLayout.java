/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

/**
 * Simple container that confines the children to a subrectangle specified as percentage values of
 * the container size. The children are centered horizontally and vertically inside the confined
 * space.
 */
public class PercentFrameLayout extends ViewGroup {
  private int xPercent = 0;
  private int yPercent = 0;
  private int widthPercent = 100;
  private int heightPercent = 100;

  public PercentFrameLayout(Context context) {
    super(context);
  }

  public PercentFrameLayout(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public PercentFrameLayout(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
  }

  public void setPosition(int xPercent, int yPercent, int widthPercent, int heightPercent) {
    this.xPercent = xPercent;
    this.yPercent = yPercent;
    this.widthPercent = widthPercent;
    this.heightPercent = heightPercent;
  }

  @Override
  public boolean shouldDelayChildPressedState() {
    return false;
  }

  @Override
  protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    final int width = getDefaultSize(Integer.MAX_VALUE, widthMeasureSpec);
    final int height = getDefaultSize(Integer.MAX_VALUE, heightMeasureSpec);
    setMeasuredDimension(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
        MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));

    final int childWidthMeasureSpec =
        MeasureSpec.makeMeasureSpec(width * widthPercent / 100, MeasureSpec.AT_MOST);
    final int childHeightMeasureSpec =
        MeasureSpec.makeMeasureSpec(height * heightPercent / 100, MeasureSpec.AT_MOST);
    for (int i = 0; i < getChildCount(); ++i) {
      final View child = getChildAt(i);
      if (child.getVisibility() != GONE) {
        child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
      }
    }
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    final int width = right - left;
    final int height = bottom - top;
    // Sub-rectangle specified by percentage values.
    final int subWidth = width * widthPercent / 100;
    final int subHeight = height * heightPercent / 100;
    final int subLeft = left + width * xPercent / 100;
    final int subTop = top + height * yPercent / 100;

    for (int i = 0; i < getChildCount(); ++i) {
      final View child = getChildAt(i);
      if (child.getVisibility() != GONE) {
        final int childWidth = child.getMeasuredWidth();
        final int childHeight = child.getMeasuredHeight();
        // Center child both vertically and horizontally.
        final int childLeft = subLeft + (subWidth - childWidth) / 2;
        final int childTop = subTop + (subHeight - childHeight) / 2;
        child.layout(childLeft, childTop, childLeft + childWidth, childTop + childHeight);
      }
    }
  }
}
