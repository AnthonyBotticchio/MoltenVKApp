#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

#include <memory>
#include "vk_renderer/vk_renderer.hpp"  // adjust include to your header

@interface VulkanMetalView : NSView
@property(nonatomic, readonly) CAMetalLayer* metalLayer;
@end

@implementation VulkanMetalView
+ (Class)layerClass
{
    return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        self.wantsLayer = YES;
        CAMetalLayer* layer = (CAMetalLayer*)self.layer;
        layer.device = MTLCreateSystemDefaultDevice();
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = YES;
    }
    return self;
}

- (CAMetalLayer*)metalLayer
{
    return (CAMetalLayer*)self.layer;
}
@end

int main(int argc, const char* argv[])
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];

        NSRect frame = NSMakeRect(0, 0, 800, 600);
        NSWindowStyleMask style = NSWindowStyleMaskTitled
                                | NSWindowStyleMaskClosable
                                | NSWindowStyleMaskResizable;

        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:style
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        [window setTitle:@"MoltenVK macOS"];
        [window makeKeyAndOrderFront:nil];

        VulkanMetalView* view = [[VulkanMetalView alloc] initWithFrame:frame];
        window.contentView = view;

        // Example: if your renderer class is similar to earlier:
        // std::unique_ptr<VulkanRenderer> renderer = std::make_unique<VulkanRenderer>();
        // renderer->init((__bridge void*)view.metalLayer, 800, 600);

        [NSApp run];
    }
    return 0;
}
