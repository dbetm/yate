/*** includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>


/* defines */

/* The CTRL_KEY macro bitwise-ANDs a character with the value 00011111, in binary. 
(In C, you generally specify bitmasks using hexadecimal, since C doesn’t have binary literals)

It sets the upper 3 bits of the character to 0. This mirrors what the Ctrl key does in the terminal: 
it strips bits 5 and 6 from whatever key you press in combination with Ctrl, and sends that.
Example:
    'q' = 113 in decimal, 1110001 in binary
    113 & 31
    01110001 & 
    00011111
=   00010001
= 17 (which is the equivalent to Ctrl+q)
*/
#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/
struct termios original_terminal;

/*** terminal ***/
void die(const char *s) {
    /*Most C library functions that fail will set the global errno variable to indicate what the error was. 
    perror() looks at the global errno variable and prints a descriptive error message for it.

    It also prints the string given to it before it prints the error message
    */
    perror(s);
    exit(1);
}


void disableRawMode() {
    // we want to disable raw mode when exiting the program in order to keep user with their Terminal "stable".
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal) == -1) {
        die("tcsetattr");
    }
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

    There is an ICANON flag that allows us to turn off canonical mode. This means we will finally be 
    reading input byte-by-byte, instead of line-by-line.
    */

    if(tcgetattr(STDIN_FILENO, &original_terminal) == -1) die("tcsetattr");
    // restore to the original value
    atexit(disableRawMode);

    struct termios raw = original_terminal;
    // Update ECHO and CANONICAL mode flags
    /* We want too to handle some key combinations:
        1. Turn off ctrl+c (SIGINT) and ctrl+z (SIGTSTP) signals, which terminates or suspend the program.
        2. Turn off Ctrl-S and Ctrl-Q which are used for software flow control.
            Ctrl-S stops data from being transmitted to the terminal until you press Ctrl-Q.
            This originates in the days when you might want to pause the transmission of data to let
            a device like a printer catch up. We can use IXON flag.
        3. Disable Ctrl-V. When you type Ctrl-V, the terminal waits for you to type another character and
            then sends that character literally. For example, before we disabled Ctrl-C, you might’ve been
            able to type Ctrl-V and then Ctrl-C to input a 3 byte. We can turn off this feature using
            the IEXTEN flag.
        4. Fix CTRL-M. Ctrl-M is weird: it’s being read as 10, when we expect it to be read as 13, since it
            is the 13th letter of the alphabet, and Ctrl-J already produces a 10. What else produces 10?
            The Enter key does. ICRNL comes from <termios.h>. The I stands for “input flag”, CR stands for 
            “carriage return”, and NL stands for “new line”.
            It turns out that the terminal is helpfully translating any carriage returns (13, '\r') inputted
            by the user into newlines (10, '\n').
        5. Turn off all output processing. The terminal requires both of these characters in order to start a
            new line of text. The carriage return moves the cursor back to the beginning of the current line,
            and the newline moves the cursor down a line (these operations originated in the days of typewriters and teletypes).
            We will turn off all output processing features by turning off the OPOST flag. In practice,
            the "\n" to "\r\n" translation is likely the only output processing feature turned on by default.
        6. Miscellaneous flags. BRKINT, INPCK, ISTRIP, and CS8 all come from <termios.h>. This step probably
            won’t have any observable effect for you, because these flags are either already turned off,
            or they don’t really apply to modern terminal emulators.
            - When BRKINT is turned on, a break condition will cause a SIGINT signal to be sent to the program, like pressing Ctrl-C.
            - INPCK enables parity checking, which doesn’t seem to apply to modern terminal emulators.
            - ISTRIP causes the 8th bit of each input byte to be stripped, meaning it will set it to 0. This is probably already turned off.
            - CS8 is not a flag, it is a bit mask with multiple bits, which we set using the bitwise-OR (|) operator unlike all the flags we are turning off.
                It sets the character size (CS) to 8 bits per byte.
    */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // local flags
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP); // input flags
    raw.c_oflag &= ~(OPOST); // output flags
    raw.c_cflag |= (CS8); // control flags
    raw.c_cc[VMIN] = 0; // The VMIN value sets the minimum number of bytes of input needed before read() can return
    raw.c_cc[VTIME] = 1; // The VTIME value sets the maximum amount of time to wait before read() returns. It is in tenths of a second, or 100 milliseconds.

    if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
    /* Wait for one keypress, and return it */
    int nread;
    char c;

    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    return c;
}

/** output ***/
void editorRefreshScreen() {
    /*The 4 in our write() call means we are writing 4 bytes out to the terminal. 
    The first byte is \x1b, which is the escape character, or 27 in decimal.

    We are writing an escape sequence to the terminal. Escape sequences always start with an escape character
    followed by a [ character. Escape sequences instruct the terminal to do various text formatting tasks, 
    such as coloring text, moving the cursor around, and clearing parts of the screen.

    We are using the J command (Erase In Display) to clear the screen. Escape sequence commands take arguments,
    which come before the command. In this case the argument is 2, which says to clear the entire screen. 
    <esc>[1J would clear the screen up to where the cursor is, and <esc>[0J would clear the screen from the 
    cursor up to the end of the screen.
    */
    write(STDOUT_FILENO, "\x1b[2J", 4);
}

/*** input ***/
void editorProcessKeypress() {
    /* waits for a keypress, and then handles it. */
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
    }
}


/*** init ***/
int main(int argc, char const *argv[]) {
    enableRawMode();

    char c;

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
