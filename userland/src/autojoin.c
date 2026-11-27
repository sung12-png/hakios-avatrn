/*
 * autojoin: iOS's own auto-join fails here (corewifi's scan is refused before it reaches the driver), so a saved network is
 * asked of wifid the way a tap in Settings asks: WiFiDeviceClientAssociateAsync(), and wifid takes the password from the
 * keychain itself. No name and no password is read or printed here.
 *
 * Build (arm64, against the SDK's private MobileWiFi), then sign with the entitlements wifid asks for:
 *   clang ... -Wl,-adhoc_codesign -F<SDK>/System/Library/PrivateFrameworks -framework CoreFoundation \
 *       -framework MobileWiFi -o autojoin userland/src/autojoin.c
 *   tools/adhoc_sign.py autojoin com.hakios.autojoin com.apple.wifi.manager-access platform-application
 * Run in the background from the RAM image's etc/rc.jobs: autojoin >> /private/var/mobile/hakios-autojoin.log &
 */
#include <CoreFoundation/CoreFoundation.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

typedef const struct __WiFiManager *WiFiManagerRef;
typedef const struct __WiFiDevice *WiFiDeviceRef;
typedef const struct __WiFiNetwork *WiFiNetworkRef;
extern WiFiManagerRef WiFiManagerClientCreate(CFAllocatorRef allocator, int type);
extern CFArrayRef WiFiManagerClientCopyDevices(WiFiManagerRef manager);
extern CFArrayRef WiFiManagerClientCopyNetworks(WiFiManagerRef manager);
extern void WiFiManagerClientScheduleWithRunLoop(WiFiManagerRef manager, CFRunLoopRef loop, CFStringRef mode);
extern int WiFiDeviceClientAssociateAsync(WiFiDeviceRef device, WiFiNetworkRef network, void *callback, void *context);

struct request {
	char name[16];
	int32_t type, value;
	uint32_t length;
	void *data __attribute__((aligned(8)));
};
#define SIOCGA80211     0xc02869c9u

static int answered, answer;

static void
associated(WiFiDeviceRef device, WiFiNetworkRef network, CFDictionaryRef data, int error, void *context)
{
	(void)device, (void)network, (void)data, (void)context;
	answered = 1;
	answer = error;
	CFRunLoopStop(CFRunLoopGetCurrent());
}

/* is a network joined: the kernel gives its name only then */
static int
joined(void)
{
	uint8_t ssid[32];
	struct request r = { .name = "en0", .type = 1, .length = sizeof(ssid), .data = ssid };
	int s = socket(AF_INET, SOCK_DGRAM, 0), yes;

	yes = s >= 0 && ioctl(s, SIOCGA80211, &r) == 0 && r.length != 0;
	if (s >= 0) {
		close(s);
	}
	return yes;
}

int
main(void)
{
	WiFiManagerRef manager = NULL;
	unsigned second = 0;

	setvbuf(stdout, NULL, _IOLBF, 0);
#ifdef CLEAN_DIAGNOSTIC_LOG
	/* (a diagnostic image of ours had wifid write its log here, with the networks' names in it) */
	unlink("/private/var/mobile/hakios-wifid.log");
#endif
	printf("=== autojoin\n");
	for (int turn = 0; turn < 24; turn++) {
		CFArrayRef devices = NULL, networks = NULL;
		CFIndex count = 0;

		sleep(turn == 0 ? 5 : 10);
		second += turn == 0 ? 5 : 10;
		if (joined()) {
			printf("%us: joined already: done\n", second);
			return 0;
		}
		if (if_nametoindex("en0") == 0) {
			continue;
		}
		if (manager == NULL) {
			manager = WiFiManagerClientCreate(kCFAllocatorDefault, 0);
			if (manager == NULL) {
				printf("%us: no manager client\n", second);
				continue;
			}
			WiFiManagerClientScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		}
		devices = WiFiManagerClientCopyDevices(manager);
		networks = WiFiManagerClientCopyNetworks(manager);
		count = networks != NULL ? CFArrayGetCount(networks) : 0;
		printf("%us: %ld radios, %ld saved networks\n", second, devices ? (long)CFArrayGetCount(devices) : 0L, (long)count);
		for (CFIndex i = 0; devices != NULL && CFArrayGetCount(devices) > 0 && i < count && !joined(); i++) {
			WiFiDeviceRef device = (WiFiDeviceRef)CFArrayGetValueAtIndex(devices, 0);
			WiFiNetworkRef network = (WiFiNetworkRef)CFArrayGetValueAtIndex(networks, i);
			int started;

			answered = 0;
			started = WiFiDeviceClientAssociateAsync(device, network, (void *)associated, NULL);
			printf("%us: network %ld asked for: %d\n", second, (long)i, started);
			if (started == 0) {
				CFRunLoopRunInMode(kCFRunLoopDefaultMode, 25.0, false);
				second += 25;
				printf("%us: network %ld: %s %d, joined: %s\n", second, (long)i, answered ? "answer" : "no answer", answer,
				    joined() ? "YES" : "no");
			}
		}
		if (devices != NULL) {
			CFRelease(devices);
		}
		if (networks != NULL) {
			CFRelease(networks);
		}
		if (joined()) {
			printf("%us: done\n", second);
			return 0;
		}
	}
	printf("gave up\n");
	return 1;
}
