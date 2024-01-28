#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const char PROMPT_NAME[] = "witsshell";
const char *EXIT_STATEMENT = "exit";
const char *COMMAND_SEPARATOR = "&";
size_t BUFFER_SIZE = (1 << 12);

typedef struct {
    char *value;
} Token;

bool isEqual(const char *s1, const char *s2) { return strcmp(s1, s2) == 0; }

const char *readLine(FILE *stream) {
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    char *result = fgets(buffer, BUFFER_SIZE, stream);

    if (result == NULL)
        return NULL;

    // Replace the end line character with a string terminator
    size_t len = strlen(buffer);

    // Replace tabs with spaces
    for (size_t i = 0; i < len; ++i) {
        if (buffer[i] == '\t')
            buffer[i] = ' ';
    }

    if (buffer[len - 1] == '\n')
        buffer[len - 1] = '\0';

    if (isEqual(buffer, EXIT_STATEMENT))
        return NULL;

    return buffer;
}

size_t countDelimiter(const char *buffer, const char *delimiter) {
    char *bufferCopy = strdup(buffer);
    size_t tokenCount = 0;
    char *token = strsep(&bufferCopy, delimiter);

    while (token != NULL) {
        // Checking if token is valid
        if (strlen(token) != 0)
            ++tokenCount;

        token = strsep(&bufferCopy, delimiter);
    }

    return tokenCount;
}

Token *split(const char *buffer, const char *delimiter, size_t *tokenCount) {
    *tokenCount = countDelimiter(buffer, delimiter);
    Token *tokens = malloc(*tokenCount * sizeof(Token));

    size_t index = 0;
    char *bufferCopy = strdup(buffer);
    char *token = strsep(&bufferCopy, delimiter);

    while (token != NULL) {
        size_t tokenSize = strlen(token);

        // Only store valid tokens
        if (tokenSize != 0) {
            tokens[index].value = (char *)malloc(BUFFER_SIZE * sizeof(char));
            tokens[index].value[0] = '\0';
            strcpy(tokens[index++].value, token);
        }

        token = strsep(&bufferCopy, delimiter);
    }

    return tokens;
}

char *join(const char *s1, const char *s2) {
    size_t totalLen = strlen(s1) + strlen(s2);
    char *result = malloc(totalLen * sizeof(char));
    result[0] = '\0';
    strcat(result, s1);
    strcat(result, s2);
    return result;
}

char *const *tokensToArgs(Token *tokens, size_t tokenCount) {
    char **result = malloc((tokenCount + 1) * sizeof(char *));

    for (size_t i = 0; i < tokenCount; ++i) {
        size_t argLen = strlen(tokens[i].value);
        result[i] = malloc(argLen * sizeof(char));
        result[i][0] = '\0';
        strcat(result[i], tokens[i].value);
    }

    result[tokenCount] = NULL;
    return result;
}

char *tokensToString(Token *tokens, size_t tokenCount) {
    size_t totalLen = 0;
    char *tokensString;

    for (size_t i = 0; i < tokenCount; ++i)
        totalLen += strlen(tokens[i].value);

    totalLen += tokenCount;
    tokensString = malloc(totalLen * sizeof(char));
    tokensString[0] = '\0';

    strcat(tokensString, tokens[0].value);

    for (size_t i = 1; i < tokenCount; ++i) {
        strcat(tokensString, " ");
        strcat(tokensString, tokens[i].value);
    }

    return tokensString;
}

void freeTokenList(Token *tokens, const size_t numTokens) {
    if (tokens == NULL)
        return;

    for (size_t i = 0; i < numTokens; ++i) {
        free(tokens[i].value);
    }

    free(tokens);
}

const char error_message[] = "An error has occurred\n";
const char *DEFAULT_PATH = "/bin/";
const char *PATH_DELIMITER = ";";
const char *COMMAND_DELIMITER = " ";
const char *REDIRECT_SYMBOL = ">";
const int INVALID = -1;
const int VALID = 0;

Token *PATH = NULL;
size_t PATH_DIR_COUNT = 0;

void setDefaultPath() {
    PATH_DIR_COUNT = 1;
    PATH = malloc(PATH_DIR_COUNT * sizeof(Token));
    PATH[0].value = strdup(DEFAULT_PATH);
}

void updatePath(Token *tokens, size_t tokenCount) {
    freeTokenList(PATH, PATH_DIR_COUNT);

    // Exclude command token
    PATH_DIR_COUNT = tokenCount - 1;
    PATH = malloc(PATH_DIR_COUNT * sizeof(Token));

    for (size_t i = 0; i < PATH_DIR_COUNT; ++i) {
        char *path = tokens[i + 1].value;

        size_t len = strlen(path);

        if (path[len - 1] != '/') {
            len += 1;
            path = malloc(len * sizeof(char));
            path[0] = '\0';
            strcat(path, tokens[i + 1].value);
            strcat(path, "/");
            PATH[i].value = path;
            printf("MODIFIED PATH: %s", path);
        } else
            PATH[i].value = strdup(tokens[i + 1].value);
            printf("PATH: %s", tokens[i + 1].value);
    }
}

void printError() {
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(1);
}

bool validateRedirect(char *command, Token *tokens, size_t tokenCount) {
    for (size_t i = 0; i < tokenCount; ++i) {
        if (isEqual(tokens[i].value, REDIRECT_SYMBOL)) {
            if (i + 2 == tokenCount)
                return true;

            printError();
        }
    }

    return false;
}

int execute(char *command, Token *tokens, size_t tokenCount) {
    int result = INVALID;
    char *const *args;

    // Check and process redirection
    if (validateRedirect(command, tokens, tokenCount)) {
        freopen(tokens[tokenCount - 1].value, "w", stdout);
        args = tokensToArgs(tokens, tokenCount - 2);
    } else
        args = tokensToArgs(tokens, tokenCount);

    // Loop through the directories stored
    for (size_t i = 0; i < PATH_DIR_COUNT && result == INVALID; ++i) {
        char *fullFilename = join(PATH[i].value, command);
        printf("FULL FILENAME: %s\n", fullFilename);
        if (access(fullFilename, X_OK) != INVALID) {
            result = VALID;
            int pid = fork();

            if (pid == 0) {
                result = execv(fullFilename, args);
            } else {
                wait(&result);
                freopen("/dev/tty", "w", stdout);
            }
        }
    }

    return result;
}

const char *fixRedirect(const char *buffer) {
    size_t len = strlen(buffer);
    bool hasRedirect = false;
    size_t redirectPos = 1;

    for (size_t i = 0; i < len; ++i) {
        if (buffer[i] == *REDIRECT_SYMBOL) {
            hasRedirect = true;
            redirectPos = i;
            break;
        }
    }

    if (!hasRedirect)
        return buffer;

    size_t redirectCount = 0;

    for (size_t i = 0; i < len; ++i) {
        if (buffer[i] == *REDIRECT_SYMBOL) {
            redirectCount++;
        }
    }

    if (redirectCount > 1)
        printError();

    char *copyBuffer = strdup(buffer);
    char *fixedRedirectBuffer = malloc((len + 3) * sizeof(char));
    fixedRedirectBuffer[0] = '\0';

    char *token = strsep(&copyBuffer, REDIRECT_SYMBOL);
    strcat(fixedRedirectBuffer, token);
    strcat(fixedRedirectBuffer, " ");
    strcat(fixedRedirectBuffer, ">");
    strcat(fixedRedirectBuffer, " ");
    token = strsep(&copyBuffer, REDIRECT_SYMBOL);
    strcat(fixedRedirectBuffer, token);
    return fixedRedirectBuffer;
}

void processCommand(const char *buffer) {
    buffer = fixRedirect(buffer);

    size_t tokenCount;
    Token *tokens = split(buffer, COMMAND_DELIMITER, &tokenCount);

    if (tokenCount == 0)
        return;

    char *command = tokens[0].value;
    int commandResult = INVALID;

    if (isEqual(command, "cd")) {
        if (tokenCount == 2) {
            commandResult = chdir(tokens[1].value);
        }
    } else if (isEqual(command, "path")) {
        if (tokenCount >= 1) {
            updatePath(tokens, tokenCount);
            commandResult = 0;
        }
    } else {
        commandResult = execute(command, tokens, tokenCount);
    }

    if (commandResult == INVALID)
        write(STDERR_FILENO, error_message, strlen(error_message));

    freeTokenList(tokens, tokenCount);
}

bool isBuiltInCommand(const char *command) {
    char *commandCopy = strdup(command);
    char *commandName = strsep(&commandCopy, COMMAND_DELIMITER);

    if (isEqual(commandName, "cd"))
        return true;

    if (isEqual(commandName, "path"))
        return true;

    return false;
}

void processLine(const char *buffer) {
    // Sepearate tokens by a space
    size_t tokenCount;
    Token *tokens = split(buffer, COMMAND_DELIMITER, &tokenCount);

    if (tokenCount == 0)
        return;

    char *tokensString = tokensToString(tokens, tokenCount);

    // Separate commands by & symbol
    size_t commandCount;
    Token *commandTokens =
        split(tokensString, COMMAND_SEPARATOR, &commandCount);

    for (size_t i = 0; i < commandCount; ++i) {
        char *command = commandTokens[i].value;

        if (isBuiltInCommand(command)) {
            processCommand(command);
        } else {
            int pid = fork();

            if (pid == 0) {
                processCommand(command);
                fclose(NULL);
                exit(0);
            }
        }
    }

    while (wait(NULL) > 0)
        ;

    freeTokenList(tokens, tokenCount);
    freeTokenList(commandTokens, commandCount);
}
int main(int MainArgc, char *MainArgv[]) {
    bool batchMode = false;
    FILE *stream = stdin;

    if (MainArgc > 2) {
        printError();
    }

    if (MainArgc == 2) {
        stream = fopen(MainArgv[1], "r");
        batchMode = true;

        if (stream == NULL) {
            printError();
        }
    }

    setDefaultPath();

    if (!batchMode)
        printf("%s> ", PROMPT_NAME);

    const char *buffer = readLine(stream);

    while (buffer != NULL) {
        processLine(buffer);

        if (!batchMode)
            printf("%s> ", PROMPT_NAME);

        buffer = readLine(stream);
    }

    return (0);
}
