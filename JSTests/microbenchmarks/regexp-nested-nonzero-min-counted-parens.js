//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// This checks that we didn't regress the performance of regular expressions with nested, counted parenthesis
// where the minimum count is not 0.

/$($($($($($($($($($($($($($($($($($($($(${-2,16}+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+/.exec("a");

/$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:$(?:${-2,16}+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+/.exec("a");

/$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=$(?=${-2,16}+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+)+/.exec("a");



