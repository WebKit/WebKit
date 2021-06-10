/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDMainViewController.h"

#import <AVFoundation/AVFoundation.h>

#import "sdk/objc/base/RTCLogging.h"
#import "sdk/objc/components/audio/RTCAudioSession.h"
#import "sdk/objc/components/audio/RTCAudioSessionConfiguration.h"
#import "sdk/objc/helpers/RTCDispatcher.h"

#import "ARDAppClient.h"
#import "ARDMainView.h"
#import "ARDSettingsModel.h"
#import "ARDSettingsViewController.h"
#import "ARDVideoCallViewController.h"

static NSString *const barButtonImageString = @"ic_settings_black_24dp.png";

// Launch argument to be passed to indicate that the app should start loopback immediatly
static NSString *const loopbackLaunchProcessArgument = @"loopback";

@interface ARDMainViewController () <ARDMainViewDelegate,
                                     ARDVideoCallViewControllerDelegate,
                                     RTC_OBJC_TYPE (RTCAudioSessionDelegate)>
@property(nonatomic, strong) ARDMainView *mainView;
@property(nonatomic, strong) AVAudioPlayer *audioPlayer;
@end

@implementation ARDMainViewController {
  BOOL _useManualAudio;
}

@synthesize mainView = _mainView;
@synthesize audioPlayer = _audioPlayer;

- (void)viewDidLoad {
  [super viewDidLoad];
  if ([[[NSProcessInfo processInfo] arguments] containsObject:loopbackLaunchProcessArgument]) {
    [self mainView:nil didInputRoom:@"" isLoopback:YES];
  }
}

- (void)loadView {
  self.title = @"AppRTC Mobile";
  _mainView = [[ARDMainView alloc] initWithFrame:CGRectZero];
  _mainView.delegate = self;
  self.view = _mainView;
  [self addSettingsBarButton];

  RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *webRTCConfig =
      [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) webRTCConfiguration];
  webRTCConfig.categoryOptions = webRTCConfig.categoryOptions |
      AVAudioSessionCategoryOptionDefaultToSpeaker;
  [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) setWebRTCConfiguration:webRTCConfig];

  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session addDelegate:self];

  [self configureAudioSession];
  [self setupAudioPlayer];
}

- (void)addSettingsBarButton {
  UIBarButtonItem *settingsButton =
      [[UIBarButtonItem alloc] initWithImage:[UIImage imageNamed:barButtonImageString]
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(showSettings:)];
  self.navigationItem.rightBarButtonItem = settingsButton;
}

+ (NSString *)loopbackRoomString {
  NSString *loopbackRoomString =
      [[NSUUID UUID].UUIDString stringByReplacingOccurrencesOfString:@"-" withString:@""];
  return loopbackRoomString;
}

#pragma mark - ARDMainViewDelegate

- (void)mainView:(ARDMainView *)mainView didInputRoom:(NSString *)room isLoopback:(BOOL)isLoopback {
  if (!room.length) {
    if (isLoopback) {
      // If this is a loopback call, allow a generated room name.
      room = [[self class] loopbackRoomString];
    } else {
      [self showAlertWithMessage:@"Missing room name."];
      return;
    }
  }
  // Trim whitespaces.
  NSCharacterSet *whitespaceSet = [NSCharacterSet whitespaceCharacterSet];
  NSString *trimmedRoom = [room stringByTrimmingCharactersInSet:whitespaceSet];

  // Check that room name is valid.
  NSError *error = nil;
  NSRegularExpressionOptions options = NSRegularExpressionCaseInsensitive;
  NSRegularExpression *regex =
      [NSRegularExpression regularExpressionWithPattern:@"\\w+"
                                                options:options
                                                  error:&error];
  if (error) {
    [self showAlertWithMessage:error.localizedDescription];
    return;
  }
  NSRange matchRange =
      [regex rangeOfFirstMatchInString:trimmedRoom
                               options:0
                                 range:NSMakeRange(0, trimmedRoom.length)];
  if (matchRange.location == NSNotFound ||
      matchRange.length != trimmedRoom.length) {
    [self showAlertWithMessage:@"Invalid room name."];
    return;
  }

  ARDSettingsModel *settingsModel = [[ARDSettingsModel alloc] init];

  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  session.useManualAudio = [settingsModel currentUseManualAudioConfigSettingFromStore];
  session.isAudioEnabled = NO;

  // Kick off the video call.
  ARDVideoCallViewController *videoCallViewController =
      [[ARDVideoCallViewController alloc] initForRoom:trimmedRoom
                                           isLoopback:isLoopback
                                             delegate:self];
  videoCallViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  videoCallViewController.modalPresentationStyle = UIModalPresentationFullScreen;
  [self presentViewController:videoCallViewController
                     animated:YES
                   completion:nil];
}

- (void)mainViewDidToggleAudioLoop:(ARDMainView *)mainView {
  if (mainView.isAudioLoopPlaying) {
    [_audioPlayer stop];
  } else {
    [_audioPlayer play];
  }
  mainView.isAudioLoopPlaying = _audioPlayer.playing;
}

#pragma mark - ARDVideoCallViewControllerDelegate

- (void)viewControllerDidFinish:(ARDVideoCallViewController *)viewController {
  if (![viewController isBeingDismissed]) {
    RTCLog(@"Dismissing VC");
    [self dismissViewControllerAnimated:YES completion:^{
      [self restartAudioPlayerIfNeeded];
    }];
  }
  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  session.isAudioEnabled = NO;
}

#pragma mark - RTC_OBJC_TYPE(RTCAudioSessionDelegate)

- (void)audioSessionDidStartPlayOrRecord:(RTC_OBJC_TYPE(RTCAudioSession) *)session {
  // Stop playback on main queue and then configure WebRTC.
  [RTC_OBJC_TYPE(RTCDispatcher)
      dispatchAsyncOnType:RTCDispatcherTypeMain
                    block:^{
                      if (self.mainView.isAudioLoopPlaying) {
                        RTCLog(@"Stopping audio loop due to WebRTC start.");
                        [self.audioPlayer stop];
                      }
                      RTCLog(@"Setting isAudioEnabled to YES.");
                      session.isAudioEnabled = YES;
                    }];
}

- (void)audioSessionDidStopPlayOrRecord:(RTC_OBJC_TYPE(RTCAudioSession) *)session {
  // WebRTC is done with the audio session. Restart playback.
  [RTC_OBJC_TYPE(RTCDispatcher) dispatchAsyncOnType:RTCDispatcherTypeMain
                                              block:^{
                                                RTCLog(@"audioSessionDidStopPlayOrRecord");
                                                [self restartAudioPlayerIfNeeded];
                                              }];
}

#pragma mark - Private
- (void)showSettings:(id)sender {
  ARDSettingsViewController *settingsController =
      [[ARDSettingsViewController alloc] initWithStyle:UITableViewStyleGrouped
                                         settingsModel:[[ARDSettingsModel alloc] init]];

  UINavigationController *navigationController =
      [[UINavigationController alloc] initWithRootViewController:settingsController];
  [self presentViewControllerAsModal:navigationController];
}

- (void)presentViewControllerAsModal:(UIViewController *)viewController {
  [self presentViewController:viewController animated:YES completion:nil];
}

- (void)configureAudioSession {
  RTC_OBJC_TYPE(RTCAudioSessionConfiguration) *configuration =
      [[RTC_OBJC_TYPE(RTCAudioSessionConfiguration) alloc] init];
  configuration.category = AVAudioSessionCategoryAmbient;
  configuration.categoryOptions = AVAudioSessionCategoryOptionDuckOthers;
  configuration.mode = AVAudioSessionModeDefault;

  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session lockForConfiguration];
  BOOL hasSucceeded = NO;
  NSError *error = nil;
  if (session.isActive) {
    hasSucceeded = [session setConfiguration:configuration error:&error];
  } else {
    hasSucceeded = [session setConfiguration:configuration
                                      active:YES
                                       error:&error];
  }
  if (!hasSucceeded) {
    RTCLogError(@"Error setting configuration: %@", error.localizedDescription);
  }
  [session unlockForConfiguration];
}

- (void)setupAudioPlayer {
  NSString *audioFilePath =
      [[NSBundle mainBundle] pathForResource:@"mozart" ofType:@"mp3"];
  NSURL *audioFileURL = [NSURL URLWithString:audioFilePath];
  _audioPlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:audioFileURL
                                                        error:nil];
  _audioPlayer.numberOfLoops = -1;
  _audioPlayer.volume = 1.0;
  [_audioPlayer prepareToPlay];
}

- (void)restartAudioPlayerIfNeeded {
  [self configureAudioSession];
  if (_mainView.isAudioLoopPlaying && !self.presentedViewController) {
    RTCLog(@"Starting audio loop due to WebRTC end.");
    [_audioPlayer play];
  }
}

- (void)showAlertWithMessage:(NSString*)message {
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction *defaultAction = [UIAlertAction actionWithTitle:@"OK"
                                                          style:UIAlertActionStyleDefault
                                                        handler:^(UIAlertAction *action){
                                                        }];

  [alert addAction:defaultAction];
  [self presentViewController:alert animated:YES completion:nil];
}

@end
