#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

int xbee_open(char *uart)
{
        struct termios tio;
        int fd = open(uart, O_RDWR | O_NOCTTY | O_SYNC);
        assert(fd >= 0);

        tcgetattr(fd, &tio);
        cfmakeraw(&tio);
        tio.c_cflag |= CS8 | CLOCAL | CREAD | B9600;
        tio.c_iflag |= IGNPAR;
        tcsetattr(fd, TCSANOW, &tio);
        tcflush(fd, TCIFLUSH);

        return fd;
}

int xbee_read(int fd, char *buf, int len)
{
        fd_set rfds;
        struct timeval tval;
        int timeout = 3;
        int ret;
        int x = 0;

        while (timeout) {
                tval.tv_sec = 0;
                tval.tv_usec = 100000;
                FD_ZERO(&rfds);
                FD_SET(fd, &rfds);
                select(fd + 1, &rfds, NULL, NULL, &tval);
                if(!FD_ISSET(fd, &rfds))
                {
                        timeout--;
                        continue;
                }
                if(len - x <= 0)
                        return x;

                ret = read(fd, &buf[x], len - x);
                x += ret;
        }

        return x;
}

int check_ok(int fd)
{
        char buf[3];
        xbee_read(fd, buf, 3);
        if (strncmp(buf, "OK\r", 3) == 0) {
                return 1;
        }
        return 0;
}

int xbee_get_version(int fd, char *buf, int len)
{
        assert(write(fd, "ATVL\r", 5) == 5);
        tcdrain(fd);
        usleep(250000);

        return xbee_read(fd, buf, len);
}

int xbee_command_mode(int fd) {
        /* Send +++ and wait 1 second of silence to 
         * enter command mode normally */
        assert(write(fd, "+++", 3) == 3);
        tcdrain(fd);
        usleep(1250000);
        if(check_ok(fd)) return 1;

        /* If the module is in a different mode, assert serial break for 6
         * seconds to force it back to 9600 and respond with OK */
        ioctl(fd, TIOCSBRK, NULL);
        usleep(6250000);
        ioctl(fd, TIOCCBRK, NULL);
        if(check_ok(fd)) return 1;

        return 0;
}

void xbee_command_exit(int fd) {
        assert(write(fd, "ATCN\r", 5) == 5);
        tcdrain(fd);
}

int main(int argc, char **argv)
{
	int fd, ret;
	char buf[256];
	bzero(buf, 256);

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <uart>\n", argv[0]);
		return -1;
	}

	fd = xbee_open(argv[1]);
	ret = xbee_command_mode(fd);
	if(!ret) {
		puts("Module not detected\n");
		return 1;
	}
	puts("Module detected\n");
	ret = xbee_get_version(fd, buf, 255);
	assert(ret != 0);

	/* Replace \r with \n to make the version printable */
	for (int i = 0; i < ret; i++)
		if (buf[i] == '\r')
			buf[i] = '\n';

	puts(buf);

	return 0;
}
