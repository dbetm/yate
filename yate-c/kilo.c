#include <ctype.h>
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

    There is an ICANON flag that allows us to turn off canonical mode. This means we will finally be 
    reading input byte-by-byte, instead of line-by-line.
    */

    tcgetattr(STDIN_FILENO, &original_terminal);
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
    tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw);
}


int main(int argc, char const *argv[]) {
    enableRawMode();

    char c;
    while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        // display read characters
        if(iscntrl(c)) { // is a control char? ASCII 0-31
            printf("%d\r\n", c);
        }
        else {
            printf("%d ('%c')\r\n", c, c); // print the ascii number and char?
        }
    }
    return 0;
}
