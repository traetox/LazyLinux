#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include "log.h"
#include "ssh.h"
#include "xstuff.h"

#define DEFAULT_IDLE_SLEEP 60*30 //30 minutes
#define DEFAULT_DISPLAY ":0"
#define DEFAULT_PORT 22
#define HELP "-h"
#define LOGGING "-l"
#define FORGROUND "-f"
#define IDLE_TIME "-t"
#define IGNORE_SSH "-i"
#define SLEEP_NOW "-n"
#define DELAY_SLEEP "-d"
#define PORT "-p"
#define MUTEX_PATH "/tmp/.LazyLinux"
#define SIG_SLEEP_INTERVAL 20000 //20ms
#define SLEEP_INTERVAL 10
#define SLEEP_DELAY_INTERVAL 600 //10 minutes
#define MAX_DELAY_TIME 60*24*365 //delays longer than one year are not allowed

void usage(char* proggy);
pid_t background();
pid_t getMutex();
pid_t serverRunning();
int dropMutex(pid_t pid);
int signalParent(pid_t pid);
int goToSleep();
int mainRoutine();
int becomeRoot();
int delaySleep(int minutes, pid_t pid);
void sigHandler(int signum);

FILE* logger = NULL;
int forground = 0;
int ignoreSsh = 0;
int sleepOverride = 0;
unsigned short port = DEFAULT_PORT;
unsigned int idleToSleep = DEFAULT_IDLE_SLEEP;
char* display = DEFAULT_DISPLAY;
unsigned int sleepDelay = 0;

int main(int argc, char* argv[]) {
	int i;
	unsigned long tempIdle;
	unsigned long tempDelay;
	char* ptr;
	int sleepNow = 0;
	int sleepDelay = 0;
	pid_t serverPid = 0;

	for(i = 1; i < argc; i++) {
		if(strcmp(HELP, argv[i]) == 0) {
			usage(argv[0]);
			return 0;
		} else if(strcmp(LOGGING, argv[i]) == 0) {
			if((i+1) >= argc) {
				fprintf(stderr, "Logging requires a file\n");
				return -1;
			}
			logger = fopen(argv[i+1], "a");
			if(logger == NULL) {
				fprintf(stderr, "Failed to open logging file\n");
				return -1;
			}
			fprintf(logger, "Process started\n");
			fprintf(stderr, "Logging to %s\n", argv[i+1]);
			i++;
		} else if(strcmp(FORGROUND, argv[i]) == 0) {
			forground = 1;
		} else if(strcmp(PORT, argv[i]) == 0) {
			unsigned long tempPort;
			if((i+1) >= argc) {
				fprintf(stderr, "Port specification requireds a port\n");
				return -1;
			}
			tempPort = strtol(argv[i+1], &ptr, 10);
			if(ptr[0] != '\0') {
				fprintf(stderr, "Invalid port %s\n", argv[i+1]);
				return -1;
			}
			if(tempPort <= 0 || tempPort > 0xffff) {
				fprintf(stderr, "Invalid port %ld\n", tempPort);
				return -1;
			}
			port = (unsigned short)tempPort;
		} else if(strcmp(IGNORE_SSH, argv[i]) == 0) {
			ignoreSsh = 1;
		} else if(strcmp(IDLE_TIME, argv[i]) == 0) {
			if((i+1) >= argc) {
				fprintf(stderr, "Invalid idle time override\n");
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
		} else if(strcmp(SLEEP_NOW, argv[i]) == 0) {
			sleepNow = 1;
		} else if(strcmp(DELAY_SLEEP, argv[i]) == 0) {
			if((i+1) >= argc) {
				fprintf(stderr, "Invalid delay time override\n");
				return -1;
			}
			tempDelay = strtol(argv[i+1], &ptr, 10);
			if(ptr[0] != '\0') {
				fprintf(stderr, "Invalid delay time override \"%s\"\n", argv[i+1]);
				fprintf(stderr, "\"%s\"\n", ptr);
				return -1;
			}
			if(tempDelay > MAX_DELAY_TIME) {
				fprintf(stderr, "Invalid delay time.  Max is %d\n", MAX_DELAY_TIME);
				return -1;
			}
			sleepDelay = tempDelay;
			i++;	
		} else {
			display = argv[i];
		}
	}
	//try to become root
	if(becomeRoot()) {
		fprintf(stderr, "Failed to become root\n");
		return -1;
	}

	//check if server is running now
	serverPid = serverRunning();
	if(serverPid) {
		//if we are asking to just sleep now that is fine
		if(sleepNow) {
			//signal the parent
			return signalParent(serverPid);
		} else if(sleepDelay > 0) {
			return delaySleep(sleepDelay, serverPid);
		}
		//not asking to sleep immediately and server is running
		//refuse to do anything
		fprintf(stderr, "Server is currently running\n");
		return -1;
	} else {
		if(sleepNow) {
			fprintf(stderr, "Daemon is not running, sleep now not allowed\n");
			return -1;
		}
	}
	
	return mainRoutine();
}

int mainRoutine() {
	unsigned long idleTime = 0;
	unsigned int sshConns = 0;

	if(forground == 0) {
		pid_t pid = background();
		if(pid) {
			//we are the parent, bail
			return 0;
		}
	}

	signal(SIGUSR1, sigHandler);
	signal(SIGUSR2, sigHandler);
	if(dropMutex(getpid())) {
		fprintf(stderr, "Failed to drop mutex\n");
		return -1;
	}
	LOG("Dropped mutex\n");

	while(1) {
		idleTime = 0;
		sshConns = 0;
		if(getIdleTime(&idleTime)) {
			LOG("Failed to get idleTime\n");
			sleep(120);
			continue;
		}
		if(activeSSHSessions(&sshConns, port)) {
			LOG("Failed to get ssh connections\n");
			return -1;
		}
		if(((idleTime/1000) > idleToSleep && sleepDelay == 0)|| sleepOverride == 1) {
			if(sshConns == 0 || ignoreSsh != 0 || sleepOverride == 1) {
				if(sleepOverride == 1) {
					LOG("Sleeping due to external sleep request\n");
				}
				sleepOverride = 0;
				sleepDelay = 0;
				LOG("Sleeping\n");
				goToSleep();
				continue;
			}
		}
		sleep(SLEEP_INTERVAL);
		if(sleepDelay < SLEEP_INTERVAL) {
			sleepDelay = 0;
		} else {
			sleepDelay -= SLEEP_INTERVAL;
		}
	}
	return 0;
}


void usage(char* proggy) {
	fprintf(stdout, "%s -h\tShow usage\n", proggy);
	fprintf(stdout, "%s -l <log file>\tLog to a file\n", proggy);
	fprintf(stdout, "%s -n\t Ask to sleep right now\n", proggy);
	fprintf(stdout, "%s -f\tDo not background the process\n", proggy);
	fprintf(stdout, "%s -t <idle minutes>\tMinutess of idle before sleep\n", proggy);
	fprintf(stdout, "%s -p <ssh port>\tPort to look for SSH connections on\n", proggy);
	fprintf(stdout, "%s -d <delay>\tDelay sleep in %d minute intervals\n", proggy, (SLEEP_DELAY_INTERVAL/60));
	fprintf(stdout, "%s <display>\n", proggy);
}


pid_t background() {
	pid_t pid = fork();
	if(pid == 0) {
		//we are the child
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
		return 0;
	}
	//we are the parent
	return pid;
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

pid_t getMutex() {
	long int tpid = 0;
	char line[64];
	char *ptr = line;
	FILE* fin;
	fin = fopen(MUTEX_PATH, "r");
	if(fin == NULL) {
		return 0;
	}
	memset(line, 0, sizeof(line));
	if(fread(line, 1, sizeof(line), fin) <= 0) {
		fclose(fin);
		return 0;
	}
	fclose(fin);

	tpid = strtol(line, &ptr, 10);
	if(tpid <= 0) {
		return 0;
	}
	if(ptr[0] != '\0') {
		//file was corrupted
		return 0;
	}
	return (pid_t)tpid;
}

pid_t serverRunning() {
	char procPath[256];
	struct stat st;
	pid_t serverPid = getMutex();
	if(serverPid == 0) {
		return 0;
	}
	//check if the process is still running
	snprintf(procPath, sizeof(procPath), "/proc/%d", serverPid);
	if(stat(procPath, &st)) {
		return 0;
	}
	return serverPid;
}

int dropMutex(pid_t pid) {
	FILE* fout = fopen(MUTEX_PATH, "w+");
	if(fout == NULL) {
		return -1;
	}
	fprintf(fout, "%ld", (long int)pid);
	fclose(fout);
	return 0;
}

int signalParent(pid_t pid) {
	return kill(pid, SIGUSR1);
}


int becomeRoot() {
	if(getuid() != 0) {
		//attempt to setuid to 0
		int ret = setuid(0);
		if(ret != 0) {
			LOG("setuid failed(%d) with error %d\n", ret, errno);
		}
	}
	return 0;
}

void sigHandler(int signum) {
	if(signum == SIGUSR1) {
		LOG("Sleep now request made\n");
		sleepOverride = 1;
	} else if(signum == SIGUSR2) {
		sleepDelay += SLEEP_DELAY_INTERVAL;
		LOG("Delaying sleep due to signal %d\n", sleepDelay);
	}
}

int delaySleep(int minutes, pid_t pid) {
	int mins;
	for(mins=0; mins < (minutes*60); mins += SLEEP_DELAY_INTERVAL) {
		if(kill(pid, SIGUSR2) != 0) {
			fprintf(stderr, "Failed to signal for sleep delay\n");
			return -1;
		}
		//sleep for a bit so the signal can seep in
		usleep(SIG_SLEEP_INTERVAL);
	}
	printf("delayed sleep by %d minutes\n", mins/60);
	return 0;
}
