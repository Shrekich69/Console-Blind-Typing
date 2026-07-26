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
    Timer
*/

static struct timespec time_start, time_end;

void timer_start()
{
    if (clock_gettime(CLOCK_MONOTONIC, &time_start) == -1)
    {
        perror("clock_gettime");
        return;
    }
}

double timer_stop()
{
    if (clock_gettime(CLOCK_MONOTONIC, &time_end) == -1)
    {
        perror("clock_gettime");
        return -1;
    }
    double timePassed = (time_end.tv_sec - time_start.tv_sec) + (time_end.tv_nsec - time_start.tv_nsec) * 1e-9;

    return timePassed;
}

/* 
    Text
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

struct TypedChar
{
    int type;
    char ch;
};

struct Text
{
    struct TypedChar *str;
    size_t length;
    size_t cappacity;
};

struct Text text_new(char *str)
{
    struct Text text;

    text.length = strlen(str);
    text.cappacity = text.length;
    for (; text.cappacity % 8 != 0; text.cappacity++);
    text.str = calloc(text.cappacity, sizeof(struct TypedChar));
    if (!text.str)
    {
        fprintf(stderr, "Allocation error");
        return text;
    }

    int i;
    for (i = 0; str[i] != '\0'; i++)
    {
        text.str[i].ch = str[i];
    }

    return text;
}

void text_free(struct Text *text)
{
    free(text->str);
    text->str = NULL;
}

void text_print(struct Text text)
{
    for (size_t i = 0; i < text.length; i++)
    {
        printf(ESC_CODE "%im%c" CANCEL_CODE, text.str[i].type, text.str[i].ch);
    }
}

/* 
    run_test
*/

void run_test(struct Text *text)
{
    // Results
    int correct     = 0;
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

    timer_start();

    while (1)
    {
        printf(CLR_CODE);
        printf(BOLD UNDERSCORE "Type." CANCEL_CODE "\n");
        text_print(*text);
        printf(ESC_CODE "%i;%iH", line, column);

        // get typed char
        if (read(STDIN_FILENO, &c, 1) != 1)
            continue;

        // To end test
        if (c == ESC)
        {
            printf(CLR_CODE);
            break;
        }
        // Delete
        else if (c == DEL && ind > 0)
        {
            ind--;

            if (text->str[ind].type == TypedCorrect && correct != 0)
            {
                correct--;
            }
            else if (text->str[ind].type == TypedMistake && mistakes != 0)
            {
                mistakes--;
                deletions++;
            }
            text->str[ind].type = NotTypedYet;

            // Move carriage
            if (column <= 1)
            {
                column = max_column;
                line--;
            }
            else
            {
                column--;
            }
        }
        // Type
        else if ((c != CR && c != DEL) && ind < text->length)
        {
            if (c == text->str[ind].ch)
            {
                text->str[ind].type = TypedCorrect;
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
                text->str[ind].type = TypedMistake;
                mistakes++;
            }

            ind++;

            // Move carriage
            if (column >= max_column)
            {
                line++;
                column = 1;
            }
            else
            {
                column++;
            }
        }
        // if user reached the end of sentence
        else if ((c == CR || c == SPACE) && ind == text->length)
        {
            printf("\n" BOLD UNDERSCORE "Results:" CANCEL_CODE "\n" GREEN "%i correct" CANCEL_CODE ", " YELLOW "%i corrections" CANCEL_CODE
                ", " RED "%i mistakes" CANCEL_CODE "\n", correct, corrections, mistakes);

            double timePassed = timer_stop();
            printf("Time passed: " GREY "%.3f secs" CANCEL_CODE "\nCPM(Chars per min): " GREY "%i cpm" CANCEL_CODE "\n", timePassed, (int)(text->length / (timePassed / 60)));

            break;
        }
    }
    
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
    quotesf[quotesInTotal - 1] = NULL;
    size_t quoteCapp;
    for (int i = 0; i < quotesInTotal - 1; i++)
    {
        getdelim(&quotesf[i], &quoteCapp, '\0', f);
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
    char again = 'y';
    while (again != 'n')
    {
        int quotesInTotal;
        char **quotes = get_quotes(argv[0], &quotesInTotal);
    
        // pick a random quote
        srand(time(NULL));
        int random_quote = rand() % (quotesInTotal - 1);
        struct Text text = text_new(quotes[random_quote]);
    
        run_test(&text);
    
        text_free(&text);
        quotes_free(quotes);

        printf("Again?(y or n) ");
        scanf("%c", &again);
    }

    return 0;
}
