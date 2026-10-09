/* nxcompat: termios stubs. Horizon has no ttys. */
#include <errno.h>
#include <string.h>
#include <termios.h>

int tcgetattr(int fd, struct termios *t) { (void)fd; memset(t, 0, sizeof(*t)); errno = ENOTTY; return -1; }
int tcsetattr(int fd, int act, const struct termios *t) { (void)fd; (void)act; (void)t; errno = ENOTTY; return -1; }
int tcsendbreak(int fd, int duration) { (void)fd; (void)duration; errno = ENOTTY; return -1; }
int tcdrain(int fd) { (void)fd; errno = ENOTTY; return -1; }
int tcflush(int fd, int queue) { (void)fd; (void)queue; errno = ENOTTY; return -1; }
int tcflow(int fd, int action) { (void)fd; (void)action; errno = ENOTTY; return -1; }
speed_t cfgetispeed(const struct termios *t) { return t->c_ispeed; }
speed_t cfgetospeed(const struct termios *t) { return t->c_ospeed; }
int cfsetispeed(struct termios *t, speed_t s) { t->c_ispeed = s; return 0; }
int cfsetospeed(struct termios *t, speed_t s) { t->c_ospeed = s; return 0; }
int cfsetspeed(struct termios *t, speed_t s) { t->c_ispeed = t->c_ospeed = s; return 0; }

void cfmakeraw(struct termios *t)
{
    t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    t->c_oflag &= ~OPOST;
    t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t->c_cflag &= ~(CSIZE | PARENB);
    t->c_cflag |= CS8;
    t->c_cc[VMIN] = 1;
    t->c_cc[VTIME] = 0;
}
