#import <Cocoa/Cocoa.h>


class QWidget;

@interface KWQView : NSView
{
    QWidget *widget;
    bool isFlipped;
}
- initWithFrame: (NSRect)r widget: (QWidget *)w; 
- (void)setIsFlipped: (bool)flag;
@end

@interface KWQNSButton : NSButton
{
    QWidget *widget;
}
- initWithFrame: (NSRect)r widget: (QWidget *)w; 
@end

@interface KWQNSComboBox : NSComboBox
{
    QWidget *widget;
}
- initWithFrame: (NSRect)r widget: (QWidget *)w; 
@end

@interface KWQNSScrollView : NSScrollView
{
    QWidget *widget;
}
- initWithFrame: (NSRect)r widget: (QWidget *)w; 
@end

