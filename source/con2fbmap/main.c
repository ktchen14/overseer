#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/sysmacros.h>

int main(int argc, char* argv[]) {
  struct fb_con2fbmap con2fbmap;
  struct stat sb;
  int file;

  char *progname = strrchr(argv[0], '/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  if (argc != 3) {
    fprintf(stderr, "usage: %s /path/to/fb /path/to/console\n", progname); 
    return 1;
  }

  if (stat(argv[1], &sb) == -1) {
    fprintf(stderr, "%s: %s: %s\n", progname, argv[1], strerror(errno));
    return 1;
  }

  if (!S_ISCHR(sb.st_mode)) {
    fprintf(stderr, "%s: %s: Character device required.\n", progname, argv[1]);
    return 1;
  }

  /* From https://www.kernel.org/doc/Documentation/fb/framebuffer.txt:
   *
   *   The frame buffer device is a character device using major 29; the minor
   *   specifies the frame buffer number.
   *
   *   By convention, the following device nodes are used (numbers indicate the
   *   device minor numbers):
   *
   *   0 = /dev/fb0   First frame buffer
   *   1 = /dev/fb1   Second frame buffer
   *       ...
   *  31 = /dev/fb31  32nd frame buffer
   */

  if (major(sb.st_rdev) != 29) {
    fprintf(stderr, "%s: %s: Framebuffer device required.\n", progname, argv[1]);
    return 1;
  }

  con2fbmap.framebuffer = minor(sb.st_rdev);
  fbPath = argv[1];

  if (stat(argv[2], &sb)) {
    fprintf(stderr, "%s: %s: %s\n", progname, argv[2], strerror(errno));
    return 1;
  }

  /* From https://www.kernel.org/doc/Documentation/admin-guide/devices.txt
   *   4 char  TTY devices
   *             0 = /dev/tty0     Current virtual console
   *             1 = /dev/tty1     First virtual console
   *               ...
   *            63 = /dev/tty63    63rd virtual console
   *            64 = /dev/ttyS0    First UART serial port
   *               ...
   *           255 = /dev/ttyS191  192nd UART serial port
   *
   *           UART serial ports refer to 8250/16450/16550 series devices.
   */

  if (!S_ISCHR(sb.st_mode)) {
    fprintf(stderr, "%s: %s must be character device\n", progname, argv[2]);
    return 1;
  }

  if (major(sb.st_rdev) != 29) {
    fprintf(stderr, "%s: %s: Console device required.\n", progname, argv[1]);
    return 1;
  }

  if ((con2fbmap.console = minor(sb.st_rdev)) >= 64) {
    fprintf(stderr, "%s: %s: Console device required.\n", progname, argv[1]);
    return 1;
  }

  if ((file = open(fbPath, O_RDWR)) == -1) {
    fprintf(stderr, "%s: %s: %s\n", progname, fbPath, strerror(errno));
    goto except_open;
  }

  if (ioctl(file, FBIOPUT_CON2FBMAP, &con2fbmap) == -1) {
    fprintf(stderr, "%s: Cannot set console mapping\n", progname);
    goto except_ioctl;
  }

  close(file);
  return 0;

except_ioctl:
  close(file);

except_open:
  return 1;
}
