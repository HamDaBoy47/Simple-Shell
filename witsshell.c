#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

char *buffer, *filename;
char **commandsTemp, **commands;
size_t bufsize = 1024;
size_t inputsize;

char paths[50][150];
int totalPaths = 3;

char error_message[30] = "An error has occurred\n";

bool state = true;
bool safeState = true;
bool correctForm = true;

int countElements(char **array) {
    int count = 0;

    for (int i = 0; array[i] != NULL; i++) {
        count++;
    }
    return count;
}

void printError() {
	write(STDERR_FILENO, error_message, strlen(error_message));
}

void printFileError() {
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    write(fd, error_message, strlen(error_message));
    close(fd);
}

bool processCreator(char *fullPath, char **buffer) {
	if (access(fullPath, X_OK) == 0) {
		pid_t pid = fork();

		if (pid == 0) {
			execv(fullPath, buffer);
			printError();
    		exit(1);
    		
		} else if (pid > 0) {
			while (wait(NULL) > 0);
		    // wait(NULL);
			return true;
    	} else {
        	printError();
    	    exit(1);
    	}
		return true;
	}
	return false;
}

bool processFileCreator(char *fullPath, char **buffer) {
	if (access(fullPath, X_OK) == 0) {
        pid_t pid = fork();

        if (pid == 0) {
            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

            dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);

            close(fd);

            execv(fullPath, buffer);
            printError();
            exit(1);
        } else if (pid > 0) {
			while (wait(NULL) > 0);
            // wait(NULL);
			return true;
        } else {
            printError();
            exit(1);
        }
        return true;
	}
	return false;
}

void exitHandler(char **buffer) {
	if (buffer[1] == NULL) {
		exit(0);
	} else if (buffer[1] != NULL) {
		printError();
	}
}

void cdHamdler(char **buffer, int i) {
	if (buffer[1] == NULL) {
		printError();
	} else if (buffer[2] != NULL) {
		printError();
	} else {
		char * tempPath = (char *)malloc(strlen(paths[1]) + strlen(buffer[i]) + 1);
		strcpy(tempPath, paths[1]);
		strcat(tempPath, buffer[i]);

		if (chdir(tempPath) != 0) {
			printError();
		} else {
			strcpy(paths[2], getcwd(NULL, 0));
			strcat(paths[2], "/");	
		}
	}	
}

bool pathHandler(char **buffer) {
	int i = countElements(buffer);
	if (buffer[1] == NULL) {
		return false;
	} else {
		for (int j = 1; j < i; j++) {
			if (buffer[j][strlen(buffer[j]) - 1] != 47) {
				strcpy(paths[totalPaths], buffer[j]);
                strcat(paths[totalPaths], "/"); 
            } else {
				strcpy(paths[totalPaths], buffer[j]);
			}
			totalPaths++;
		}
	}
	return true;	
}

bool redirectHandler(char **buffer) {
	commands = (char **)malloc(bufsize * sizeof(char *) + 4);
	int bufferSize = countElements(buffer);
	int i = 0;
	
	if ((strstr(buffer[0], ">") != NULL)) {
		correctForm = false;
		return false;
	}
	
	while ((buffer[i] != NULL) && (strstr(buffer[i], ">") == NULL)) {
		// printf("Before: %s \n", buffer[i]); // change to appending to COMMAND ARRAY
		commands[i++] = buffer[i];
	}

	int commandSize = countElements(commands);
	if ((bufferSize - commandSize) != 2 && (bufferSize - commandSize) != 0) {
		correctForm = false;
		return false;
	} 

	if ((buffer[i] != NULL) && (strstr(buffer[i], ">>") != NULL)) {
		correctForm = false;
	}

	if (correctForm) {
		i++;
		while (buffer[i] != NULL) {
			if (strstr(buffer[i], ">") != NULL) {
				correctForm = false;
				break;
			} else {
				filename = (char *)malloc(strlen(buffer[i]) + 1);
				strcpy(filename, buffer[i]); // change to append to file string
				return true;
			}
			i++;
		}
	}
	return false;
}

bool executer(char **buffer) {
	// Built-in commands
	if (strcmp(buffer[0], "exit") == 0) {
		exitHandler(buffer);	// exit	
		return true;
	} 
	
	if (strcmp(buffer[0], "cd") == 0) {
		cdHamdler(buffer, 1);	// cd
		return true;
	}

	if (strcmp(buffer[0], "path") == 0) {
		safeState = pathHandler(buffer);	// path
		return true;
	}
	
	// External commands
	bool redirect = redirectHandler(buffer);
	char *command;

	if (redirect) {
		command = commands[0];
	} else {
		command = buffer[0];
	}
	
	char *fullPath = (char *)malloc(strlen(paths[0]) + strlen(command) + 1);
	bool found = false;
	int i = 0;

	strcpy(fullPath, paths[i]);
	strcat(fullPath, command);

	while (i < totalPaths && found == false && safeState == true && correctForm == true) {
		// printf("%s \n", fullPath);
		if (redirect) {
			found = processFileCreator(fullPath, commands);
		} else {
			found = processCreator(fullPath, buffer);
		}
		
		free(fullPath);
		
		i++;

		fullPath = (char *)malloc(strlen(paths[i]) + strlen(command) + 1);
		strcpy(fullPath, paths[i]);
		strcat(fullPath, command);
	}

	if (found == false || safeState == false) {
		if (correctForm && redirect) {
			printFileError();
		} 
		else {
			printError();
		}
		correctForm = true;
	} 
	free(fullPath);
	return true;
}

char** splitter(char *input, char *delim) {
	// Replace the end line character with a string terminator
    size_t len = strlen(input);

    // Replace tabs with spaces
    for (size_t i = 0; i < len; ++i) {
        if (input[i] == '\t')
            input[i] = ' ';
    }

    if (input[len - 1] == '\n')
        input[len - 1] = '\0';

    char *token;

    // Allocate memory for an array of strings to store the tokens
    char **tokens = (char **)malloc(bufsize * sizeof(char *) + 4);
    int i = 0;

    // Tokenize the command string
    token = strtok(input, delim);
    while (token != NULL) {
        // Store the token in the array
        token[strcspn(token, "\n")] = '\0';
        tokens[i++] = token;
        // Get the next token
        token = strtok(NULL, delim);
    }

    return tokens;
}

bool executeCommand(char *command) {
    char **commandsTemp = splitter(command, " ");
    bool state = executer(commandsTemp);
    return state;
}

void parallelExecutor(char modified_buffer[bufsize]) {
	char *token;
	char *saveptr;
	char *command;

	token = strtok_r(modified_buffer, "&", &saveptr);
	while (token != NULL) {
		// Execute each command in parallel
		command = token;
		if (command[0] == ' ') {
			// Remove leading space if present
			command++;
		}
		if (command[strlen(command) - 1] == ' ') {
			// Remove trailing space if present
			command[strlen(command) - 1] = '\0';
		}

		if (command[0] != '\0') {  // Ignore empty commands
			pid_t pid = fork();

			if (pid == 0) {
				// This is the child process
				bool state = executeCommand(command);

				// Exit the child process
				exit(state ? 0 : 1);
			} else if (pid > 0) {
				// This is the parent process

			} else {
				printError();
				exit(1);
			}
		}

		// Get the next command
		token = strtok_r(NULL, "&", &saveptr);
	}

	// Wait for all child processes to complete
	int child_status;
	while (wait(&child_status) > 0) {
		if (WIFEXITED(child_status) && WEXITSTATUS(child_status) != 0) {
			// Handle any errors from child processes
			printError();
		}
	}
}

bool inputHandler() {
    // Remove the trailing newline character, if present
    bool state = true;

    int input_len = strlen(buffer);
    if (input_len > 0 && buffer[input_len - 1] == '\n') {
        buffer[input_len - 1] = '\0';
    }

    // Check if the line is only spaces or truly empty
    int is_empty = 1;  // Assume it's empty
    for (int i = 0; i < input_len - 1; i++) {
        if (buffer[i] != ' ' && buffer[i] != '\t') {
            is_empty = 0;  // Not empty
            break;
        }
    }

    if (!is_empty) {
		// Create a modified buffer with spaces around ">" and "&" characters
        char modified_buffer[bufsize];
        int j = 0;

        for (int i = 0; i < input_len; i++) {
            if (buffer[i] == '>' || buffer[i] == '&') {
                // Add a space before and after ">" and "&" characters
                modified_buffer[j++] = ' ';
                modified_buffer[j++] = buffer[i];
                modified_buffer[j++] = ' ';
            } else {
                modified_buffer[j++] = buffer[i];
            }
        }
        modified_buffer[j] = '\0';

		// Check if "&" is present in the modified_buffer
        char *ampersand_check = strchr(modified_buffer, '&');

		if (ampersand_check) {
			// Execute each command in parallel
			parallelExecutor(modified_buffer);
		} else {
			state = executeCommand(modified_buffer);
		}
    }

    return state;
}

void interactiveMode() {
	buffer = (char *)malloc(bufsize * sizeof(char));

	while (state) {
        printf("witsshell> ");

        // Use fgets to read input
        if (fgets(buffer, bufsize, stdin) == NULL) {
            printf("\n");
            exit(0);
        }

		state = inputHandler();
    }
    exit(0);
}

void batchMode(char *MainArgv[]) {
    buffer = (char *)malloc(bufsize * sizeof(char));
    FILE *fp = fopen(MainArgv[1], "r");

    if (fp == NULL && access(MainArgv[1], F_OK) == -1) {
        printError();
        exit(1);
    }

    while (state && !feof(fp)) {
        // Use fgets to read input
        if (fgets(buffer, bufsize, fp) == NULL) {
            exit(0);  // End of file reached
        }

		state = inputHandler();
    }
    fclose(fp);
    free(buffer);
}

int main(int MainArgc, char *MainArgv[]){
	strcpy(paths[0], "/bin/"); // Add /bin/ to the paths array
	strcpy(paths[1], getcwd(NULL, 0)); // Add 'home' directory to the paths array
	strcat(paths[1], "/");

	if (MainArgc > 2) {
		printError();
		exit(1);
	}

	if (MainArgc == 1) {
		interactiveMode();
	} else {
		batchMode(MainArgv);
	}
	return(0);
}
