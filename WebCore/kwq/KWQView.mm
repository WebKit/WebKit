#import "KWQView.h"

#import <qwidget.h>

@implementation KWQView

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    widget = w;
    isFlipped = YES;
}


// This should eventually be removed.
- (void)drawRect:(NSRect)rect {
    widget->paint((void *)0);
}

- (void)setIsFlipped: (bool)flag
{
    isFlipped = flag;
}


- (BOOL)isFlipped 
{
    return isFlipped;
}

@end


@implementation KWQNSButton

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    widget = w;
}

@end


@implementation KWQNSComboBox

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    widget = w;
}

@end
