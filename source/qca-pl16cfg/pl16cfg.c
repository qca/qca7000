/*
 * Copyright (c) 2012 Qualcomm Atheros
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

char *const slattach_cmd[] = {
	"slattach",
	"-p",
	"qca",
	"-s",
	"115200",
	"/dev/ttySP0",
	NULL
};

char *const rmmod_qcaspi_cmd[] = {
	"rmmod",
	"qcaspi",
	NULL
};

char *const rmmod_qcauart_cmd[] = {
	"rmmod",
	"qcauart",
	NULL
};

char *const insmod_qcaspi_cmd[] = {
	"insmod",
	"/root/qcaspi.ko",
	NULL
};

char *const insmod_qcauart_cmd[] = {
	"insmod",
	"/root/qcauart.ko",
	NULL
};

char *const ifconfig_qca0_up_cmd[] = {
	"ifconfig",
	"qca0",
	"up",
	NULL
};

char *const ifconfig_qca0_down_cmd[] = {
	"ifconfig",
	"qca0",
	"down",
	NULL
};

char *const brctl_addif_br0_qca0_cmd[] = {
	"brctl",
	"addif",
	"br0",
	"qca0",
	NULL
};

char *const brctl_delif_br0_qca0_cmd[] = {
	"brctl",
	"delif",
	"br0",
	"qca0",
	NULL
};

#define DEFAULT_PORT 4321

#define MODE_DISABLED 0
#define MODE_SPI 1
#define MODE_UART 2
#define MODE_RESET_NEEDED 3

struct pl16cfg {
	int mode;

	int clientsock;
	int listensock;
	uint16_t port;
	struct sockaddr_in localaddr;
	struct sockaddr_in remoteaddr;

	char rxbuf[1024];
	int rxbuf_used;

	pid_t slattach_pid;
};

void usage(void);

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-d] [-p port] [-s score_limit]\n",
	    __progname);

	exit(1);
}

/* return pid on success, -1 on error */
pid_t
execute_cmd(const char *file, char *const argv[])
{
	pid_t pid;

	pid = fork();
	if (pid == -1) {
		perror("fork");
	}

	/* parent, possibly -1 */
	if (pid)
		return pid;

	/* child */
	if (execvp(file, argv) == -1) {
		perror("execvp");
		_exit(1);
	}

	/* parent has already returned, if child somehow manages to get here, just exit */
	_exit(1);
}

int
xsend(int sock, const void *msg, size_t len, int flags)
{
	size_t sent = 0;

	while (sent < len) {
		ssize_t ret = send(sock, msg + sent, len - sent, flags);
		if (ret == -1) {
			perror("send");
			return -1;
		}

		sent += ret;
	}

	return 0;
}

int
enable_spi(struct pl16cfg *pl16cfg)
{
	int status;
	pid_t pid;

	/* if everything goes OK, this is updated */
	pl16cfg->mode = MODE_RESET_NEEDED;

	pid = execute_cmd(insmod_qcaspi_cmd[0], insmod_qcaspi_cmd);
	if (pid == -1)
		return -1;

	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("insmod qcaspi exit status: %d\n", status);
	if (status)
		return -1;

	pid = execute_cmd(ifconfig_qca0_up_cmd[0], ifconfig_qca0_up_cmd);
	if (pid == -1)
		return -1;
	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("ifconfig qca up exit status: %d\n", status);
	if (status)
		return -1;

	pid = execute_cmd(brctl_addif_br0_qca0_cmd[0], brctl_addif_br0_qca0_cmd);
	if (pid == -1)
		return -1;
	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("brctl addif br0 qca0 exit status: %d\n", status);
	if (status)
		return -1;

	/* everything started OK. */
	pl16cfg->mode = MODE_SPI;

	return 0;
}

int
disable_spi(struct pl16cfg *pl16cfg)
{
	int status;
	pid_t pid;

	/* if everything goes OK, this is updated */
	pl16cfg->mode = MODE_RESET_NEEDED;

	pid = execute_cmd(brctl_delif_br0_qca0_cmd[0], brctl_delif_br0_qca0_cmd);
	if (pid == -1)
		return -1;
	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("brctl delif br0 qca0 exit status: %d\n", status);
	if (status)
		return -1;

	pid = execute_cmd(rmmod_qcaspi_cmd[0], rmmod_qcaspi_cmd);
	if (pid == -1)
		return -1;

	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("rmmod qcaspi exit status: %d\n", status);
	if (status)
		return -1;

	pl16cfg->mode = MODE_DISABLED;

	return 0;
}

int
enable_uart(struct pl16cfg *pl16cfg)
{
	int status;
	pid_t pid;

	/* if everything goes OK, this is updated */
	pl16cfg->mode = MODE_RESET_NEEDED;

	pid = execute_cmd(insmod_qcauart_cmd[0], insmod_qcauart_cmd);
	if (pid == -1)
		return -1;

	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("insmod qcauart exit status: %d\n", status);
	if (status)
		return -1;

	pid = execute_cmd(ifconfig_qca0_up_cmd[0], ifconfig_qca0_up_cmd);
	if (pid == -1)
		return -1;
	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("ifconfig qca up exit status: %d\n", status);
	if (status)
		return -1;

	pid = execute_cmd(brctl_addif_br0_qca0_cmd[0], brctl_addif_br0_qca0_cmd);
	if (pid == -1)
		return -1;
	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("brctl addif br0 qca0 exit status: %d\n", status);
	if (status)
		return -1;

	pid = execute_cmd(slattach_cmd[0], slattach_cmd);
	if (pid == -1)
		return -1;

	/* wait for 2 seconds, and then check if it is still running, if so assume success */
	status = 0;
	if (waitpid(pid, &status, WNOHANG) == -1) {
		perror("wait");
		exit(1);
	}
	if (status) {
		printf("slattach exited early: %d\n", status);
		return -1;
	}

	/* save PID for killing later */
	pl16cfg->slattach_pid = pid;

	pl16cfg->mode = MODE_UART;

	return 0;
}

int
disable_uart(struct pl16cfg *pl16cfg)
{
	int status;
	pid_t pid;

	/* if everything goes OK, this is updated */
	pl16cfg->mode = MODE_RESET_NEEDED;

	/*
	 * XXX: what about early termination of slattach?
	 * should probably catch sigchld.
	 */
	if (pl16cfg->slattach_pid != -1) {
		if (kill(pl16cfg->slattach_pid, SIGTERM) == -1) {
			perror("kill");
			return -1;
		}

		/* wait for exit */
		waitpid(pl16cfg->slattach_pid, &status, 0);
		pl16cfg->slattach_pid = -1;
	}

	pid = execute_cmd(rmmod_qcauart_cmd[0], rmmod_qcauart_cmd);
	if (pid == -1)
		return -1;

	if (waitpid(pid, &status, 0) == -1) {
		perror("wait");
		exit(1);
	}
	printf("rmmod qcauart exit status: %d\n", status);
	if (status)
		return -1;

	pl16cfg->mode = MODE_DISABLED;

	return 0;
}

int
change_mode_cmd(struct pl16cfg *pl16cfg, int new_mode)
{
	switch (pl16cfg->mode) {
	case MODE_RESET_NEEDED:
		/* user is not allowed to do anything except reset board. */
		break;

	case MODE_DISABLED:
		switch (new_mode) {
		case MODE_DISABLED:
			/* nothing to do */
			break;

		case MODE_SPI:
			enable_spi(pl16cfg);
			break;

		case MODE_UART:
			enable_uart(pl16cfg);
			break;
		}
		break;

	case MODE_SPI:
		switch (new_mode) {
		case MODE_DISABLED:
			disable_spi(pl16cfg);
			break;

		case MODE_SPI:
			/* nothing to do */
			break;

		case MODE_UART:
			if (!disable_spi(pl16cfg)) {
				enable_uart(pl16cfg);
			}
			break;
		}
		break;

	case MODE_UART:
		switch (new_mode) {
		case MODE_DISABLED:
			disable_uart(pl16cfg);
			break;

		case MODE_SPI:
			if (!disable_uart(pl16cfg)) {
				enable_spi(pl16cfg);
			}
			break;

		case MODE_UART:
			/* nothing to do */
			break;
		}
		break;
	}

	return 0;
}

int
mode_cmd(struct pl16cfg *pl16cfg)
{
	char *mode;

	switch (pl16cfg->mode) {
	case MODE_DISABLED:
		mode = "DISABLED\n";
		break;

	case MODE_SPI:
		mode = "SPI\n";
		break;

	case MODE_UART:
		mode = "UART\n";
		break;

	case MODE_RESET_NEEDED:
		mode = "RESET_NEEDED\n";
		break;

	default:
		mode = "UNKNOWN\n";
	}

	if (xsend(pl16cfg->clientsock, mode, strlen(mode), 0)) {
		close(pl16cfg->clientsock);
		pl16cfg->clientsock = -1;
	}

	return 0;
}

int
handle_command(struct pl16cfg *pl16cfg)
{
	char *p;
	char *buf = pl16cfg->rxbuf;

	p = strchr(buf, '\n');
	if (p == NULL)
		return 0;

	*p = '\0';

	if (!strcmp(buf, "mode")) {
		mode_cmd(pl16cfg);
	} else if (!strcmp(buf, "spi")) {
		change_mode_cmd(pl16cfg, MODE_SPI);
	} else if (!strcmp(buf, "uart")) {
		change_mode_cmd(pl16cfg, MODE_UART);
	} else if (!strcmp(buf, "disable")) {
		change_mode_cmd(pl16cfg, MODE_DISABLED);
	} else {
		printf("unknown command received, ignored\n");
	}

	return p - buf + 1;
}

int
main(int argc, char **argv)
{
	int yes;
	int ch;
	int d_flag;
	struct pl16cfg pl16cfg;

	srand(time(NULL));

	memset(&pl16cfg, 0, sizeof(pl16cfg));

	/* board boots in SPI mode */
	pl16cfg.mode = MODE_SPI;
	pl16cfg.port = DEFAULT_PORT;
	pl16cfg.slattach_pid = -1;

	d_flag = 0;

	while ((ch = getopt(argc, argv, "dp:")) != -1) {
		switch (ch) {
		case 'd':
			d_flag = 1;
			break;
		case 'p':
			pl16cfg.port = atoi(optarg);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	pl16cfg.listensock = socket(AF_INET, SOCK_STREAM, 0);
	if (pl16cfg.listensock == -1) {
		perror("socket");
		exit(1);
	}

	yes = 1;
	if (setsockopt(pl16cfg.listensock, SOL_SOCKET, SO_REUSEADDR, &yes,
	    sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	pl16cfg.localaddr.sin_family = AF_INET;
	pl16cfg.localaddr.sin_port = htons(pl16cfg.port);
	pl16cfg.localaddr.sin_addr.s_addr = INADDR_ANY;
	memset(&(pl16cfg.localaddr.sin_zero), '\0', 8);

	if (bind(pl16cfg.listensock, (struct sockaddr *) &pl16cfg.localaddr,
	    sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(pl16cfg.listensock, 0) == -1) {
		perror("listen");
		exit(1);
	}

	if (d_flag) {
		daemon(0, 0);
	}

	socklen_t addrlen = sizeof(pl16cfg.remoteaddr);
	int nbytes;
	while (1) {
		/* accept */
		pl16cfg.clientsock = accept(pl16cfg.listensock, (struct sockaddr *) &pl16cfg.remoteaddr, &addrlen);
		if (pl16cfg.clientsock == -1) {
			perror("accept");
			exit(1);
		}

		while (1) {
			/* read */
			if (pl16cfg.rxbuf_used == sizeof(pl16cfg.rxbuf)) {
				/* buffer jammed, flush it */
				memset(pl16cfg.rxbuf, 0, sizeof(pl16cfg.rxbuf));
				pl16cfg.rxbuf_used = 0;
				printf("buffer overflow, emptied.\n");
			}

			/* leave space for null terminator */
			nbytes = recv(pl16cfg.clientsock, pl16cfg.rxbuf + pl16cfg.rxbuf_used,
			    sizeof(pl16cfg.rxbuf) - pl16cfg.rxbuf_used - 1, 0);

			if (nbytes < 0) {
				perror("recv");
				break;
			}
			printf("received %d bytes\n", nbytes);
			pl16cfg.rxbuf_used += nbytes;
			printf("rxbuf_used: %d\n", pl16cfg.rxbuf_used);
			pl16cfg.rxbuf[pl16cfg.rxbuf_used] = '\0';

			/* handle all commands */
			for (;;) {
				int consumed = handle_command(&pl16cfg);
				if (pl16cfg.clientsock == -1) {
					/* error with client */
					break;
				}
				/* include null terminator */
				memmove(pl16cfg.rxbuf, pl16cfg.rxbuf + consumed, pl16cfg.rxbuf_used - consumed + 1);
				pl16cfg.rxbuf_used -= consumed;
				printf("handle_commands, rxbuf_used: %d\n", pl16cfg.rxbuf_used);

				/* no commands left */
				if (consumed <= 0)
					break;
			}

			if (nbytes == 0) {
				/* user disconnected */
				break;
			}
		}

		close(pl16cfg.clientsock);
	}

	return 0;
}
