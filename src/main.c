#include "display-driver.h"
#include "data-ops.h"

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
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
static speed_t baud = B9600;

enum AppMode {
	AM_STDIN = 0x00,
	AM_TTY = 0x01,
	AM_FILE = 0x02,
	AM_UDP = 0x04,
	AM_TCP = 0x08
} appmode = AM_STDIN;

void printUsage(int argc, char* const argv[])
{
	assert(argc >= 0);

	printf("Usage:\n"
	       "%1$s\n"
	       "%1$s -f <filename>\n"
	       "%1$s -u <host> | -t <host> -p <port>\n"
	       "%1$s -s <serial> [-b <baud>]\n"
	       "%1$s -h\n"
	       "%1$s -V\n"
	       "\n"
	       "Calling with no options will read from stdin. Alternatively -f, -u, or -t\n"
	       "can be supplied to read from a file or from a UDP socket. To read from a\n"
	       "serial port, specify the port with -s and the speed with -b.\n"
	       "\n"
	       "Options:\n"
	       "-f <filename>	A filename to read json env data from\n"
	       "-u <host>	A hostname or ip address to connect to via UDP. Cannot\n"
	       "		be used with the -f option\n"
	       "-t <host>	A hostname or ip address to connect to via TCP. Cannot\n"
	       "		be used with the -f option\n"
	       "-p <port>	A port number to connect to on the remote port. Must be\n"
	       "		used with the -u or -t option\n"
	       "-s <serial>	Special file path for a serial device\n"
	       "-b <baud>	Baud rate for serial connection (default: 9600)\n"
	       "-h		Print usage message, then exit\n"
	       "-V		Print version information, then exit\n",
	       argv[0]);
}

void parseOptions(int argc, char* const argv[])
{
	int c;

	while ((c = getopt(argc, argv, "f:u:t:p:s:b:hV")) != -1) {
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

		case 't':
			if (appmode == AM_FILE) {
				fprintf(stderr, "Error: "
					"t cannot be used with f\n");
				exit(1);
			}

			strncpy(ipbuffer, optarg, APP_BUFFERSIZE);
			appmode = AM_TCP;
			break;

		case 'p':
			if (appmode == AM_FILE) {
				fprintf(stderr, "Error: "
					"p cannot be used with f\n");
				exit(1);
			}

			strncpy(portbuffer, optarg, APP_BUFFERSIZE);
			break;

		case 's':
			/* Can only set TTY mode if no other major
			 * mode is set */
			if (appmode & (AM_FILE | AM_UDP)) {
				fprintf(stderr, "Error: "
					"%c cannot be used with u or "
					"f\n", c);
				exit(1);
			}

			/* Store the TTY special path in the
			 * filebuffer */
			strncpy(filebuffer, optarg, APP_BUFFERSIZE);
			appmode = AM_TTY;
			break;

		case 'b':
			/* Can only set baud in TTY mode */
			if (appmode & (AM_FILE | AM_UDP)) {
				fprintf(stderr, "Error: "
					"%c cannot be used with u or "
					"f\n", c);
				exit(1);
			}

			baud = strtoll(optarg, NULL, 10);

			/* Check that baud is non-zero after str to
			 * int conversion */
			if (baud == 0) {
				fprintf(stderr, "Error: "
					"Invalid baud %s\n", optarg);
				exit(1);
			}

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

int tcpConnect()
{
	int s; /* Socket */
	int error;

	struct addrinfo hints = {
		.ai_flags = AI_PASSIVE,
		.ai_family = 0,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
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

int ttyConnect()
{
	int ret;
	struct termios ts;

	fd = open(filebuffer, APP_BUFFERSIZE, O_RDWR);

	if (fd < 0) {
		perror("ERROR: When opening TTY: ");

		return -1;
	}

	ret = tcgetattr(fd, &ts);

	if (ret < 0) {
		perror("ERROR: When getting TTY properties: ");
	}

	ret = cfsetspeed(&ts, baud);

	if (ret < 0) {
		perror("ERROR: When setting TTY speed: ");

		return ret;
	}

	/* Set baud and turn off flow control */
	ts.c_cflag &= ~CRTSCTS;
	ts.c_cflag |= CREAD | CLOCAL;
	ts.c_iflag &= ~(IXON | IXOFF | IXANY);
	ts.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	/* ts.c_iflag |= ICRNL; */

	ret = tcsetattr(fd, TCSANOW, &ts);

	if (ret < 0) {
		perror("ERROR: Setting TTY settings: ");

		return ret;
	}

	return 0;
}

void closeDescriptor()
{
	close(fd);
}

void signalHandler(int sig)
{
	metric_form_exit();
	closeDescriptor();
	printf("Received signal %d: %s\n", sig, strsignal(sig));

	if (sig == SIGINT || sig == SIGTERM) {
		return;
	}

	exit(1);
}

int runNcursesInterface(int fd)
{
	int ret;

	/* Use ncurses display mode (none others currently
	 * available */
	struct metric_form *m;

	m = ncursesCFG(fd); /* Use default window config */

	ret = metric_form_init(m);

	ncursesFreeMetric(); /* The metric data must be freed */

	return ret;
}

int main(int argc, char* const argv[])
{
	int ret;

	/* Register signal handlers */
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGABRT, signalHandler);
	signal(SIGPIPE, signalHandler);

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
	case AM_TCP:
		if ((fd = tcpConnect()) < 0) {
			fprintf(stderr, "Failed to open %s:%s\n",
				ipbuffer, portbuffer);
			return 1;
		}
		break;
	case AM_TTY:
		if (ttyConnect() < 0) {
			fprintf(stderr, "Failed to open %s\n",
				filebuffer);
			return 1;
		}

		break;
	}

	ret = runNcursesInterface(fd);

	return ret;
}
