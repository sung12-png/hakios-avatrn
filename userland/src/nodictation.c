/*
 * libnodictation.dylib, for programs with a keyboard on a phone where Darwin's dictation service does not run
 * (the Motorola Edge 2024's images have no assistantd). There the keyboard is told over and over that
 * dictation's availability has changed and each time works out its languages and buttons again, on the main
 * thread: SpringBoard did nothing else for minutes after the search keyboard came up, so that no touch or
 * button was answered. The two methods that pass the news on are replaced by nothing.
 */
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned dropped;

static void
ignore(void)
{
	if (++dropped == 1 || dropped % 500 == 0) {
		fprintf(stderr, "hakios-nodictation: %s: %u dictation availability changes ignored\n", getprogname(), dropped);
	}
}

static int
replace(const char *class_name, const char *method_name)
{
	Class cls = objc_getClass(class_name);
	SEL selector = sel_registerName(method_name);
	Method method = cls != NULL ? class_getInstanceMethod(cls, selector) : NULL;

	if (method == NULL && cls != NULL) {
		method = class_getClassMethod(cls, selector);
	}
	if (method == NULL) {
		fprintf(stderr, "hakios-nodictation: %s: no -[%s %s]\n", getprogname(), class_name, method_name);
		return 0;
	}
	method_setImplementation(method, (IMP)ignore);
	return 1;
}

__attribute__((constructor))
static void
nodictation_init(void)
{
	int n = replace("UIDictationController", "postUIKeyboardDictationAvailabilityDidChangeNotification") +
	    replace("UIDictationConnection", "dictationConnnectionDidChangeAvailability:");

	fprintf(stderr, "hakios-nodictation: %s: %d of 2 methods replaced\n", getprogname(), n);
}
