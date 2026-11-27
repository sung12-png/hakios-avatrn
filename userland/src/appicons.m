/*
 * Part of libnodictation.dylib (the library our apps are started with): app icons without a graphics chip. UIKit asks
 * MobileIcons for an application's icon, which scales it with CoreImage, whose context ends in a null pointer in an app
 * here (no GL driver; kCIContextUseSoftwareRenderer does not keep it off that road): Settings > Notifications crashed.
 * UIKit's method is replaced by plain Core Graphics drawing: the picture scaled to the format's size, corners rounded.
 * Not in SpringBoard, which draws its icons itself.
 */
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CGFloat
side_of(NSInteger format)
{
	/* (the formats' sizes in points: small as in Settings' rows, Spotlight's, the home screen's) */
	return format == 1 ? 40 : format == 2 ? 60 : 29;
}

static UIImage *
plain_icon(UIImage *self, SEL selector, NSInteger format, BOOL precomposed, NSInteger idiom, double scale)
{
	CGFloat side = side_of(format);
	CGRect all = CGRectMake(0, 0, side, side);
	UIGraphicsImageRendererFormat *how = [UIGraphicsImageRendererFormat preferredFormat];

	(void)selector, (void)precomposed, (void)idiom;
	how.opaque = NO;
	if (scale > 0) {
		how.scale = scale;
	}
	return [[[UIGraphicsImageRenderer alloc] initWithSize:all.size format:how] imageWithActions:^(UIGraphicsImageRendererContext *context) {
		(void)context;
		[[UIBezierPath bezierPathWithRoundedRect:all cornerRadius:side * 0.225] addClip];
		[self drawInRect:all];
	}];
}

__attribute__((constructor))
static void
appicons_init(void)
{
	Method method;

	if (strcmp(getprogname(), "SpringBoard") == 0) {
		return;
	}
	method = class_getInstanceMethod([UIImage class], sel_registerName("_applicationIconImageForFormat:precomposed:idiom:scale:"));
	if (method == NULL) {
		fprintf(stderr, "hakios-appicons: %s: UIKit has no such method\n", getprogname());
		return;
	}
	method_setImplementation(method, (IMP)plain_icon);
	fprintf(stderr, "hakios-appicons: %s: icons drawn plainly\n", getprogname());
}
