#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>


struct termios original_terminal;

void disableRawMode() {
    // we want to disable raw mode when exiting the program in order to keep user with their Terminal "stable".
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal);
}


void enableRawMode() {
    /*By default your terminal starts in canonical mode, also called cooked mode.
    In this mode, keyboard input is only sent to your program when the user presses Enter.

    But it does not work well for programs with more complex user interfaces, like text editors. 
    We want to process each keypress as it comes in, so we can respond to it immediately. That's called
    the raw mode.
    */

    // Update terminal's attributes to turn off echoing
    /*The ECHO feature causes each key you type to be printed to the terminal, 
    so you can see what you’re typing. This is useful in canonical mode, but really gets 
    in the way when we are trying to carefully render a user interface in raw mode. 
    So we turn it off.
    */

    tcgetattr(STDIN_FILENO, &original_terminal);
    // restore to the original value
    atexit(disableRawMode);

    struct termios raw = original_terminal;
    // update echo flag
    raw.c_lflag &= ~(ECHO);
    tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw);
}


int main(int argc, char const *argv[]) {
    enableRawMode();

    char c;
    while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}
