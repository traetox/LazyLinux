#include <stdio.h>
#include "ssh.h"
#include "log.h"

#define PROC_NET_STAT "/proc/net/tcp"

//reads line from file, stripping newline
int readline(FILE* fin, char* line, int maxlen) {
	int i;
	int ct;
	char c;
	
	for(i=0; i < maxlen-1; i++) {
		ct = getc(fin);
		if(ct == '\n') {
			break;
		}
		if(ct == EOF) {
			if(i == 0) {
				return -1;
			}
			break;
		}
		line[i] = (char)ct;
	}
	line[i] = '\0';
	return 0;
}

int parseLine(char* line, unsigned short *dstPort, unsigned long *srcAddy) {
	long int temp;
	char* ptr = line;
	int x;
	int i;

	//walk until we hit the second colon
	x = 0;
	i = 0;
	while(line[i] != '\0') {
		if(line[i] == ':') {
			x++;
		}
		if(x==2) {
			break;
		}
		i++;
	}
	if(x != 2) {
		LOG("Never found 3rd colon\n");
		return -1;
	}
	i++;//skip the colon
	//try to grab the port
	temp = strtol(&line[++i], &ptr, 16);
	if(ptr == &line[i]) {
		LOG("couldn't get string out of %s\n", &line[i]);
		return -1;
	}
	*dstPort = (unsigned short)temp;

	//skip the space
	line = &ptr[1];
	temp = strtol(line, &ptr, 16);
	if(ptr == &line[i]) {
		LOG("couldn't get string out of %s\n", &line[i]);
		return -1;
	}
	*srcAddy = (unsigned long)temp;
	return 0;
}

int activeSSHSessions(unsigned int* count, unsigned short sshPort) {
	unsigned int cnt = 0;
	FILE* fin;
	char line[2048];
	unsigned long src;
	unsigned short port;
	if(count == NULL) {
		LOG("parameter is NULL\n");
		return -1;
	}
	fin = fopen(PROC_NET_STAT, "r");
	if(fin == NULL) {
		LOG("Failed to open proc file\n");
		return -1;
	}
	//read the title line
	if(readline(fin, line, sizeof(line))) {
		fclose(fin);
		return -1;
	}
	//start reading lines
	while(readline(fin, line, sizeof(line)) == 0) {
		if(parseLine(line, &port, &src)) {
			fclose(fin);
			return -1;
		}
		if(port == sshPort && src != 0) {
			(*count)++;
		}
	}
		
	fclose(fin);
	return 0;
}
