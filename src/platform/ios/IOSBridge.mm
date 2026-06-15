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

// --- Claude API Translation ---

static NSString* g_claudeResult = nil;
static bool g_claudeReady = false;
static bool g_claudeError = false;
static bool g_claudeInFlight = false;

void iosClaudeTranslateStart(const char* text, const char* apiKey) {
    if (g_claudeInFlight) return;
    g_claudeInFlight = true;
    g_claudeReady = false;
    g_claudeError = false;
    g_claudeResult = nil;

    @autoreleasepool {
        NSString* nsText = [NSString stringWithUTF8String:text];
        NSString* nsApiKey = [NSString stringWithUTF8String:apiKey];

        NSString* escapedText = [nsText copy];
        escapedText = [escapedText stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"];
        escapedText = [escapedText stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
        escapedText = [escapedText stringByReplacingOccurrencesOfString:@"\n" withString:@"\\n"];
        escapedText = [escapedText stringByReplacingOccurrencesOfString:@"\t" withString:@"\\t"];

        NSString* jsonBody = [NSString stringWithFormat:
            @"{\"model\":\"claude-sonnet-4-20250514\","
             "\"max_tokens\":2048,"
             "\"messages\":[{\"role\":\"user\","
             "\"content\":\"Translate the following text to Chinese. "
             "Use ONLY plain text, no markdown, no bold, no bullet points, no emoji, no special symbols. "
             "First output the translation. "
             "Then on a new line output 'Vocabulary:' followed by difficult words, one per line in format 'word - explanation in Chinese'. "
             "If no difficult words, skip the vocabulary section.\\n\\n%@\"}]}",
            escapedText];

        NSURL* url = [NSURL URLWithString:@"https://api.anthropic.com/v1/messages"];
        NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:url];
        req.HTTPMethod = @"POST";
        [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
        [req setValue:nsApiKey forHTTPHeaderField:@"x-api-key"];
        [req setValue:@"2023-06-01" forHTTPHeaderField:@"anthropic-version"];
        req.HTTPBody = [jsonBody dataUsingEncoding:NSUTF8StringEncoding];
        req.timeoutInterval = 30.0;

        NSURLSession* session = [NSURLSession sharedSession];
        [[session dataTaskWithRequest:req completionHandler:
            ^(NSData* data, NSURLResponse* response, NSError* error) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (error || !data) {
                        g_claudeError = true;
                        g_claudeInFlight = false;
                        g_claudeReady = true;
                        return;
                    }
                    NSString* body = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                    // Extract "text":"..." from response JSON
                    NSRange textRange = [body rangeOfString:@"\"text\":\""];
                    if (textRange.location != NSNotFound) {
                        NSUInteger start = textRange.location + textRange.length;
                        // Find closing quote, handling escapes
                        NSMutableString* extracted = [NSMutableString string];
                        BOOL escaped = NO;
                        for (NSUInteger i = start; i < body.length; ++i) {
                            unichar ch = [body characterAtIndex:i];
                            if (escaped) {
                                if (ch == 'n') [extracted appendString:@"\n"];
                                else if (ch == 't') [extracted appendString:@"\t"];
                                else [extracted appendFormat:@"%C", ch];
                                escaped = NO;
                            } else if (ch == '\\') {
                                escaped = YES;
                            } else if (ch == '"') {
                                break;
                            } else {
                                [extracted appendFormat:@"%C", ch];
                            }
                        }
                        g_claudeResult = [extracted copy];
                    } else {
                        g_claudeError = true;
                    }
                    g_claudeInFlight = false;
                    g_claudeReady = true;
                });
            }] resume];
    }
}

bool iosClaudeTranslateReady() { return g_claudeReady; }
bool iosClaudeTranslateError() { return g_claudeError; }

std::string iosClaudeTranslateResult() {
    return g_claudeResult ? std::string([g_claudeResult UTF8String]) : "";
}

void iosClaudeTranslateConsume() {
    g_claudeReady = false;
    g_claudeError = false;
    g_claudeResult = nil;
    g_claudeInFlight = false;
}

// --- Gemini API Translation ---

static NSString* g_geminiResult = nil;
static bool g_geminiReady = false;
static bool g_geminiError = false;
static bool g_geminiInFlight = false;

void iosGeminiTranslateStart(const char* text, const char* apiKey) {
    if (g_geminiInFlight) return;
    g_geminiInFlight = true;
    g_geminiReady = false;
    g_geminiError = false;
    g_geminiResult = nil;

    @autoreleasepool {
        NSString* nsText = [NSString stringWithUTF8String:text];
        NSString* nsApiKey = [NSString stringWithUTF8String:apiKey];

        NSString* escapedText = [nsText copy];
        escapedText = [escapedText stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"];
        escapedText = [escapedText stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
        escapedText = [escapedText stringByReplacingOccurrencesOfString:@"\n" withString:@"\\n"];
        escapedText = [escapedText stringByReplacingOccurrencesOfString:@"\t" withString:@"\\t"];

        NSString* prompt = [NSString stringWithFormat:
            @"Translate the following text to Chinese. "
             "Use ONLY plain text, no markdown, no bold, no bullet points, no emoji, no special symbols. "
             "First output the translation. "
             "Then on a new line output 'Vocabulary:' followed by difficult words, one per line in format 'word - explanation in Chinese'. "
             "If no difficult words, skip the vocabulary section.\\n\\n%@",
            escapedText];

        NSString* jsonBody = [NSString stringWithFormat:
            @"{\"contents\":[{\"parts\":[{\"text\":\"%@\"}]}]}",
            prompt];

        NSString* urlStr = [NSString stringWithFormat:
            @"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%@",
            nsApiKey];

        NSURL* url = [NSURL URLWithString:urlStr];
        NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:url];
        req.HTTPMethod = @"POST";
        [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
        req.HTTPBody = [jsonBody dataUsingEncoding:NSUTF8StringEncoding];
        req.timeoutInterval = 30.0;

        NSURLSession* session = [NSURLSession sharedSession];
        [[session dataTaskWithRequest:req completionHandler:
            ^(NSData* data, NSURLResponse* response, NSError* error) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (error || !data) {
                        g_geminiError = true;
                        g_geminiInFlight = false;
                        g_geminiReady = true;
                        return;
                    }
                    NSString* body = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                    // Extract "text": "..." from Gemini response
                    NSRange textRange = [body rangeOfString:@"\"text\": \""];
                    if (textRange.location == NSNotFound) {
                        textRange = [body rangeOfString:@"\"text\":\""];
                    }
                    if (textRange.location != NSNotFound) {
                        NSUInteger start = textRange.location + textRange.length;
                        NSMutableString* extracted = [NSMutableString string];
                        BOOL escaped = NO;
                        for (NSUInteger i = start; i < body.length; ++i) {
                            unichar ch = [body characterAtIndex:i];
                            if (escaped) {
                                if (ch == 'n') [extracted appendString:@"\n"];
                                else if (ch == 't') [extracted appendString:@"\t"];
                                else [extracted appendFormat:@"%C", ch];
                                escaped = NO;
                            } else if (ch == '\\') {
                                escaped = YES;
                            } else if (ch == '"') {
                                break;
                            } else {
                                [extracted appendFormat:@"%C", ch];
                            }
                        }
                        g_geminiResult = [extracted copy];
                    } else {
                        g_geminiError = true;
                    }
                    g_geminiInFlight = false;
                    g_geminiReady = true;
                });
            }] resume];
    }
}

bool iosGeminiTranslateReady() { return g_geminiReady; }
bool iosGeminiTranslateError() { return g_geminiError; }

std::string iosGeminiTranslateResult() {
    return g_geminiResult ? std::string([g_geminiResult UTF8String]) : "";
}

void iosGeminiTranslateConsume() {
    g_geminiReady = false;
    g_geminiError = false;
    g_geminiResult = nil;
    g_geminiInFlight = false;
}

// --- API Key Input Dialog ---

static NSString* g_apiKeyInput = nil;
static bool g_apiKeyInputReady = false;

void iosShowApiKeyInput(const char* currentKey) {
    @autoreleasepool {
        g_apiKeyInputReady = false;
        g_apiKeyInput = nil;

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
        if (!rootVC) return;

        NSString* current = currentKey ? [NSString stringWithUTF8String:currentKey] : @"";
        UIAlertController* alert = [UIAlertController
            alertControllerWithTitle:@"Claude API Key"
            message:@"Enter your Anthropic API key"
            preferredStyle:UIAlertControllerStyleAlert];

        [alert addTextFieldWithConfigurationHandler:^(UITextField* field) {
            field.text = current;
            field.placeholder = @"sk-ant-...";
            field.secureTextEntry = NO;
            field.autocorrectionType = UITextAutocorrectionTypeNo;
        }];

        [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel
            handler:^(UIAlertAction* action) {
                g_apiKeyInputReady = true;
                g_apiKeyInput = nil;
            }]];

        [alert addAction:[UIAlertAction actionWithTitle:@"Save" style:UIAlertActionStyleDefault
            handler:^(UIAlertAction* action) {
                g_apiKeyInput = [alert.textFields.firstObject.text copy];
                g_apiKeyInputReady = true;
            }]];

        [rootVC presentViewController:alert animated:YES completion:nil];
    }
}

bool iosApiKeyInputReady() { return g_apiKeyInputReady; }

std::string iosConsumeApiKeyInput() {
    g_apiKeyInputReady = false;
    return g_apiKeyInput ? std::string([g_apiKeyInput UTF8String]) : "";
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
