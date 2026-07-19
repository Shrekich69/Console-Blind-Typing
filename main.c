#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

void reset_terminal_mode(struct termios *orig_term)
{
    tcsetattr(STDIN_FILENO, TCSANOW, orig_term);
}

void set_terminal_mode(struct termios *orig_term)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig_term);
    raw = *orig_term;

    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// Enum of char types
enum TypeOfTyped
{
    NotTypedYet  = 0,    // If user didn't reach letter yet
    TypedCorrect = 32,   // If user typed letter right
    TypedMistake = 31    // If user did a mistake
};

struct Text
{
    char  *str;
    size_t length;
    size_t capacity;
};

struct Text text_create(char *str, size_t capacity)
{
    struct Text text;

    text.length = strlen(str);
    if (capacity < text.length)
        capacity = text.length * 2;
    text.capacity = capacity;

    text.str = malloc(sizeof(char) * (text.capacity + 1));
    if (!text.str)
    {
        fprintf(stderr, "Allocation error");
        return text;
    }
    for (int i = 0; i < text.length; i++)
    {
        text.str[i] = str[i];
    }
    text.str[text.length] = '\0';
    return text;
}

int text_append(struct Text *text, char c)
{
    if (text->length >= text->capacity)
    {
        text->capacity *= 2;
        text->str = realloc(text->str, text->capacity);
        if (!text->str)
        {
            fprintf(stderr, "Allocation error");
            return 0;
        }
    }
    text->length++;
    text->str[text->length - 1] = c;
    text->str[text->length] = '\0';

    return 1;
}

void text_insert(struct Text *text, char c, size_t i)
{
    text->str[i] = c;
    text->length--;
}

void run_test(struct Text text)
{
    int corn[text.length];
    int corrects    = 0;
    // corrected mistakes
    int corrections = 0;
    int mistakes    = 0;

    int deleteSteps = 0;

    char c;
    int ind = 0;
    int column = 1;
    
    struct termios orig_termios;
    set_terminal_mode(&orig_termios);
    setvbuf(stdout, NULL, _IONBF, 0);

    while (1)
    {
        printf("\033c");
        printf("Type\n");
        for (unsigned int i = 0; i < text.length; i++)
        {
            printf("\033[%im%c\033[m", corn[i], text.str[i]);
        }
        printf("\033[2;%iH", column);

        // get typed char
        if (read(STDIN_FILENO, &c, 1) == 1)
        {
            if (c == '\033')
            {
                break;
            }
            else if (c == 127 && ind >= 0) // 127 is Del
            {
                deleteSteps++;
                ind--;
                column--;
                corn[ind] = 0;
            }
            else if (c != '\n' && ind < text.length)
            {
                if (c == text.str[ind])
                {
                    corn[ind] = TypedCorrect;
                    if (deleteSteps > 0)
                    {
                        deleteSteps--;
                        mistakes--;
                        corrections++;
                    }
                    else
                    {
                        corrects++;
                    }
                }
                else
                {
                    corn[ind] = TypedMistake;
                    mistakes++;
                }

                ind++;
                column++;
            }
            else if (c == '\n' && ind == text.length)
            {
                printf("\nResult:\n%i correct, %i corrections, %i mistakes", corrects, corrections, mistakes);
                break;
            }
        }
    }
    printf("\n");

    reset_terminal_mode(&orig_termios);
}


struct Text parse_args(int argc, char **argv)
{
    if (!argv[1])
    {
        printf("Pass string that you want to type");
        struct Text text = {};
        return text;
    }
    
    struct Text text = text_create(argv[1], 0);
    text_append(&text, ' ');
    for (int i = 2; i < argc; i++)
    {
        for (int j = 0; ; j++)
        {
            if (argv[i][j] == '\0')
                break;
            text_append(&text, argv[i][j]);
        }
        text_append(&text, ' ');
    }
    text_insert(&text, ' ', text.length - 1);

    return text;
}

int main(int argc, char **argv)
{
    struct Text text = parse_args(argc, argv);
    
    run_test(text);
    
    return 0;
}
