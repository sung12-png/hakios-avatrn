/*
 * libpastbuddy.dylib, inside fileproviderd (the kernel's list of inserted libraries): the file service turns every
 * client away ("denying connection because the device is not set up") until Apple's setup assistant has been gone
 * through, which cannot happen here; Files then has no storage at all and stops itself when the view is switched.
 *
 * -[FPDServer _computeUnlockedStatusAndSetup] works out two bytes of the server, "unlocked" and _isPastBuddy, and
 * prepares the storage when both are set; -listener:shouldAcceptNewConnection: reads _isPastBuddy itself, not through
 * -isPastBuddy. So the byte is set before that method runs.
 */
#include <objc/runtime.h>
#include <stdio.h>

static void (*compute)(id, SEL);
static ptrdiff_t past_buddy;

static void
compute_past_buddy(id self, SEL selector)
{
	volatile unsigned char *server = (volatile unsigned char *)(void *)self;

	server[past_buddy] = 1;
	compute(self, selector);
	/* (the byte before is "unlocked": without it the storage is not prepared) */
	fprintf(stderr, "hakios-pastbuddy: after it: unlocked %u, past setup %u\n", server[past_buddy - 1], server[past_buddy]);
}

__attribute__((constructor))
static void
pastbuddy_init(void)
{
	Class server = objc_getClass("FPDServer");
	Method method = server != NULL ? class_getInstanceMethod(server, sel_registerName("_computeUnlockedStatusAndSetup")) : NULL;
	Ivar ivar = server != NULL ? class_getInstanceVariable(server, "_isPastBuddy") : NULL;

	if (method == NULL || ivar == NULL) {
		fprintf(stderr, "hakios-pastbuddy: FPDServer is not as expected (%d, %d)\n", method != NULL, ivar != NULL);
		return;
	}
	past_buddy = ivar_getOffset(ivar);
	compute = (void (*)(id, SEL))method_getImplementation(method);
	method_setImplementation(method, (IMP)compute_past_buddy);
	fprintf(stderr, "hakios-pastbuddy: the file service will take the setup assistant as done (byte %td)\n", past_buddy);
}
