#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT_LENGTH 100
#define MAX_ARGS 10

int ctrl_c_z_pressed = 0;
int ctrl_d_pressed = 0;

void display_prompt() {
	
    char cwd[MAX_INPUT_LENGTH];
    char hostname[MAX_INPUT_LENGTH];
    char *username = getenv("USER");
    
    if (!username || gethostname(hostname, sizeof(hostname)) == -1) {
        perror("getenv/gethostname");
        exit(EXIT_FAILURE);
    }
    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    char *home = getenv("HOME");

    if (home && strstr(cwd, home) == cwd) {
        printf("[mysh] %s@%s:~%s$ ", username, hostname, cwd + strlen(home));
    } else {
        printf("[mysh] %s@%s:%s$ ", username, hostname, cwd);
    }
}

void replace_home_directory(char *cwd) {
    char *home = getenv("HOME");
    if (home != NULL && strstr(cwd, home) == cwd) {
        // O diretório home do usuário faz parte do caminho atual
        memmove(cwd, "~", 1); // Substitui o diretório home pelo símbolo ~
        memmove(cwd + 1, home + strlen(home), strlen(cwd) - 1); // Adiciona o restante do caminho após o diretório home
    }
    return;
}
void signal_handler(int signum) {
	ctrl_c_z_pressed = 1;
    // Ignora os sinais SIGTSTP e SIGINT
}
void signal_d(int signum) {
	ctrl_d_pressed = 1;
}

void execute_commands_with_pipes(char *commands[], int num_commands) {
    int pipes[num_commands - 1][2]; // Array de pipes
    pid_t pid;

    // Cria os pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Executa cada comando em um processo filho
    for (int i = 0; i < num_commands; i++) {
        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Processo filho
            // Redireciona a entrada se não for o primeiro comando
            if (i != 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
                close(pipes[i - 1][0]);
            }
            // Redireciona a saída se não for o último comando
            if (i != num_commands - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
                close(pipes[i][1]);
            }

            // Fecha todos os pipes
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Parse o comando e seus argumentos
            char *args[MAX_ARGS];
            char *token = strtok(commands[i], " ");
            int j = 0;
            while (token != NULL && j < MAX_ARGS - 1) {
                args[j++] = token;
                token = strtok(NULL, " ");
            }
            args[j] = NULL;

            // Executa o comando
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }

    // Fecha todos os pipes
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Espera todos os processos filhos terminarem
    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

int main() {
    char input[MAX_INPUT_LENGTH];
    char *token;
	char *args[] = {"ls", "-l", NULL};
	char *path = "~";
	char *pos;
	char cwd[MAX_INPUT_LENGTH];
	char *username = getenv("USER");
	
	signal(SIGTSTP, signal_handler); // Ignora Ctrl+Z
	signal(SIGINT, signal_handler); // Ignora Ctrl+C
	signal(SIGQUIT, signal_d);
	
	 if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
	
	while (1) {
    display_prompt();
    
      if (fgets(input, MAX_INPUT_LENGTH, stdin) == NULL || feof(stdin)) {
            printf("\n");
            break; // Ctrl+D or EOF
        }
    input[strcspn(input, "\n")] = 0; // Remove a nova linha do final da entrada

	 if (strcmp(input, "exit") == 0) { // Verifica se o comando digitado foi exit
            break;
     }
     
      if (strncmp(input, "cd", 2) == 0) { // se o comando digitado começa com cd
            char *path = input + 3;
            if (path[0] == '\0') {
                path = getenv("HOME");
            }
            if (path == NULL || strcmp(path, "~") == 0) {
		        path = getenv("HOME");
		        if (path == NULL) {
		            fprintf(stderr, "cd error: HOME variable not set\n");
		            break;
		        }
    		}
            if (!path) {
                fprintf(stderr, "cd error: HOME variable not set\n");
                continue;
            }
            if (chdir(path) != 0) {
                perror("Diretorio inexistente");
            }
            continue;
        }
        
    // Separa os comandos por pipes
    char *commands[MAX_ARGS];
    int num_commands = 0;
    token = strtok(input, "|");
    while (token != NULL && num_commands < MAX_ARGS) {
        commands[num_commands++] = token;
        token = strtok(NULL, "|");
    }

    // Executa os comandos com pipes
    execute_commands_with_pipes(commands, num_commands);
	}
    return 0;
}