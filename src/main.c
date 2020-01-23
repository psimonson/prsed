#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

static struct termios orig_term;

/* Disable raw mode.
 */
static void disable_raw()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}
/* Enable raw mode.
 */
void enable_raw()
{
	struct termios raw;
	tcgetattr(STDIN_FILENO, &orig_term);
	atexit(disable_raw);

	/* setup raw mode */
	raw = orig_term;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
/* Simple Text Editor (prsed).
 */
int main()
{
	enable_raw();
	while(1) {
		char c;
		if(read(STDIN_FILENO, &c, 1) != 1)
			continue;
		if(iscntrl(c)) {
			printf("%x\r\n", c);
		} else {
			printf("%x (%c)\r\n", c, c);
		}
		if(c == 'q') break;
	}
	return 0;
}
