#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/scrnsaver.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "log.h"

#define DEFAULT_IDLE_SLEEP 60*30 //30 minutes
#define DEFAULT_DISPLAY ":0"
#define PORT 22
#define HELP "-h"
#define VERBOSE "-v"
#define FORGROUND "-f"
#define IDLE_TIME "-t"
#define IGNORE_SSH "-i"


void usage(char* proggy);
int getIdleTime(Display* dpy, unsigned long* idleTime);
unsigned long workaroundCreepyXServer(Display *dpy, unsigned long _idleTime );
int background();
int goToSleep();
int mainRoutine(Display* dpy);

int verbose = 0;
int forground = 0;
int ignoreSsh = 0;
unsigned int idleToSleep = DEFAULT_IDLE_SLEEP;
char* display = DEFAULT_DISPLAY;

int main(int argc, char* argv[]) {
	Display *dpy;
	int i;
	unsigned long tempIdle;
	char* ptr;

	for(i = 1; i < argc; i++) {
		if(strcmp(HELP, argv[i]) == 0) {
			usage(argv[0]);
			return 0;
		} else if(strcmp(VERBOSE, argv[i]) == 0) {
			verbose = 1;
		} else if(strcmp(FORGROUND, argv[i]) == 0) {
			forground = 1;
		} else if(strcmp(IGNORE_SSH, argv[i]) == 0) {
			ignoreSsh = 1;
		} else if(strcmp(IDLE_TIME, argv[i]) == 0) {
			if((i+1) >= argc) {
				fprintf(stderr, "Invalid idle time override \"%s\"\n", argv[i+1]);
				fprintf(stderr, "\"%s\"\n", ptr);
				return -1;
			}
			tempIdle = strtol(argv[i+1], &ptr, 10);
			if(ptr[0] != '\0') {
				fprintf(stderr, "Invalid idle time override \"%s\"\n", argv[i+1]);
				fprintf(stderr, "\"%s\"\n", ptr);
				return -1;
			}
			idleToSleep = tempIdle;
			i++;
		} else {
			display = argv[i];
		}
	}
	
	dpy = XOpenDisplay(display);
	if(dpy == NULL) {
		LOG("Failed to open %s\n", display);
		return -1;
	}
	return mainRoutine(dpy);
}

int mainRoutine(Display* dpy) {
	unsigned long idleTime = 0;
	unsigned int sshConns = 0;

	//try to become root
	if(setuid(0) != 0) {
		LOG("Failed to become root!\n");
		return -1;
	}

	if(forground == 0) {
		if(background()) {
			//we are the parent, bail
			return 0;
		}
	}
	while(1) {
		idleTime = 0;
		sshConns = 0;
		if(getIdleTime(dpy, &idleTime)) {
			LOG("Failed to get idleTime\n");
			return -1;
		}
		if(activeSSHSessions(&sshConns, PORT)) {
			LOG("Failed to get ssh connections\n");
			return -1;
		}
		if((idleTime/1000) > idleToSleep) {
			if(sshConns == 0 || ignoreSsh != 0) {
				goToSleep();
				continue;
			}
			printf("Not sleeping due to SSH session\n");
		}
		sleep(10);
	}
	return 0;
}


int getIdleTime(Display* dpy, unsigned long* idleTime) {
	int event, error;
	XScreenSaverInfo* ssi;

	if(idleTime == NULL) {
		LOG("paramter NULL\n");
		return -1;
	}

	if(!XScreenSaverQueryExtension(dpy, &event, &error)) {
		LOG("Could not query screensaver\n");
		return -1;
	}
	ssi = XScreenSaverAllocInfo();
	if(ssi == NULL) {
		LOG("Failed to allocate screensaver info\n");
		return -1;
	}
	if(!XScreenSaverQueryInfo(dpy, DefaultRootWindow(dpy), ssi)) {
		XFree(ssi);
		LOG("Screensaver extension not supported\n");
		return -1;
	}

	*idleTime = workaroundCreepyXServer(dpy, ssi->idle);

	XFree(ssi);
	return 0;
}

void usage(char* proggy) {
	fprintf(stdout, "%s -h\tShow usage\n", proggy);
	fprintf(stdout, "%s -v\tBe verbose\n", proggy);
	fprintf(stdout, "%s -f\tDo not background the process\n", proggy);
	fprintf(stdout, "%s -t <idle seconds>\tSeconds of idle before sleep\n", proggy);
	fprintf(stdout, "%s <display>\n", proggy);
}


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

int background() {
	if(fork() == 0) {
		//we are the child
		fclose(stdin);
		fclose(stdout);
		if(verbose == 0) {
			fclose(stderr);
		}
		return 0;
	}
	//we are the parent
	return 1;
}

char* timestamp() {
	char* stime;
	time_t ltime;
	ltime = time(NULL);
	stime = ctime(&ltime);
	stime[strlen(stime)-1] = '\0';
	return stime;
}

#define POWERFILE "/sys/power/state"
int nightyNight() {
	FILE* fout = fopen(POWERFILE, "w");
	if(fout == NULL) {
		LOG("Failed to open %s\n", POWERFILE);
		return -1;
	}
	fprintf(fout, "mem\n");
	return fclose(fout);
}

int goToSleep() {
	LOG("%s Going to sleep\n", timestamp());
	//write 3 into the sleep handle
	if(nightyNight()) {
		LOG("%s Failed to go to sleep\n", timestamp());
		return -1;
	}

	//sleep 10 seconds for the sleep to happen
	sleep(10);

	//sleep 10 seconds for the wakeup to finish
	//so that we don't go into a sleep loop
	sleep(10);

	LOG("%s Woke up from sleep\n", timestamp());
	return 0;
}
