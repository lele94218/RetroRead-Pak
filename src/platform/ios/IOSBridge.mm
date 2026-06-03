#import <Foundation/Foundation.h>
#include <cstdlib>
#include <string>

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
