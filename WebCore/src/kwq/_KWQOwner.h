#import <Cocoa/Cocoa.h>
#import <WCBackForwardList.h>

@interface _KWQOwner : NSApplication
{
    IBOutlet id window;
    IBOutlet id containerView;
    IBOutlet NSProgressIndicator *throbber;
    IBOutlet NSComboBox *urlBar;
    IBOutlet NSButton *backButton;
    IBOutlet NSButton *forwardButton;
    id <WCBackForwardList> backForwardList;
}

-(void)updateButtons;

@end
