#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/scrnsaver.h>
#include "xstuff.h"
#include "log.h"
extern FILE* logger;
/*!
 * This function works around an XServer idleTime bug in the
 * XScreenSaverExtension if dpms is running. In this case the current
 * dpms-state time is always subtracted from the current idletime.
 * This means: XScreenSaverInfo->idle is not the time since the last
 * user activity, as descriped in the header file of the extension.
 * This result in SUSE bug # and sf.net bug #. The bug in the XServer itself
 * is reported at https://bugs.freedesktop.org/buglist.cgi?quicksearch=6439.
 *
 * Workaround: Check if if XServer is in a dpms state, check the 
 *             current timeout for this state and add this value to 
 *             the current idle time and return.
 *
 * \param _idleTime a unsigned long value with the current idletime from
 *                  XScreenSaverInfo->idle
 * \return a unsigned long with the corrected idletime
 */
unsigned long workaroundCreepyXServer(Display *dpy, unsigned long _idleTime ){
  int dummy;
  CARD16 standby, suspend, off;
  CARD16 state;
  BOOL onoff;

  if (DPMSQueryExtension(dpy, &dummy, &dummy)) {
    if (DPMSCapable(dpy)) {
      DPMSGetTimeouts(dpy, &standby, &suspend, &off);
      DPMSInfo(dpy, &state, &onoff);

      if (onoff) {
        switch (state) {
          case DPMSModeStandby:
            /* this check is a littlebit paranoid, but be sure */
            if (_idleTime < (unsigned) (standby * 1000))
              _idleTime += (standby * 1000);
            break;
          case DPMSModeSuspend:
            if (_idleTime < (unsigned) ((suspend + standby) * 1000))
              _idleTime += ((suspend + standby) * 1000);
            break;
          case DPMSModeOff:
            if (_idleTime < (unsigned) ((off + suspend + standby) * 1000))
              _idleTime += ((off + suspend + standby) * 1000);
            break;
          case DPMSModeOn:
          default:
            break;
        }
      }
    } 
  }

  return _idleTime;
}

extern char* display;
int getIdleTime(unsigned long* idleTime) {
	Display* dpy;
	int event, error;
	XScreenSaverInfo* ssi;

	if(idleTime == NULL) {
		LOG("paramter NULL\n");
		return -1;
	}
	dpy = XOpenDisplay(display);
	if(dpy == NULL) {
		LOG("Failed to open %s\n", display);
		return -1;
	}

	if(!XScreenSaverQueryExtension(dpy, &event, &error)) {
		LOG("Could not query screensaver\n");
		XCloseDisplay(dpy);
		return -1;
	}
	ssi = XScreenSaverAllocInfo();
	if(ssi == NULL) {
		LOG("Failed to allocate screensaver info\n");
		XCloseDisplay(dpy);
		return -1;
	}
	if(!XScreenSaverQueryInfo(dpy, DefaultRootWindow(dpy), ssi)) {
		XFree(ssi);
		XCloseDisplay(dpy);
		LOG("Screensaver extension not supported\n");
		return -1;
	}

	*idleTime = workaroundCreepyXServer(dpy, ssi->idle);

	XFree(ssi);
	XCloseDisplay(dpy);
	return 0;
}
