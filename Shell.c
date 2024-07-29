#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>


// Definindo tamanhos máximos 
#define MAX_LINHA 1024
#define MAX_ARGS 100
#define MAX_HISTORICO 100


// Criando array de ponteiros para o histórico
char *historico[MAX_HISTORICO];
int historico_inicio = 0;  // Índice do início do histórico
int historico_contador = 0;  // Número de comandos no histórico



// Comando de adicionar ao histórico usando uma fila circular
void adicionar_ao_historico(char *cmd) {
    // Verifica se o histórico estiver cheio,  caso positivo libera a memória do comando mais antigo, negativo, somente incrementa o contador
    if (historico_contador == MAX_HISTORICO) {
        free(historico[historico_inicio]);
        historico_inicio = (historico_inicio + 1) % MAX_HISTORICO;  // Atualiza o índice de início
    } else {
        historico_contador++;
    }

    // Calcula o índice do próximo comando a ser adicionado
    int indice = (historico_inicio + historico_contador - 1) % MAX_HISTORICO;
    historico[indice] = strdup(cmd);
}

// Função para imprimir o histórico 
void print_historico() {
    for (int i = 0; i < historico_contador; i++) {
        int indice = (historico_inicio + i) % MAX_HISTORICO;
        printf("%d: %s\n", i + 1, historico[indice]);
    }
}


// Função para dar parse no comando, recebendo uma string , separando em argumentos, arquivos de entrada e saída, além de pipes
void parse_comando(char *cmd, char **args, int *background, char **entrada_arquivo, char **saida_arquivo, char **pipe_args) {
    
    // inicia variáveis de índice, pipes e arquivos
    int i = 0;
    int pipe_flag = 0; //flag de pipe
    *background = 0; //flag de background
    *entrada_arquivo = NULL;
    *saida_arquivo = NULL;
    *pipe_args = NULL;


    // Loop externo para percorrer a string cmd até encontrar \0 (final)
    while (*cmd != '\0') {
        // Loop interno para pular espaços, tabulações e nova linha, substituindo por \0
        while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n') {
            *cmd++ = '\0';
        }
        // Verifica se o caractere é pipe "|", em caso positivo substitui por \0, ativa a flag de pipe para 1 e retorna o índice para 0 
        if (*cmd != '\0') {
            if (*cmd == '|') {
                *cmd++ = '\0';
                pipe_flag = 1;
                i = 0;
            } 
            // Verifica se o caractere é do tipo de redirecionamento de entrada "<", em caso positivo substitui por \0, ignorando espaços tabulações e novas linhas 
            else if (*cmd == '<') {
                *cmd++ = '\0';
                while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n') {
                    *cmd++ = '\0';
                }
                // define o arquivo de entrada como o ponteiro atual e avança cmd até encontrar espaço, nova linha ou final da string
                *entrada_arquivo = cmd;
                while (*cmd != '\0' && *cmd != ' ' && *cmd != '\t' && *cmd != '\n') {
                    cmd++;
                }
            } 
            // Verifica se o caractere é do tipo de redirecionamento de saída ">", em caso positivo substitui por \0, ignorando espaços tabulações e novas linhas 
            else if (*cmd == '>') {
                *cmd++ = '\0';
                while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n') {
                    *cmd++ = '\0';
                }
                // define o arquivo de saída como o ponteiro atual e avança cmd até encontrar espaço, nova linha ou final da string
                *saida_arquivo = cmd;
                while (*cmd != '\0' && *cmd != ' ' && *cmd != '\t' && *cmd != '\n') {
                    cmd++;
                }
            } 
            // Se a flag de pipe estiver ativa, adiciona o argumento a pipe_args, caso contrário, adiciona o argumento a args
            else {
                if (pipe_flag) {
                    pipe_args[i++] = cmd;
                } else {
                    args[i++] = cmd;
                }
            }
            //Avança cmd até encontrar um espaço, tabulação, nova linha ou o final da string
            while (*cmd != '\0' && *cmd != ' ' && *cmd != '\t' && *cmd != '\n') {
                cmd++;
            }
        }
    }
    //Adiciona NULL ao final de pipe_args ou args para indicar o fim dos argumentos
    if (pipe_flag) {
        pipe_args[i] = NULL;
    } else {
        args[i] = NULL;
    }
    //Verifica se o último argumento é & para indicar execução em segundo plano utilizando strcmp
    if (i > 0 && args[i-1] && strcmp(args[i-1], "&") == 0) {
        //Define background como 1
        *background = 1;
        //Substitui & por NULL no array de argumentos
        args[i-1] = NULL;
    }
}
// Função para executar o comando, inteiro que indica se o comando deve ser executado em segundo plano (1 para background, 0 para foreground), arquivos de entrada e saída, além de pipes
void executar_comando(char **args, int background, char *entrada_arquivo, char *saida_arquivo, char **pipe_args) {

    int fd[2];//array de dois inteiros que armazena os descritores de arquivos usados para o pipe
    pid_t pid1, pid2;//Variáveis usadas para armazenar os ids dos processos filhos

    // Se pipe_args não for nulo, cria um pipe usando a função pipe(fd), em caso de falha, retorna uma mensagem de erro e finaliza o processo
    if (pipe_args[0] != NULL) {
        if (pipe(fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
    // Cria um processo filho usando fork()
    pid1 = fork();
    //Se pid1 for igual a 0, o código a seguir é executado no processo filho
    if (pid1 == 0) { 
        //Se entrada_arquivo não for nulo, abre o arquivo de entrada em modo somente leitura
        if (entrada_arquivo) {
        
            int fd_in = open(entrada_arquivo, O_RDONLY);
            //Se a abertura do arquivo falhar, exibe uma mensagem de erro e encerra o processo
            if (fd_in < 0) {
                perror("abertura entrada arquivo");
                exit(EXIT_FAILURE);
            }
            //Copia a entrada padrão (STDIN_FILENO) para o descritor de arquivo do arquivo de entrada e fecha o descritor de arquivo do arquivo de entrada
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        //Se pipe_args não for nulo, redireciona a saída padrão (STDOUT_FILENO) para o descritor de escrita do pipe e fecha os descritores de leitura e escrita do pipe
        if (pipe_args[0] != NULL) {
            dup2(fd[1], STDOUT_FILENO);
            close(fd[0]);
            close(fd[1]);
        } 
        //Se pipe_args for nulo mas saida_arquivo não for nulo, abre o arquivo de saída em modo de escrita, criando ou truncando o arquivo
        else if (saida_arquivo) {
            // Cria uma variável fd_out para armazenar o descritor de arquivo, no modo de apenas escrita, criando o arquivo se não existir, ou esvazia o arquivo se já existir, além de dar permissão de leitura e escrita para somente o proprietário
            int fd_out = open(saida_arquivo, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            //Se a abertura do arquivo de saída falhar, exibe uma mensagem de erro e encerra o processo
            if (fd_out < 0) {
                perror("abertura saida arquivo");
                exit(EXIT_FAILURE);
            }
            //Dupilca a saída padrão para o descritor de arquivo do arquivo de saída e fecha o descritor de arquivo
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        //Executa o comando especificado em args usando execvp()
        execvp(args[0], args);

        //Em caso de falha, exibe uma mensagem de erro e encerra o processo
        perror("execvp");
        exit(EXIT_FAILURE);
    } 
    //Se pid1 for maior que 0, o código a seguir é executado pelo processo pai
    else if (pid1 > 0) { 
        //Se o comando não deve ser executado em background e não há pipe, espera o primeiro processo filho terminar usando waitpid()
        if (!background && pipe_args[0] == NULL) {
            waitpid(pid1, NULL, 0);
        }
        //Se pipe_args não for nulo, cria um segundo processo filho usando fork()
        if (pipe_args[0] != NULL) {
            pid2 = fork();
            //Se o valor de pid2 for igual a 0, o código é executado no segundo processo filho
            if (pid2 == 0) {
                //Duplica a entrada padrão (STDIN_FILENO) para o descritor de leitura do pipe e fecha os descritores de leitura e escrita do pipe
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);
                close(fd[0]);
                //Se saida_arquivo não for nulo, redireciona a saída padrão para o descritor de arquivo do arquivo de saída
                if (saida_arquivo) {
                    // Cria uma variável fd_out para armazenar o descritor de arquivo, no modo de apenas escrita, criando o arquivo se não existir, ou esvazia o arquivo se já existir, além de dar permissão de leitura e escrita para somente o proprietário
                    int fd_out = open(saida_arquivo, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    // Se fd_out for menor que zero, retorna mensagem de erro e finaliza o processo
                    if (fd_out < 0) {
                        perror("abertura saida arquivo");
                        exit(EXIT_FAILURE);
                    }
                    //Dupilca a saída padrão para o descritor de arquivo do arquivo de saída e fecha o descritor de arquivo
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
                //Executa o comando especificado em pipe_args usando execvp()
                execvp(pipe_args[0], pipe_args);
                //Em caso de falha, exibe uma mensagem de erro e encerra o processo
                perror("execvp");
                exit(EXIT_FAILURE);
            } 
            // Se pid2 for maior que 0, o código a seguir é executado pelo processo pai
            else if (pid2 > 0) { 
                //Fecha os descritores de leitura e escrita do pipe
                close(fd[0]);
                close(fd[1]);
                //Se o comando não deve ser executado em background, espera os processos filhos terminarem usando waitpid()
                if (!background) {
                    waitpid(pid1, NULL, 0);
                    waitpid(pid2, NULL, 0);
                }
            } 
            //Se o fork() falhar para o segundo processo filho, exibe uma mensagem de erro e encerra o processo.
            else { 
                perror("fork");
                exit(EXIT_FAILURE);
            }
        }
    } 
    //Se o fork() falhar para o primeiro processo filho, exibe uma mensagem de erro e encerra o processo.
    else { 
        perror("fork");
        exit(EXIT_FAILURE);
    }
}
// Função que procura comandos nos diretórios listados em PATH, verifica se é executável e então executa
void search_and_executar_comando(char **args, int background, char *entrada_arquivo, char *saida_arquivo, char **pipe_args) {
    char *path = getenv("PATH");// Obtém a variávle em PATH
    char *path_copy = strdup(path);//Cria uma cópia da string path usando strdup
    char *dir = strtok(path_copy, ":");//Usa strtok para dividir path_copy em tokens, usando : como delimitador
    char completo_path[MAX_LINHA];// String completo_path que será usada para construir o caminho completo para o comando

    //loop que percorre todos os diretórios listados na variável PATH
    while (dir != NULL) {
        snprintf(completo_path, sizeof(completo_path), "%s/%s", dir, args[0]);//Cria string buffer qie recebe dir e args
        //Verifica se o arquivo no caminho completo_path existe e é executável (X_OK)
        if (access(completo_path, X_OK) == 0) {
            //Copia o caminhi do executável, executa a função de execução e depois libera a memória usada de buffer
            args[0] = completo_path;
            executar_comando(args, background, entrada_arquivo, saida_arquivo, pipe_args);
            free(path_copy);
            return;
        }
        //Obtém o próximo diretório da lista PATH
        dir = strtok(NULL, ":");
    }
    //Se nenhum comando for encontrado nos diretórios PATH, imprime uma mensagem de erro e libera memória de buffer
    fprintf(stderr, "Comando nao encontrado: %s\n", args[0]);
    free(path_copy);
}

int main() {
    char linha[MAX_LINHA];//Array que armazena a linha de comando digitada pelo usuário
    char *args[MAX_ARGS];//Array de ponteiros que armazenará os argumentos do comando, após a divisão da linha de comando
    char *pipe_args[MAX_ARGS];//Array de ponteiros  usado para armazenar os argumentos de um comando após o pipe
    int background;// Flag de backgorund
    char *entrada_arquivo;// Ponteiro para nome de arquivo de entrada
    char *saida_arquivo;// Ponteiro para nome de arquivo de saída

    while (1) {
        printf("shell> ");
        //Lê a lnha de comando, em caso de encontrar valor nulo, exibe mesagem de erro e fecha o loop infinito
        if (fgets(linha, MAX_LINHA, stdin) == NULL) {
            perror("fgets");
            exit(EXIT_FAILURE);
        }

        //Lê a linha de comando procurando o comando de saída
        if (strcmp(linha, "exit\n") == 0) {
            break;
        }

        //Lê a linha de comando procurando pelo comando de histórico, chamando a função de printar o histórico caso afirmativo
        if (strcmp(linha, "historico\n") == 0) {
            print_historico();
            continue;
        }

        //Adiciona a linah de comando ao histórico da Shell
        adicionar_ao_historico(linha);

        //Chama a função de parse para a linha de comando, dividindo em argumentos, flags de pipe e background além de arquivos de entrada e saída 
        parse_comando(linha, args, &background, &entrada_arquivo, &saida_arquivo, pipe_args);

        //Verifica se o argumento a ser executado está nulo
        if (args[0] != NULL) {

            //Chama a função de ler o PATH para executar o comando
            search_and_executar_comando(args, background, entrada_arquivo, saida_arquivo, pipe_args);
        }
    }

    return 0;
}
