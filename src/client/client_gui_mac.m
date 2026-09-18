#import <Cocoa/Cocoa.h>
#include "usbredir_protocol.h"
#include "network_socket.h"
#include "usb_device.h"

@interface ClientAppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTextField *titleLabel;
@property (strong) NSTextField *subLabel;
@property (strong) NSTextField *techIdInput;
@property (strong) NSButton *connectBtn;

@property (strong) NSBox *wizardCard;
@property (strong) NSTextField *step1Label;
@property (strong) NSTextField *step1DevLabel;
@property (strong) NSTextField *step2Label;
@property (strong) NSTextField *step2TechLabel;
@property (strong) NSTextField *step3Label;
@property (strong) NSProgressIndicator *progressBar;
@property (strong) NSTextField *step4Label;
@end

static socket_t g_mac_client_sock = INVALID_SOCKET;
static char g_mac_tech_id[32] = "7891";
static usb_device_info_t g_mac_device;
static ClientAppDelegate *g_appDelegate = nil;

@implementation ClientAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSRect frame = NSMakeRect(0, 0, 600, 480);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"USB Redirector Client (macOS Native)"];
    [self.window center];
    [self.window setBackgroundColor:[NSColor colorWithCalibratedRed:0.96 green:0.97 blue:0.98 alpha:1.0]];

    NSView *contentView = [self.window contentView];

    // Header Card
    NSBox *headerBox = [[NSBox alloc] initWithFrame:NSMakeRect(0, 400, 600, 80)];
    [headerBox setBoxType:NSBoxCustom];
    [headerBox setBorderType:NSNoBorder];
    [headerBox setFillColor:[NSColor colorWithCalibratedRed:0.12 green:0.15 blue:0.20 alpha:1.0]];
    [contentView addSubview:headerBox];

    self.titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(24, 40, 550, 28)];
    [self.titleLabel setStringValue:@"USB Redirector Customer Client"];
    [self.titleLabel setFont:[NSFont boldSystemFontOfSize:18]];
    [self.titleLabel setTextColor:[NSColor whiteColor]];
    [self.titleLabel setEditable:NO];
    [self.titleLabel setBordered:NO];
    [self.titleLabel setDrawsBackground:NO];
    [headerBox addSubview:self.titleLabel];

    self.subLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(24, 16, 550, 20)];
    [self.subLabel setStringValue:@"Share Mac USB Devices with Remote Windows Technician"];
    [self.subLabel setFont:[NSFont systemFontOfSize:12]];
    [self.subLabel setTextColor:[NSColor colorWithCalibratedRed:0.7 green:0.75 blue:0.8 alpha:1.0]];
    [self.subLabel setEditable:NO];
    [self.subLabel setBordered:NO];
    [self.subLabel setDrawsBackground:NO];
    [headerBox addSubview:self.subLabel];

    // Screen 1: Tech ID Entry
    NSTextField *labelPrompt = [[NSTextField alloc] initWithFrame:NSMakeRect(40, 320, 520, 24)];
    [labelPrompt setStringValue:@"Enter Numeric Technician ID:"];
    [labelPrompt setFont:[NSFont boldSystemFontOfSize:14]];
    [labelPrompt setTextColor:[NSColor labelColor]];
    [labelPrompt setEditable:NO];
    [labelPrompt setBordered:NO];
    [labelPrompt setDrawsBackground:NO];
    [contentView addSubview:labelPrompt];

    NSTextField *labelHint = [[NSTextField alloc] initWithFrame:NSMakeRect(40, 290, 520, 20)];
    [labelHint setStringValue:@"Please enter the 4-digit ID (e.g. 7891) provided by your support technician."];
    [labelHint setFont:[NSFont systemFontOfSize:12]];
    [labelHint setTextColor:[NSColor secondaryLabelColor]];
    [labelHint setEditable:NO];
    [labelHint setBordered:NO];
    [labelHint setDrawsBackground:NO];
    [contentView addSubview:labelHint];

    self.techIdInput = [[NSTextField alloc] initWithFrame:NSMakeRect(160, 230, 280, 40)];
    [self.techIdInput setStringValue:@"7891"];
    [self.techIdInput setFont:[NSFont fontWithName:@"Menlo" size:22]];
    [self.techIdInput setAlignment:NSTextAlignmentCenter];
    [contentView addSubview:self.techIdInput];

    self.connectBtn = [[NSButton alloc] initWithFrame:NSMakeRect(210, 160, 180, 44)];
    [self.connectBtn setTitle:@"Connect to Technician"];
    [self.connectBtn setBezelStyle:NSBezelStyleRounded];
    [self.connectBtn setTarget:self];
    [self.connectBtn setAction:@selector(onConnectClicked:)];
    [contentView addSubview:self.connectBtn];

    // Screen 2: 4-Step Wizard Container (Hidden initially)
    self.wizardCard = [[NSBox alloc] initWithFrame:NSMakeRect(30, 40, 540, 340)];
    [self.wizardCard setBoxType:NSBoxCustom];
    [self.wizardCard setBorderColor:[NSColor colorWithCalibratedRed:0.85 green:0.87 blue:0.90 alpha:1.0]];
    [self.wizardCard setFillColor:[NSColor whiteColor]];
    [self.wizardCard setCornerRadius:12.0];
    [self.wizardCard setHidden:YES];
    [contentView addSubview:self.wizardCard];

    // Step 1
    self.step1Label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 280, 500, 24)];
    [self.step1Label setStringValue:@"1. Plug your Mac USB device"];
    [self.step1Label setFont:[NSFont boldSystemFontOfSize:14]];
    [self.step1Label setTextColor:[NSColor systemGreenColor]];
    [self.step1Label setEditable:NO]; [self.step1Label setBordered:NO]; [self.step1Label setDrawsBackground:NO];
    [self.wizardCard addSubview:self.step1Label];

    self.step1DevLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(40, 256, 480, 20)];
    [self.step1DevLabel setStringValue:@"Detected USB: SanDisk Ultra USB 3.0 Flash Drive (Serial: 07810024101)"];
    [self.step1DevLabel setFont:[NSFont systemFontOfSize:12]];
    [self.step1DevLabel setTextColor:[NSColor secondaryLabelColor]];
    [self.step1DevLabel setEditable:NO]; [self.step1DevLabel setBordered:NO]; [self.step1DevLabel setDrawsBackground:NO];
    [self.wizardCard addSubview:self.step1DevLabel];

    // Step 2
    self.step2Label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 210, 500, 24)];
    [self.step2Label setStringValue:@"2. Waiting for technician to start servicing your device"];
    [self.step2Label setFont:[NSFont boldSystemFontOfSize:14]];
    [self.step2Label setTextColor:[NSColor systemBlueColor]];
    [self.step2Label setEditable:NO]; [self.step2Label setBordered:NO]; [self.step2Label setDrawsBackground:NO];
    [self.wizardCard addSubview:self.step2Label];

    self.step2TechLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(40, 186, 480, 20)];
    [self.step2TechLabel setStringValue:@"Connected to Technician ID [7891]. Waiting for acceptance..."];
    [self.step2TechLabel setFont:[NSFont systemFontOfSize:12]];
    [self.step2TechLabel setTextColor:[NSColor secondaryLabelColor]];
    [self.step2TechLabel setEditable:NO]; [self.step2TechLabel setBordered:NO]; [self.step2TechLabel setDrawsBackground:NO];
    [self.wizardCard addSubview:self.step2TechLabel];

    // Step 3
    self.step3Label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 140, 500, 24)];
    [self.step3Label setStringValue:@"3. Servicing your device"];
    [self.step3Label setFont:[NSFont boldSystemFontOfSize:14]];
    [self.step3Label setTextColor:[NSColor labelColor]];
    [self.step3Label setEditable:NO]; [self.step3Label setBordered:NO]; [self.step3Label setDrawsBackground:NO];
    [self.wizardCard addSubview:self.step3Label];

    self.progressBar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(40, 110, 460, 20)];
    [self.progressBar setStyle:NSProgressIndicatorStyleBar];
    [self.progressBar setIndeterminate:NO];
    [self.progressBar setMinValue:0.0];
    [self.progressBar setMaxValue:100.0];
    [self.progressBar setDoubleValue:0.0];
    [self.wizardCard addSubview:self.progressBar];

    // Step 4
    self.step4Label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 60, 500, 24)];
    [self.step4Label setStringValue:@"4. Servicing finished"];
    [self.step4Label setFont:[NSFont boldSystemFontOfSize:14]];
    [self.step4Label setTextColor:[NSColor secondaryLabelColor]];
    [self.step4Label setEditable:NO]; [self.step4Label setBordered:NO]; [self.step4Label setDrawsBackground:NO];
    [self.wizardCard addSubview:self.step4Label];

    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)onConnectClicked:(id)sender {
    NSString *val = [self.techIdInput stringValue];
    if ([val length] == 0) return;

    strncpy(g_mac_tech_id, [val UTF8String], sizeof(g_mac_tech_id) - 1);

    [self.wizardCard setHidden:NO];
    [self.techIdInput setHidden:YES];
    [self.connectBtn setHidden:YES];

    [self.step2TechLabel setStringValue:[NSString stringWithFormat:@"Connected to Technician ID [%s]. Waiting for acceptance...", g_mac_tech_id]];

    // Start background network thread to communicate with VPS Server 209.126.81.68
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        net_init();
        usb_device_init(&g_mac_device);

        g_mac_client_sock = net_connect("209.126.81.68", USBREDIR_PORT);
        if (g_mac_client_sock != INVALID_SOCKET) {
            usbredir_header_t hdr;
            usbredir_header_init(&hdr, USBREDIR_CMD_REGISTER_CLIENT, sizeof(usbredir_packet_register_t));

            usbredir_packet_register_t reg_pkt;
            memset(&reg_pkt, 0, sizeof(reg_pkt));
            strncpy(reg_pkt.client_ip, "192.168.10.50 (macOS)", sizeof(reg_pkt.client_ip) - 1);
            strncpy(reg_pkt.target_tech_id, g_mac_tech_id, sizeof(reg_pkt.target_tech_id) - 1);
            reg_pkt.device = g_mac_device;

            net_send_all(g_mac_client_sock, &hdr, sizeof(hdr));
            net_send_all(g_mac_client_sock, &reg_pkt, sizeof(reg_pkt));

            while (1) {
                usbredir_header_t rx_hdr;
                if (net_recv_all(g_mac_client_sock, &rx_hdr, sizeof(rx_hdr)) < 0) break;

                if (usbredir_header_verify(&rx_hdr)) {
                    if (rx_hdr.cmd == USBREDIR_CMD_START_SERVICE) {
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [self.step3Label setTextColor:[NSColor systemGreenColor]];
                            for (int i = 0; i <= 100; i += 10) {
                                [self.progressBar setDoubleValue:(double)i];
                            }
                        });
                    } else if (rx_hdr.cmd == USBREDIR_CMD_FINISH_SERVICE) {
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [self.step4Label setTextColor:[NSColor systemGreenColor]];
                        });
                        break;
                    }
                }
            }
        }
    });
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        g_appDelegate = [[ClientAppDelegate alloc] init];
        [app setDelegate:g_appDelegate];
        [app run];
    }
    return 0;
}
