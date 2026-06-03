#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <AVFoundation/AVFoundation.h>
#import <MediaPlayer/MediaPlayer.h>
#include <cmath>
#include <cstdlib>
#include <string>

static bool g_filePickerActive = false;
static bool g_fileImported = false;
static int g_volumeAction = 0;
static float g_lastVolume = -1.0f;
static float g_restoreVolume = -1.0f;
static bool g_volumeListening = false;
static UISlider* g_volumeSlider = nil;

@interface RetroReadDocumentPicker : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, copy) NSString* documentsPath;
@end

@implementation RetroReadDocumentPicker

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    for (NSURL* url in urls) {
        BOOL accessing = [url startAccessingSecurityScopedResource];
        NSString* filename = [url lastPathComponent];
        NSString* dest = [self.documentsPath stringByAppendingPathComponent:filename];

        NSFileManager* fm = [NSFileManager defaultManager];
        if ([fm fileExistsAtPath:dest]) {
            [fm removeItemAtPath:dest error:nil];
        }
        NSError* error = nil;
        [fm copyItemAtURL:url toURL:[NSURL fileURLWithPath:dest] error:&error];

        if (accessing) {
            [url stopAccessingSecurityScopedResource];
        }
        if (!error) {
            g_fileImported = true;
        }
    }
    g_filePickerActive = false;
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    g_filePickerActive = false;
}

@end

static RetroReadDocumentPicker* g_pickerDelegate = nil;

std::string iosDocumentsPath() {
    @autoreleasepool {
        NSArray* paths = NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSUserDomainMask, YES);
        if (paths.count == 0) {
            return ".";
        }
        return std::string([paths[0] UTF8String]);
    }
}

std::string iosBundleResourcePath() {
    @autoreleasepool {
        NSString* path = [[NSBundle mainBundle] resourcePath];
        if (path == nil) {
            return ".";
        }
        return std::string([path UTF8String]);
    }
}

int iosTopSafeInset() {
    @autoreleasepool {
        if (@available(iOS 15.0, *)) {
            for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
                if ([scene isKindOfClass:[UIWindowScene class]]) {
                    UIWindowScene* ws = (UIWindowScene*)scene;
                    UIWindow* w = ws.windows.firstObject;
                    if (w) return static_cast<int>(w.safeAreaInsets.top);
                }
            }
        }
        return 50;
    }
}

void iosOpenFilePicker() {
    @autoreleasepool {
        if (g_filePickerActive) return;
        g_filePickerActive = true;
        g_fileImported = false;

        if (!g_pickerDelegate) {
            g_pickerDelegate = [[RetroReadDocumentPicker alloc] init];
        }
        NSArray* paths = NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSUserDomainMask, YES);
        g_pickerDelegate.documentsPath = paths.firstObject;

        NSArray<UTType*>* types = @[
            [UTType typeWithFilenameExtension:@"epub"],
            [UTType typeWithFilenameExtension:@"txt"],
        ];
        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types];
        picker.delegate = g_pickerDelegate;
        picker.allowsMultipleSelection = YES;

        UIViewController* rootVC = nil;
        if (@available(iOS 15.0, *)) {
            for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
                if ([scene isKindOfClass:[UIWindowScene class]]) {
                    UIWindowScene* ws = (UIWindowScene*)scene;
                    rootVC = ws.windows.firstObject.rootViewController;
                    break;
                }
            }
        }
        if (rootVC) {
            [rootVC presentViewController:picker animated:YES completion:nil];
        } else {
            g_filePickerActive = false;
        }
    }
}

bool iosFilePickerActive() {
    return g_filePickerActive;
}

bool iosConsumeFileImported() {
    bool result = g_fileImported;
    g_fileImported = false;
    return result;
}

@interface RetroReadVolumeObserver : NSObject
@end

@implementation RetroReadVolumeObserver

- (void)observeValueForKeyPath:(NSString*)keyPath ofObject:(id)object
                        change:(NSDictionary*)change context:(void*)context {
    if ([keyPath isEqualToString:@"outputVolume"]) {
        float newVol = [change[NSKeyValueChangeNewKey] floatValue];
        if (g_restoreVolume >= 0.0f) {
            float delta = newVol - g_restoreVolume;
            if (delta > 0.001f) {
                g_volumeAction = 1;
            } else if (delta < -0.001f) {
                g_volumeAction = -1;
            }
            // Restore volume to original level without triggering another KVO
            if (g_volumeSlider && std::fabs(delta) > 0.001f) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    [g_volumeSlider setValue:g_restoreVolume animated:NO];
                });
            }
        }
        g_lastVolume = newVol;
    }
}

@end

static RetroReadVolumeObserver* g_volumeObserver = nil;

void iosStartVolumeListener() {
    if (g_volumeListening) return;
    g_volumeListening = true;

    @autoreleasepool {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        [session setCategory:AVAudioSessionCategoryAmbient error:nil];
        [session setActive:YES error:nil];

        g_lastVolume = session.outputVolume;
        g_restoreVolume = session.outputVolume;

        g_volumeObserver = [[RetroReadVolumeObserver alloc] init];
        [session addObserver:g_volumeObserver
                  forKeyPath:@"outputVolume"
                     options:NSKeyValueObservingOptionNew
                     context:nil];

        dispatch_async(dispatch_get_main_queue(), ^{
            UIViewController* rootVC = nil;
            if (@available(iOS 15.0, *)) {
                for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
                    if ([scene isKindOfClass:[UIWindowScene class]]) {
                        UIWindowScene* ws = (UIWindowScene*)scene;
                        rootVC = ws.windows.firstObject.rootViewController;
                        break;
                    }
                }
            }
            if (rootVC) {
                MPVolumeView* volumeView = [[MPVolumeView alloc] initWithFrame:CGRectMake(-2000, -2000, 100, 100)];
                volumeView.alpha = 0.01f;
                volumeView.clipsToBounds = YES;
                volumeView.showsRouteButton = NO;
                [rootVC.view addSubview:volumeView];

                for (UIView* subview in volumeView.subviews) {
                    if ([subview isKindOfClass:[UISlider class]]) {
                        g_volumeSlider = (UISlider*)subview;
                        break;
                    }
                }
            }
        });
    }
}

int iosConsumeVolumeAction() {
    int action = g_volumeAction;
    g_volumeAction = 0;
    return action;
}

__attribute__((constructor))
static void iosSetupEnvironment() {
    @autoreleasepool {
        NSString* resourcePath = [[NSBundle mainBundle] resourcePath];
        if (resourcePath) {
            NSString* assetsPath = [resourcePath stringByAppendingPathComponent:@"assets"];
            setenv("NEXTREADING_ASSETS_PATH", [assetsPath UTF8String], 0);
        }
    }
}
