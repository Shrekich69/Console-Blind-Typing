#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>

#define RELEASE

#ifdef DEBUG
static size_t malloced = 0;
#endif // DEBUG

/* 
    Terminal mode
*/

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

/* 
    Text
*/

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

    text.str = calloc(text.capacity + 1, sizeof(char));
#ifdef DEBUG
    malloced += (text.capacity + 1) * sizeof(char);
#endif // DEBUG
    if (!text.str)
    {
        fprintf(stderr, "Allocation error");
        return text;
    }
    strcpy(text.str, str);
    text.str[text.length] = '\0';
    return text;
}

// just a wrap for an existing pointer ("ncp" means "no copy")
struct Text text_create_ncp(char *str)
{
    struct Text text;

    text.str = str;
    text.length = strlen(str);
    text.capacity = text.length * 2;

    return text;
}

int text_append(struct Text *text, char c)
{
    if (text->length >= text->capacity)
    {
        text->capacity *= 2;
        text->str = realloc(text->str, text->capacity * sizeof(char));
#ifdef DEBUG
        malloced += text->capacity * sizeof(char);
#endif // DEBUG
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

/* 
    run_test
*/

// keycodes
#define ESC   27
#define DEL   127
#define CR    10
#define SPACE 32

/* 
    ANSI Escape Codes
*/
#define ESC_CODE    "\033["
#define CANCEL_CODE "\033[m" // Cancels previous codes
#define CLR_CODE    "\033c"
// Modificators
#define BOLD       ESC_CODE "1m"
#define UNDERSCORE ESC_CODE "4m"
// Text colors
#define RED    ESC_CODE "31m"
#define GREEN  ESC_CODE "32m"
#define YELLOW ESC_CODE "33m"
#define GREY   ESC_CODE "2m"

// Enum of char types
enum TypeOfTyped
{
    NotTypedYet  = 0,    // If user didn't reach letter yet
    TypedCorrect = 32,   // If user typed letter right
    TypedMistake = 31    // If user did a mistake
};

void run_test(struct Text text)
{
    // Results
    int *corn = calloc(text.length, sizeof(int)); // correct or not
#ifdef DEBUG
    malloced += text.length * sizeof(int);
#endif // DEBUG
    int correct    = 0;
    // corrected mistakes
    int corrections = 0;
    int mistakes    = 0;
    
    int deletions = 0;

    char c;
    size_t ind = 0;
    uint16_t column = 1;
    uint16_t line = 2;

    uint16_t max_column;
    {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
        {
            max_column = ws.ws_col;
        }
        else
        {
            perror("Failed to get winsize");
            return;
        }
    }

    // set terminal
    struct termios orig_termios;
    set_terminal_mode(&orig_termios);
    setvbuf(stdout, NULL, _IONBF, 0);

    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    {
        perror("clock_gettime");
        return;
    }

    while (1)
    {
        printf(CLR_CODE);
        printf(BOLD UNDERSCORE "Type." CANCEL_CODE "\n");
        for (int i = 0; i < text.length; i++)
        {
            printf(ESC_CODE "%im%c" CANCEL_CODE, corn[i], text.str[i]);
        }
        printf(ESC_CODE "%i;%iH", line, column);

        // get typed char
        if (read(STDIN_FILENO, &c, 1) == 1)
        {
            if (c == ESC)
            {
                break;
            }
            else if (c == DEL && ind > 0)
            {
                ind--;

                if (column <= 1)
                {
                    column = max_column;
                    line--;
                }
                else
                {
                    column--;
                }

                if (corn[ind] == TypedCorrect && correct != 0)
                {
                    correct--;
                }
                else if (corn[ind] == TypedMistake && mistakes != 0)
                {
                    mistakes--;
                    deletions++;
                }
                corn[ind] = NotTypedYet;
            }
            else if (c != CR && c != DEL && ind < text.length)
            {
                if (c == text.str[ind])
                {
                    corn[ind] = TypedCorrect;
                    if (deletions > 0)
                    {
                        deletions--;
                        corrections++;
                    }
                    else
                    {
                        correct++;
                    }
                }
                else
                {
                    corn[ind] = TypedMistake;
                    mistakes++;
                }

                ind++;
                if (column >= max_column)
                {
                    line++;
                    column = 1;
                }
                else
                    column++;
            }
            // if user reached the end of sentence
            else if ((c == CR || c == SPACE) && ind == text.length)
            {
                printf("\n" BOLD UNDERSCORE "Results:" CANCEL_CODE "\n" GREEN "%i correct" CANCEL_CODE ", " YELLOW "%i corrections" CANCEL_CODE
                    ", " RED "%i mistakes" CANCEL_CODE "\n", correct, corrections, mistakes);

                if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
                {
                    perror("clock_gettime");
                    return;
                }
                double timePassed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
                printf("Time passed: " GREY "%.3f secs" CANCEL_CODE "\nCPM(Chars per min): " GREY "%i cpm" CANCEL_CODE "\n", timePassed, (int)(text.length / (timePassed / 60)));

                break;
            }
        }
    }
    
    free(corn);
    reset_terminal_mode(&orig_termios);
}

int py_find_quotes(char *currDir)
{
    char **args = calloc(3, sizeof(char *));
    if (!args)
    {
        fprintf(stderr, "Allocation error\n");
        return 0;
    }

    args[0] = malloc((strlen("python3") + 1) * sizeof(char));
    args[1] = malloc((strlen(currDir) + strlen("find_quotes.py") + 1) * sizeof(char));
#ifdef DEBUG
    malloced += ((strlen("python3") + 1) * sizeof(char)) + ((strlen(currDir) + strlen("find_quotes.py") + 1) * sizeof(char));
#endif // DEBUG
    if (!args[0] || !args[1])
    {
        fprintf(stderr, "Allocation error\n");
        free(args[1]);
        free(args[0]);
        free(args);
        return 0;
    }
    args[2] = NULL;

    strcpy(args[0], "python3");
    strcpy(args[1], currDir);
    strcat(args[1], "find_quotes.py");

    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0)
    {
        if (execvp(args[0], args) == -1)
        {
            perror("cbt");
        }
        free(args[1]);
        free(args[0]);
        free(args);
        return 0;
    }
    else if (pid < 0)
    {
        perror("cbt");
        return 0;
    }
    else
    {
        do
        {
            waitpid(pid, &status, 0);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    free(args[1]);
    free(args[0]);
    free(args);

    return 1;
}

char *path(char *binPath, char *_Nullable fileName)
{
    {
        int i = strlen(binPath) - 1;
        while (i >= 0)
        {
            if (binPath[i] == '/')
            {
                binPath[i + 1] = '\0';
                break;
            }
            i--;
        }
    }
    
    char *path;
    if (fileName)
        path = calloc(strlen(binPath) + strlen(fileName) + 1, sizeof(char));
    else
        path = calloc(strlen(binPath) + 1, sizeof(char));

#ifdef DEBUG
    malloced += (strlen(binPath) + 1) * sizeof(char);
#endif // DEBUG

    if (!path)
    {
        fprintf(stderr, "Allocation error\n");
        return NULL;
    }
    strcpy(path, binPath);
    
    if (fileName)
    {
        strcat(path, fileName);
    }
    
    return path;
}

char **get_quotes(char *binPath, int *_Nullable a_quotesInTotal)
{
    // get quotes
    char *currPath = path(binPath, NULL);
    if (!py_find_quotes(currPath))
    {
        fprintf(stderr, "Error while executing python script\n");
    }
    free(currPath);

    char *filePath = path(binPath, "quotes.txt");
    FILE *f = fopen(filePath, "r");
    free(filePath);
    if (!f)
    {
        fprintf(stderr, "Failed opening file\n");
        return NULL;
    }

    // read quotes
    int quotesInTotal;
    fscanf(f, "%i", &quotesInTotal);
    if (a_quotesInTotal)
        *a_quotesInTotal = quotesInTotal;

    char **quotesf = calloc(quotesInTotal, sizeof(char *));
#ifdef DEBUG
    malloced += quotesInTotal * sizeof(char *);
#endif // DEBUG
    quotesf[quotesInTotal - 1] = NULL;
    size_t quoteCapp;
    for (int i = 0; i < quotesInTotal - 1; i++)
    {
        getdelim(&quotesf[i], &quoteCapp, '\0', f);
#ifdef DEBUG
        malloced += quoteCapp * sizeof(char);
#endif // DEBUG
    }
    fclose(f);

    return quotesf;
}

void quotes_free(char **quotes)
{
    for (int i = 0; quotes[i] != NULL; i++)
    {
        free(quotes[i]);
    }
    free(quotes);
}

int main(int argc, char **argv)
{
    int quotesInTotal;
    char **quotes = get_quotes(argv[0], &quotesInTotal);
    if (!quotes)
    {
        return 1;
    }

    // pick a random quote
    srand(time(NULL));
    int random_quote = rand() % (quotesInTotal - 2);
    struct Text text = text_create_ncp(quotes[random_quote]);
    run_test(text);

#ifdef DEBUG
    printf("Malloced: %lu", malloced);
#endif // DEBUG
    
    // cleaning
    quotes_free(quotes);
    return 0;
}
