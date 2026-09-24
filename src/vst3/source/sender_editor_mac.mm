#include "sender_editor.hpp"

#include "dhrelink/shared_audio_stream.hpp"
#include "public.sdk/source/common/pluginview.h"

#import <AppKit/AppKit.h>

#include <cstring>

namespace {
constexpr int kViewWidth = 360;
constexpr int kViewHeight = 152;
}

@interface DhreLinkStatusView : NSView {
@private
  dhrelink::StreamStatusReader reader_;
  dhrelink::StreamStatus status_;
  NSTimer* timer_;
}
@end

@implementation DhreLinkStatusView

- (instancetype)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self != nil) {
    timer_ = [[NSTimer scheduledTimerWithTimeInterval:0.1
                                              target:self
                                            selector:@selector(refreshStatus:)
                                            userInfo:nil
                                             repeats:YES] retain];
    [self refreshStatus:nil];
  }
  return self;
}

- (void)dealloc {
  [timer_ invalidate];
  [timer_ release];
  [super dealloc];
}

- (BOOL)isFlipped { return YES; }

- (void)refreshStatus:(NSTimer*)unused {
  (void)unused;
  if (!reader_.isOpen()) (void)reader_.open("main-mix");
  if (!reader_.isOpen() || !reader_.snapshot(status_) || !status_.senderConnected) {
    reader_.close();
    status_ = {};
  }
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  [[NSColor colorWithCalibratedRed:0.039 green:0.039 blue:0.047 alpha:1.0] setFill];
  NSRectFill([self bounds]);
  [[NSColor colorWithCalibratedRed:0.086 green:0.086 blue:0.098 alpha:1.0] setFill];
  NSRectFill(NSMakeRect(0, 0, self.bounds.size.width, 39));

  NSDictionary* titleStyle = @{
    NSFontAttributeName: [NSFont boldSystemFontOfSize:18],
    NSForegroundColorAttributeName: [NSColor whiteColor]
  };
  [@"dhreLink" drawAtPoint:NSMakePoint(14, 8) withAttributes:titleStyle];

  NSColor* statusColor = status_.senderConnected
    ? [NSColor colorWithCalibratedRed:0.37 green:0.89 blue:0.61 alpha:1.0]
    : [NSColor colorWithCalibratedRed:0.62 green:0.62 blue:0.66 alpha:1.0];
  NSDictionary* statusStyle = @{
    NSFontAttributeName: [NSFont boldSystemFontOfSize:12],
    NSForegroundColorAttributeName: statusColor
  };
  NSString* connection = status_.senderConnected ? @"Connected to OBS" : @"Waiting for OBS";
  [connection drawAtPoint:NSMakePoint(170, 12) withAttributes:statusStyle];

  NSDictionary* bodyStyle = @{
    NSFontAttributeName: [NSFont systemFontOfSize:12],
    NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:0.8 green:0.8 blue:0.84 alpha:1.0]
  };
  [@"DAW  →  dhreLink  →  OBS" drawAtPoint:NSMakePoint(14, 55) withAttributes:bodyStyle];
  NSString* format = status_.senderConnected
    ? [NSString stringWithFormat:@"%.1f kHz  |  %@",
        static_cast<double>(status_.sampleRate) / 1000.0,
        status_.channels == 1 ? @"Mono" : @"Stereo"]
    : @"Start the Sender in your DAW";
  [format drawAtPoint:NSMakePoint(14, 90) withAttributes:bodyStyle];
  [@"Developed by dhreian" drawAtPoint:NSMakePoint(14, 125) withAttributes:bodyStyle];
}

@end

namespace dhrelink::vst3 {
namespace {

class SenderEditor final : public Steinberg::CPluginView {
public:
  SenderEditor() { setRect(Steinberg::ViewRect(0, 0, kViewWidth, kViewHeight)); }
  ~SenderEditor() override { detach(); }

  Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override {
    return type != nullptr && std::strcmp(type, Steinberg::kPlatformTypeNSView) == 0
      ? Steinberg::kResultTrue : Steinberg::kResultFalse;
  }

  Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override {
    if (parent == nullptr || view_ != nil || isPlatformTypeSupported(type) != Steinberg::kResultTrue) {
      return Steinberg::kInvalidArgument;
    }
    view_ = [[DhreLinkStatusView alloc] initWithFrame:NSMakeRect(0, 0, kViewWidth, kViewHeight)];
    if (view_ == nil) return Steinberg::kResultFalse;
    [(NSView*)parent addSubview:view_];
    systemWindow = parent;
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult PLUGIN_API removed() override {
    detach();
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override {
    if (newSize == nullptr) return Steinberg::kInvalidArgument;
    setRect(*newSize);
    if (view_ != nil) {
      [view_ setFrameSize:NSMakeSize(newSize->getWidth(), newSize->getHeight())];
    }
    return Steinberg::kResultTrue;
  }

private:
  void detach() noexcept {
    if (view_ != nil) {
      [view_ removeFromSuperview];
      [view_ release];
      view_ = nil;
    }
    systemWindow = nullptr;
  }

  DhreLinkStatusView* view_ = nil;
};

} // namespace

Steinberg::IPlugView* createSenderEditor() { return new SenderEditor(); }

} // namespace dhrelink::vst3
