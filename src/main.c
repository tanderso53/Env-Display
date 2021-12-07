#include "display-driver.h"

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#ifndef APP_BUFFERSIZE
#define APP_BUFFERSIZE 128
#endif /* #ifndef APP_BUFFERSIZE */

#ifndef VM_VERSION
#define VM_VERSION "Unknown"
#endif /* #ifndef VM_VERSION */

extern char* optarg;
extern int optind, opterr, optopt;

static char filebuffer[APP_BUFFERSIZE];
static char ipbuffer[APP_BUFFERSIZE];
static char portbuffer[APP_BUFFERSIZE];
static int fd;

enum AppMode {
	AM_STDIN,
	AM_FILE,
	AM_UDP
} appmode = AM_STDIN;

void printUsage(int argc, char* const argv[])
{
	assert(argc >= 0);

	printf("Usage:\n"
	       "%1$s\n"
	       "%1$s -f <filename>\n"
	       "%1$s -u <host> -p <port>\n"
	       "\n"
	       "Calling with no options will read from stdin. Alternatively -f or -u\n"
	       "can be supplied to read from a file or from a UDP socket.\n"
	       "\n"
	       "Options:\n"
	       "-f <filename>	A filename to read json env data from\n"
	       "-u <host>	A hostname or ip address to connect to via UDP. Cannot\n"
	       "		be used with the -f option\n"
	       "-p <port>	A port number to connect to on the remote port. Must be\n"
	       "		used with the -u option\n"
	       "-h		Print usage message, then exit\n"
	       "-V		Print version information, then exit\n",
	       argv[0]);
}

void parseOptions(int argc, char* const argv[])
{
	int c;

	while ((c = getopt(argc, argv, "f:u:p:hV")) != -1) {
		switch(c) {

		case 'f':
			if (appmode != AM_STDIN) {
				fprintf(stderr, "Error: "
					"f cannot be used with u or"
					" p\n");
				exit(1);
			}

			strncpy(filebuffer, optarg, APP_BUFFERSIZE);
			appmode = AM_FILE;
			break;

		case 'u':
			if (appmode == AM_FILE) {
				fprintf(stderr, "Error: "
					"u cannot be used with f\n");
				exit(1);
			}

			strncpy(ipbuffer, optarg, APP_BUFFERSIZE);
			appmode = AM_UDP;
			break;

		case 'p':
			if (appmode == AM_FILE) {
				fprintf(stderr, "Error: "
					"p cannot be used with f\n");
				exit(1);
			}

			strncpy(portbuffer, optarg, APP_BUFFERSIZE);
			break;

		case 'h':
			/* Print usage and exit */
			printUsage(argc, argv);
			exit(0);

		case 'V':
			/* Print fersion and exit */
			printf("%s\n", VM_VERSION);
			exit(0);

		case '?':
			printUsage(argc, argv);
			exit(1);

		default:
			break;
		}
	}
}

int remoteConnect()
{
	int s; /* Socket */
	int error;

	struct addrinfo hints = {
		.ai_flags = AI_PASSIVE,
		.ai_family = 0,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = IPPROTO_UDP,
		.ai_addrlen = 0,
		.ai_canonname = NULL,
		.ai_addr = NULL,
		.ai_next = NULL
	};

	struct addrinfo* sockai;
	struct addrinfo* ai_iter;

	if ((error = getaddrinfo(ipbuffer, portbuffer, &hints, &sockai)) != 0) {
		fprintf(stderr, "Failed to look up address %s:%s with code %d: "
			"%s", ipbuffer, portbuffer, error, gai_strerror(error));
		return -1;
	}

	assert(sockai);
	ai_iter = sockai;

	do {
		if ((s = socket(ai_iter->ai_family, ai_iter->ai_socktype,
				ai_iter->ai_protocol)) < 0) {
			continue;
		}

		if (connect(s, ai_iter->ai_addr, ai_iter->ai_addrlen) == 0) {
			break;
		}

		s = -1;
	}
	while ((ai_iter = ai_iter->ai_next) != NULL);

	freeaddrinfo(sockai);

	return s;
}

void closeDescriptor()
{
	close(fd);
}

void signalHandler(int sig)
{
	formExit();
	closeDescriptor();
	printf("Received signal %d: %s\n", sig, strsignal(sig));

	if (sig == SIGINT || sig == SIGTERM) {
		exit(0);
	}
	else if (sig == SIGWINCH) {
		fprintf(stderr, "Window cannot be resized..."
			"exiting\n");
		exit(1);
	}

	exit(1);
}

int main(int argc, char* const argv[])
{
	/* Register signal handlers */
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGABRT, signalHandler);
	signal(SIGPIPE, signalHandler);
	signal(SIGWINCH, signalHandler);

	/* Read options */
	parseOptions(argc, argv);

	/* Open file discriptor */
	switch (appmode) {
	case AM_STDIN:
		if ((fd = open("/dev/stdin", O_RDWR)) < 0) {
			perror("Failed to open stdin: ");
			return 1;
		}

		break;
	case AM_FILE:
		if ((fd = open(filebuffer, O_RDWR)) < 0) {
			fprintf(stderr, "Failed to open %s: %s\n",
				filebuffer, strerror(errno));
			return 1;
		}

		break;
	case AM_UDP:
		if ((fd = remoteConnect()) < 0) {
			fprintf(stderr, "Failed to open %s:%s\n",
				ipbuffer, portbuffer);
			return 1;
		}

		/* Attempt to write to remote to start data flow */
		if (write(fd, "hello\n", 6) < 0) {
			fprintf(stderr, "Failed to write to address %s:%s:"
				" %s\n", ipbuffer, portbuffer,
				strerror(errno));
			return 1;
		}
		break;
	}

	return formRun(fd);
}
