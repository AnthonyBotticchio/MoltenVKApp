#include "vk_renderer/vk_renderer.hpp"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>
#include <memory>

@interface VulkanView : UIView
@property ( nonatomic, readonly ) CAMetalLayer* metalLayer;
@end

@implementation VulkanView

+ (Class)layerClass
{
    return [CAMetalLayer class];
}

- (CAMetalLayer*)metalLayer
{
    return (CAMetalLayer*)self.layer;
}

@end

@interface ViewController : UIViewController
@end

@implementation ViewController
{
    std::unique_ptr<VulkanRenderer> _renderer;
    CADisplayLink* _displayLink;
}

- (void)loadView
{
    self.view = [[VulkanView alloc] initWithFrame:[UIScreen mainScreen].bounds];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    VulkanView* v       = (VulkanView*)self.view;
    CAMetalLayer* layer = v.metalLayer;

    layer.device          = MTLCreateSystemDefaultDevice();
    layer.pixelFormat     = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    layer.opaque          = YES;

    CGFloat scale       = [UIScreen mainScreen].scale;
    layer.contentsScale = scale;

    CGSize size        = self.view.bounds.size;
    layer.drawableSize = CGSizeMake( size.width * scale, size.height * scale );

    _renderer = std::make_unique<VulkanRenderer>();
    _renderer->init( (__bridge void*)layer, (uint32_t)layer.drawableSize.width, (uint32_t)layer.drawableSize.height );

    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector( tick )];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];

    VulkanView* v       = (VulkanView*)self.view;
    CAMetalLayer* layer = v.metalLayer;

    CGFloat scale      = [UIScreen mainScreen].scale;
    CGSize size        = self.view.bounds.size;
    layer.drawableSize = CGSizeMake( size.width * scale, size.height * scale );

    if( _renderer )
    {
        _renderer->resize( (uint32_t)layer.drawableSize.width, (uint32_t)layer.drawableSize.height );
    }
}

- (void)tick
{
    if( _renderer )
    {
        _renderer->drawFrame();
    }
}

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property ( strong, nonatomic ) UIWindow* window;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    self.window                    = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    self.window.rootViewController = [ViewController new];
    [self.window makeKeyAndVisible];
    return YES;
}

@end

int main( int argc, char* argv[] )
{
    @autoreleasepool
    {
        return UIApplicationMain( argc, argv, nil, NSStringFromClass( [AppDelegate class] ) );
    }
}
