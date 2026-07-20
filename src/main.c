#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>

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

    text.str = calloc(text.capacity + 1, sizeof(char));
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
    // Results
    int *corn = calloc(text.length, sizeof(int));
    int correct    = 0;
    // corrected mistakes
    int corrections = 0;
    int mistakes    = 0;

    int deletions = 0;

    char c;
    int ind = 0;
    int column = 1;
    int line = 2;
    int max_column;
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
        printf("\033c");
        printf("\033[1m\033[4mType.\033[m\n");
        for (int i = 0; i < text.length; i++)
        {
            printf("\033[%im%c\033[m", corn[i], text.str[i]);
        }
        printf("\033[%i;%iH", line, column);

        // get typed char
        if (read(STDIN_FILENO, &c, 1) == 1)
        {
            if (c == '\033')
            {
                break;
            }
            else if (c == 127 && ind >= 0) // 127 is Delete
            {
                deletions++;
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
                if (corn[ind] == TypedCorrect)
                    correct--;
                else if (corn[ind] == TypedMistake)
                    mistakes--;
                corn[ind] = NotTypedYet;
            }
            else if (c != '\n' && ind < text.length)
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
            else if ((c == '\n' || c == ' ') && ind == text.length)
            {
                printf("\n\033[1m\033[4mResults:\033[m\n\033[32m%i correct\033[m, \033[33m%i corrections\033[m, \033[31m%i mistakes\033[m\n", correct, corrections, mistakes);

                if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
                {
                    perror("clock_gettime");
                    return;
                }
                double timePassed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
                printf("Time passed: \033[2m%.3f secs\033[m\nCPM(Chars per min): \033[2m%i cpm\033[m\n", timePassed, (int)(text.length / (timePassed / 60)));

                break;
            }
        }
    }
    
    free(corn);
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

    struct Text text = text_create(argv[1], 100);
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

int py_find_quotes(char *working_dir)
{
    char **args = calloc(3, sizeof(char *));
    if (!args)
    {
        fprintf(stderr, "Allocation error\n");
        return 0;
    }

    args[0] = malloc((strlen("python3") + 1) * sizeof(char));
    args[1] = malloc((strlen(working_dir) + strlen("find_quotes.py") + 1) * sizeof(char));
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
    strcpy(args[1], working_dir);
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

char **get_quotes(int *quotesInTotal, char *pathToBin)
{
    // get quotes
    char *working_dir = malloc(sizeof(char) * (strlen(pathToBin) + strlen("quotes.txt") + 1));
    strlcpy(working_dir, pathToBin, strlen(pathToBin) - 2);
    if (!py_find_quotes(working_dir))
    {
        fprintf(stderr, "Error while executing python script\n");
    }

    char file_path[strlen(working_dir)];
    strcpy(file_path, working_dir);
    strcat(file_path, "quotes.txt");
    FILE *f = fopen(file_path, "r");
    if (!f)
    {
        fprintf(stderr, "Failed opening file\n");
        free(working_dir);
        return NULL;
    }

    // read quotes
    fscanf(f, "%i", quotesInTotal);
    char **quotesf = calloc(*quotesInTotal - 1, sizeof(char *));
    size_t quoteLen;
    for (int i = 0; i < *quotesInTotal; i++)
    {
        getline(&quotesf[i], &quoteLen, f);
    }
    fclose(f);

    free(working_dir);
    return quotesf;
}

void quotes_free(char **quotes, int quotesInTotal)
{
    for (int i = 0; i < quotesInTotal; i++)
    {
        free(quotes[i]);
    }
    free(quotes);
}

int main(int argc, char **argv)
{
    int quotesInTotal;
    char **quotes = get_quotes(&quotesInTotal, argv[0]);
    if (!quotes)
    {
        return 1;
    }

    // pick a random quote
    srand(time(NULL));
    int random_quote = rand() % quotesInTotal;
    struct Text text = text_create_ncp(quotes[random_quote]);
    run_test(text);
    
    // cleaning
    quotes_free(quotes, quotesInTotal);
    return 0;
}
