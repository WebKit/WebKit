/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "UIImage+ARDUtilities.h"

@implementation UIImage (ARDUtilities)

+ (UIImage *)imageForName:(NSString *)name color:(UIColor *)color {
  UIImage *image = [UIImage imageNamed:name];
  if (!image) {
    return nil;
  }
  UIGraphicsBeginImageContextWithOptions(image.size, NO, 0.0f);
  [color setFill];
  CGRect bounds = CGRectMake(0, 0, image.size.width, image.size.height);
  UIRectFill(bounds);
  [image drawInRect:bounds blendMode:kCGBlendModeDestinationIn alpha:1.0f];
  UIImage *coloredImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  return coloredImage;
}

@end
