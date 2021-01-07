/**
 * Sistemas Operativos - AVENTURA 2
 * 
 * My_Shell
 * 
 * Jorge González Pascual - Lluís Barca Pons - Joan Martorell Ferriol
 */

#define _POSIX_C_SOURCE 200112L

// Librerias
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

// Constantes
#define DEBUG0 0
#define DEBUG1 0
#define DEBUG2 0
#define DEBUG3 0
#define DEBUG4 0
#define DEBUG5 0

#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define PROMPT '$'
#define N_JOBS 64
#define NAME_SIZE 255
#define FOREGROUND 0
#define EXECUTED 'E'
#define STOPPED 'D'
#define FINALIZED 'F'
#define NONE 'N'

#define RED "\x1b[91m"
#define GREEN "\x1b[92m"
#define YELLOW "\x1b[93m"
#define BLUE "\x1b[94m"
#define MAGENTA "\x1b[95m"
#define CYAN "\x1b[96m"
#define WHITE "\x1b[97m"
#define COLOR_RESET "\x1b[0m"
#define BLOND "\x1b[1m"

// Variables
// Estructura para almacenar los procesos
struct info_process
{
    pid_t pid;
    char status;                 //N (Ninguno), E (Ejecutando), D (Detenido), F (Finalizado)
    char cmd[COMMAND_LINE_SIZE]; //Líniea de comando
};

static struct info_process jobs_list[N_JOBS];
static int active_jobs = 1;
char mini_shell[COMMAND_LINE_SIZE];

// Funciones
char *read_line(char *line);
int execute_line(char *line);
int parse_args(char **args, char *line);
int check_internal(char **args);
int internal_cd(char **args);
int internal_export(char **args);
int internal_source(char **args);
int internal_jobs(char **args);
int internal_fg(char **args);
int internal_bg(char **args);
int internal_exit(char **args);
int internal_fg(char **args);
void jobs_list_init();
void reaper(int signum);
void ctrlc(int signum);
void ctrlz(int signum);

void characterEraser(char *args, char caracter);
char *replaceWord(const char *cadena, const char *cadenaAntigua, const char *nuevaCadena);
int is_background(char *line);
int is_output_redirection(char **args);

int jobs_list_add(pid_t pid, char status, char *cmd);
int jobs_list_find(pid_t pid);
int jobs_list_remove(int pos);

/**
 * Main del programa
 */
int main(int argc, char *argv[])
{
    // Inicializamos minishell
    strcpy(mini_shell, argv[0]);

    // Manejadores de señales para el padre o para el shell
    signal(SIGCHLD, reaper);
    signal(SIGINT, ctrlc);
    signal(SIGTSTP, ctrlz);

    // Inicializamos la pila de jobs_list
    jobs_list[FOREGROUND].pid = FOREGROUND;
    jobs_list[FOREGROUND].status = NONE;
    memset(jobs_list[FOREGROUND].cmd, '\0', sizeof(jobs_list[FOREGROUND].cmd));

    char *cmd = (char *)malloc(sizeof(char) * COMMAND_LINE_SIZE);

    while (1)
    {
        if (read_line(cmd))
        {
            execute_line(cmd);
        }
    }

    return 0;
}

/**
 * Método para imprimir el PROMPT
 */
void imprimir_prompt()
{
    // Obtenemos el nombre de usuario
    char *user = getenv("USER");
    char *prompt;

    if ((prompt = malloc((sizeof(char) * COMMAND_LINE_SIZE) - sizeof(user))))
    {
        // Obtener el directorio de trabajo actual
        getcwd(prompt, COMMAND_LINE_SIZE);
        if (strcmp(prompt, getenv("HOME")))
        {
            if (strstr(prompt, getenv("HOME")))
            {
                prompt = replaceWord(prompt, getenv("HOME"), "~");
            }
        }

        // Imprimimos el el PROMPT "personalizado"
        printf(BLOND RED "%s:" BLUE "%s " COLOR_RESET YELLOW "%c: " COLOR_RESET, user, prompt, PROMPT);
    }
    else
    {
        perror("Error");
    }

    // Liberamos el flujo de salida de datos
    free(prompt);
    fflush(stdout);
}

/**
 * Método para eer una linea de la consola
 */
char *read_line(char *line)
{
    imprimir_prompt();
    char *ptr = fgets(line, COMMAND_LINE_SIZE, stdin);

    // Leer la entrada introducida en stdin por el usuario
    if (ptr == NULL)
    {
        printf("\r\n");

        if (feof(stdin))
        {
#if DEBUG3
            printf("Adeu\n");
#endif
            exit(0);
        }
    }

    return line;
}

/**
 * Método que efectua la ejecución de la linea leída
 */
int execute_line(char *line)
{
    char cmd[COMMAND_LINE_SIZE];
    // Reservamos memoria para los tokens
    char **args = malloc(sizeof(char *) * ARGS_SIZE);

    if (args != NULL)
    {
        int bckgrd = is_background(line);

        // Copiamos en una variable el comando sin parsear
        characterEraser(line, '\n');
        strcpy(cmd, line);

        // Parseamos
        parse_args(args, line);

        if (args[0])
        {
            if (!check_internal(args))
            {
                pid_t pid = fork();

                // Hijo
                if (pid == 0)
                {
                    // Ignoramos la señal SIGSTP
                    signal(SIGTSTP, SIG_IGN);

                    // Asignamos la acción por defecto al SIGCHLD
                    signal(SIGCHLD, SIG_DFL);

                    // Ignoramos la señal SIGINT
                    signal(SIGINT, SIG_IGN);

                    is_output_redirection(args);

                    execvp(args[0], args);

                    // Terminación anormal
                    fprintf(stderr, "Error al leer el comando externo: %s.\n", args[0]);
                    fflush(stderr);
                    exit(EXIT_FAILURE);
                }
                // Padre
                else if (pid > 0)
                {
                    // Si es background
                    if (bckgrd == 1)
                    {
#if DEBUG5
                        printf("Es un proceso en backgroud\n");
#endif

                        jobs_list_add(pid, EXECUTED, cmd);
                    }
                    else
                    {
                        // Copiamos el padre en la pila
                        jobs_list[FOREGROUND].pid = pid;
                        jobs_list[FOREGROUND].status = 'E';
                        strcpy(jobs_list[FOREGROUND].cmd, cmd);

#if DEBUG4
                        printf("[execute_line() → PID padre: %d] (%s)\n", getpid(), mini_shell);
                        printf("[execute_line() → PID hijo: %d] (%s)\n", pid, jobs_list[FOREGROUND].cmd);
#endif

                        // Mientras haya un proceso foreground
                        while (jobs_list[FOREGROUND].pid > 0)
                        {
                            pause();
                        }
                    }
                }
                // Error de fork()
                else
                {
                    perror("Error fork");
                    // Terminación anormal
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "Memoria dinámica llena.\n");
    }

    // Liberamos memoria
    memset(line, '\0', COMMAND_LINE_SIZE);
    free(args);

    return EXIT_SUCCESS;
}

/**
 * Método que mira si es un comando en segundo plano, es decir que si tiene el & al final
 */
int is_background(char *line)
{
    int numeroLetras = strlen(line);

    if (line[numeroLetras - 2] == '&')
    {
        // Quitamos tambien el espacio antes del '&'
        line[numeroLetras - 3] = '\0';
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * Método que comprueba si es un redireccionamiento y si lo es, cambia el '>' por NULL
 */
int is_output_redirection(char **args)
{
    int outp = 0;
    int args_path;

    for (int i = 0; (args[i] != NULL && outp == 0); i++)
    {
        if (strcmp(args[i], ">") == 0)
        {
            if (strlen(args[i + 1]) > 1)
            {
                args_path = i + 1;
                outp = 1;
                args[i] = NULL;
            }
        }
    }

    if (outp == 1)
    {
        int fd = open(args[args_path], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        dup2(fd, 1);
        close(fd);
    }

    return outp;
}

/**
 * Método que parsea divide la linea en tokens y elimina comentarios
 */
int parse_args(char **args, char *line)
{
    //Creamos una nueva variable para asi no perder la linea original
    char aux[COMMAND_LINE_SIZE];
    strcpy(aux, line);
    int i = 0;
    char *token = strtok(aux, " \n\r\t");

    while (token != NULL)
    {

        args[i] = token;

#if DEBUG1
        printf("[parse_args() --> token%i = %s] \n", i, token);
#endif

        // Si no es un comentario lo añadimos como argumento
        if (strncmp(args[i], "#", 1) != 0)
        {
            i++;
        }

        // Añadimos NULL
        token = strtok(NULL, " \n\r\t");
    }

    args[i] = 0;
    // Quitamos el salto de línea al final
    strtok(line, "\n\r");

    return i;
}

/**
 * Método que comprueba si es un comando interno
 */
int check_internal(char **args)
{
    int comandoInterno = 0;

    const char cd[] = "cd";
    const char export[] = "export";
    const char source[] = "source";
    const char jobs[] = "jobs";
    const char fg[] = "fg";
    const char bg[] = "bg";
    const char exit[] = "exit";

    if (!strcmp(args[0], cd))
    {
        internal_cd(args);
        comandoInterno = 1;
    }
    else if (!strcmp(args[0], export))
    {
        internal_export(args);
        comandoInterno = 1;
    }
    else if (!strcmp(args[0], source))
    {
        internal_source(args);
        comandoInterno = 1;
    }
    else if (!strcmp(args[0], jobs))
    {
        internal_jobs(args);
        comandoInterno = 1;
    }
    else if (!strcmp(args[0], fg))
    {
        internal_fg(args);
        comandoInterno = 1;
    }
    else if (!strcmp(args[0], bg))
    {
        internal_bg(args);
        comandoInterno = 1;
    }
    else if (!strcmp(args[0], exit))
    {
        internal_exit(args);
        comandoInterno = 1;
    }

    return comandoInterno;
}

/**
 * Método que utiliza la llamada al sistema chdir() para cambiar de directorio
 */
int internal_cd(char **args)
{
    char *linea = malloc(sizeof(char) * COMMAND_LINE_SIZE);

    if (linea == NULL)
    {
        fprintf(stderr, "No hay memoria dinámica disponible en este momento.\n");
    }

    int nArgs = 0;

    // Concatenamos los args
    for (int i = 0; args[i]; i++)
    {
        strcat(linea, " ");
        strcat(linea, args[i]);
        nArgs++;
    }

    // Separadores en ASCII: barra,comillas, comilla, espacio
    const int sep[] = {92, 34, 39, 32};

    // Verificamos si es un caso escepcional
    if (nArgs > 2)
    {
        int numeroLetrasArgs1 = strlen(args[1]);
        int permitido = 0;

        char *ruta;
        // Comillas
        if (args[1][0] == (char)sep[1])
        {
            ruta = strchr(linea, (char)(sep[1]));
            characterEraser(ruta, (char)sep[1]);
            permitido = 1;
        }
        // Comilla
        else if (args[1][0] == (char)sep[2])
        {
            ruta = strchr(linea, (char)(sep[2]));
            characterEraser(ruta, (char)sep[2]);
            permitido = 1;
        }
        // Barra
        else if (args[1][numeroLetrasArgs1 - 1] == (char)sep[0])
        {
            ruta = strchr(linea, args[1][0]);
            characterEraser(ruta, (char)sep[0]);
            permitido = 1;
        }

        // Si se permiten 2 palabras después del cd
        if (!permitido)
        {
            fprintf(stderr, "Error: Too much arguments\n");
        }
        else
        {
            if (chdir(ruta))
            {
                perror("Error");
            }
        }
    }
    // Si es una palabra
    else
    {
        if (nArgs == 1)
        {
            if (chdir(getenv("HOME")))
            {
                perror("Error");
            }
        }
        else
        {
            if (chdir(args[1]))
            {
                perror("Error");
            }
        }
    }

#if DEBUG0
    char *prompt;

    if ((prompt = malloc((sizeof(char) * COMMAND_LINE_SIZE))))
    {
        // Obtiene el directorio de trabajo actual
        getcwd(prompt, COMMAND_LINE_SIZE);

        printf("[internal_cd() → %s]\n", prompt);
    }
    else
    {
        perror("Error");
    }

    free(prompt);
#endif

    free(linea);
    return 1;
}

/**
 * Método que modifica la variable env indicada en el args con el nuevo valor
 */
int internal_export(char **args)
{
    const char *separador = "=";
    char *nombre, *valor;

    if (args[1])
    {
        nombre = strtok(args[1], separador);
        valor = strtok(NULL, separador);
    }

    if (nombre == NULL || valor == NULL)
    {
        fprintf(stderr, "Error de sintaxis\n");
    }
    else
    {
#if DEBUG1
        printf("[internal_export() → nombre: %s]\n", nombre);
        printf("[internal_export() → valor: %s]\n", valor);
        printf("[internal_export() → antiguo valor para %s: %s]\n", nombre, getenv(nombre));
#endif
        setenv(nombre, valor, 1);
#if DEBUG1
        printf("[internal_export() → nuevo valor para %s: %s]\n", nombre, getenv(nombre));
#endif
    }

    return 1;
}

/**
 * Método que hace posible la mútliple ejecución de comandos contenidos dentro de un fichero
 */
int internal_source(char **args)
{
    // Creamos la variable y reservamos memoria para leer las lineas del fichero
    char *linea = (char *)malloc(sizeof(char) * COMMAND_LINE_SIZE);

    if (linea)
    {
        // Declaramos, instanciamos y creamos el enlace al fichero a leer
        FILE *fichero = fopen(args[1], "r");

        if (fichero)
        {
            // Leemos las lineas y las ejecutamos
            while (fgets(linea, COMMAND_LINE_SIZE, fichero))
            {
                execute_line(linea);
                fflush(fichero);
            }

            fclose(fichero);
            free(linea);

            return EXIT_SUCCESS;
        }
        else
        {
            // Error al leer el fichero
            perror("Error");
            free(linea);
        }
    }

    return EXIT_FAILURE;

#if DEBUG1
    printf("[internal_source() → Esta función ejecutará un fichero de líneas de comandos]\n");
#endif

    return 1;
}

/**
 * Método que visualiza por pantalla todos los trabajos activos
 */
int internal_jobs(char **args)
{
    int id = 1;

    while (id < active_jobs)
    {
        printf("[%d] %d \t%c \t%s \n", id, jobs_list[id].pid, jobs_list[id].status, jobs_list[id].cmd);
        id++;
    }

    return EXIT_SUCCESS;
}

/**
 * Método que mueve un trabajo al foreground y espera hasta acabar
 */
int internal_fg(char **args)
{
    if (args[1])
    {
        // Obtenemos el índice del trabajo
        // Restamos 48 que es el valor del caracter '0' en ASCII
        int job_index = (int)*(args[1]) - 48;

        if (job_index > 0 && job_index <= active_jobs)
        {
            // Si el trabajo está parado, enviamos la señal para ejectuarlo
            if (jobs_list[job_index].status == STOPPED)
            {
                kill(jobs_list[job_index].pid, SIGCONT);
            }

            characterEraser(jobs_list[job_index].cmd, '&');

            // Actualizamos el foreground con el trabajo actual
            jobs_list[FOREGROUND].pid = jobs_list[job_index].pid;
            jobs_list[FOREGROUND].status = jobs_list[job_index].status;
            strcpy(jobs_list[FOREGROUND].cmd, jobs_list[job_index].cmd);

            // Eliminamos el trabajo anterior de la lista de trabajos
            jobs_list_remove(job_index);

            // Visualizamos el nuevo cmd
            printf("%s\n", jobs_list[FOREGROUND].cmd);

            // Ejecutamos pause() mientras acaba el trabajo
            while (jobs_list[FOREGROUND].pid)
            {
                pause();
            }

            return EXIT_SUCCESS;
        }

        fprintf(stderr, "fg: El trabajo %d no existe\n", job_index);
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "Error fg\n");
    return EXIT_FAILURE;
}

/**
 * Método que continua cualquier trabajo parado en el background
 */
int internal_bg(char **args)
{
    if (args[1])
    {
        // Obtenemos el índice del trabajo
        // Restamos 48 que es el valor del caracter '0' en ASCII
        int job_index = (int)*(args[1]) - 48;

        if (job_index > 0 && job_index < active_jobs)
        {
            if (jobs_list[job_index].status == STOPPED)
            {
                // Añadimos el '&' y el final de linea
                strcat(jobs_list[job_index].cmd, " &\0");

                // Actualizamos el estado del trabajo
                jobs_list[job_index].status = EXECUTED;

                // Enviamos la señal e informamos por pantalla
                kill(jobs_list[job_index].pid, SIGCONT);

#if DEBUG5
                fprintf(stderr, "[internal_fg() -> Señal %d enviada a %d (%s)\n", SIGCONT, jobs_list[job_index].pid, jobs_list[job_index].cmd);
#endif

                fprintf(stderr, "[%d] %d \t%c \t%s \n", job_index, jobs_list[job_index].pid, jobs_list[job_index].status, jobs_list[job_index].cmd);

                return EXIT_SUCCESS;
            }

            fprintf(stderr, "El trabajo %d, ya está en segundo plano\n", job_index);
            return EXIT_FAILURE;
        }

        fprintf(stderr, "bg: El trabajo %d, no existe\n", job_index);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Error bg\n");
    return EXIT_FAILURE;
}

/**
 * Método que permite la finalización del programa
 */
int internal_exit(char **args)
{
    exit(0);
    return EXIT_SUCCESS;
}

/**
 * Método que permite manejar la señal SIGCHLD
 */
void reaper(int sig_num)
{
    signal(SIGCHLD, reaper);

    pid_t ended;
    int status;

    while ((ended = waitpid(-1, &status, WNOHANG)) > 0)
    {
        // Si es un proceso en foreground
        if (ended == jobs_list[FOREGROUND].pid)
        {
            // Si el proceso ha finalizado solo
            if (WIFEXITED(status))
            {
#if DEBUG5
                printf("\n[reaper() -> Proceso hijo %d en foreground (%s) finalizado con exit code %d]\n", ended, jobs_list[FOREGROUND].cmd, WEXITSTATUS(status));
#endif
            }

            // Si el proceso ha finalizado por una señal
            else if (WIFSIGNALED(status))
            {
#if DEBUG5
                printf("\n[Proceso hijo %d (ps f) en foreground (%s) finalizado con exit code %d]\n", ended, jobs_list[FOREGROUND].cmd, WTERMSIG(status));
#endif
            }

            // Reseteamos el jobs_list[FOREGROUND]
            jobs_list[FOREGROUND].pid = FOREGROUND;
            jobs_list[FOREGROUND].status = NONE;
            memset(jobs_list[FOREGROUND].cmd, '\0', sizeof(jobs_list[FOREGROUND].cmd));
        }

        // Si es un proceso en background
        else
        {
            int posicion = jobs_list_find(ended);

            if (WIFEXITED(status))
            {
#if DEBUG5
                printf("\n[reaper() -> Proceso hijo %d (ps f) en background (%s) finalizado con exit code %d]\n", ended, jobs_list[posicion].cmd, WEXITSTATUS(status));
#endif
            }
            else if (WIFSIGNALED(status))
            {
#if DEBUG5
                printf("\n[Proceso hijo %d (ps f) en background (%s) finalizado con exit code %d]\n", ended, jobs_list[posicion].cmd, WTERMSIG(status));
#endif
            }
            printf("Proceso finalizado con PID %d (%s) en jobs_list[%d] con status %d\n", ended, jobs_list[posicion].cmd, posicion, status);

            // Eliminamos el proceso de la pila
            jobs_list_remove(posicion);
        }
    }
}

/**
 * Método que permite manejar la señal SIGINT(Ctrl + C)
 */
void ctrlc(int signum)
{
    signal(SIGINT, ctrlc);

#if DEBUG5
    printf("\n[ctrlc() → Soy el proceso con PID %d (%s), el proceso en "
           "foreground es %d(%s)]\n",
           getpid(), mini_shell, jobs_list[FOREGROUND].pid,
           jobs_list[FOREGROUND].cmd);
#endif

    // Si es un proceso hijo
    if (jobs_list[0].pid > 0)
    {
        // Verificamos si es la minishell
        if (strcmp(jobs_list[0].cmd, mini_shell))
        {
            kill(jobs_list[0].pid, SIGTERM);
        }
        else
        {
#if DEBUG4
            fprintf(stderr, "\nSeñal SIGTERM no enviada debido a que el proceso en foreground es el shell");
#endif
        }
    }
    else
    {
#if DEBUG4
        fprintf(stderr, "\nSeñal SIGTERM no enviada debido a que no hay proceso en foreground");
#endif
    }

    // Limpiamos el flujo de salida
    printf("\n");
    fflush(stdout);
}

/**
 * Método que permite manejar la señal SIGSTP(Ctrl + Z)
 */
void ctrlz(int signum)
{
    signal(SIGTSTP, ctrlz);

#if DEBUG5
    printf("\n[ctrlz() -> Soy el proceso con PID %d, el proceso en foreground es %d (%s)]\n", getpid(), jobs_list[FOREGROUND].pid, jobs_list[FOREGROUND].cmd);
#endif

    // Comprobamos si se trata de un proceso en foreground
    if (jobs_list[FOREGROUND].pid > FOREGROUND)
    {
        // Comprobamos si el hijo en el foreground no es la minishell
        if (strcmp(jobs_list[FOREGROUND].cmd, mini_shell))
        {
            // Detenemos el proceso foreground
            kill(jobs_list[FOREGROUND].pid, SIGSTOP);

#if DEBUG5
            printf("[ctrlz() -> Señal %d (SIGSTOP) enviada a %d (%s) por %d (%s)]\n", signum, jobs_list[FOREGROUND].pid, jobs_list[FOREGROUND].cmd, getpid(), mini_shell);
#endif

            // Actualizamos el proceso detenido y lo añadimos a la lista de jobs
            jobs_list[FOREGROUND].status = STOPPED;
            jobs_list_add(jobs_list[FOREGROUND].pid, jobs_list[FOREGROUND].status, jobs_list[FOREGROUND].cmd);

            // Actualizamos el foreground con sus propiedades de serie (Reset)
            jobs_list[FOREGROUND].pid = FOREGROUND;
            jobs_list[FOREGROUND].status = NONE;
            memset(jobs_list[FOREGROUND].cmd, '\0', sizeof(jobs_list[FOREGROUND].cmd));
        }
        else
        {
            // Visualizamos el error
            printf("ctrlz() -> Señal %d (SIGTSTP) no enviada debido a que el proceso en foreground es el shell\n", signum);
        }
    }
    else
    {
        // Visualizamos el error
        printf("ctrlz() -> Señal %d (SIGTSTP) no enviada debido a que no hay proceso en el foreground\n", signum);
    }

    fflush(stdout);
}

/// Operadores de la PILA jobs_list[] ///

/**
 * Método que añade un nuevo trabajo a la última posición de jobs_list y actualiza active_jobs
 */
int jobs_list_add(pid_t pid, char status, char *cmd)
{
    if (active_jobs < N_JOBS)
    {
        jobs_list[active_jobs].status = status;
        jobs_list[active_jobs].pid = pid;
        strcpy(jobs_list[active_jobs].cmd, cmd);

        active_jobs++;

        return EXIT_SUCCESS;
    }
    else
    {
        fprintf(stderr, "Se ha llegado al máximo de procesos permitidos.");
        return EXIT_FAILURE;
    }
}

/**
 * Método que busca y devuelve la posición del trabajo en la pila jobs_list
 */
int jobs_list_find(pid_t pid)
{
    for (int i = 1; i < N_JOBS; i++)
    {
        if (jobs_list[i].pid == pid)
        {
            return i;
        }
    }
    return -1;
}

/**
 * Método que elimina un trabajo de la lista y añade el último active_job en la posición correspondiente
 */
int jobs_list_remove(int pos)
{
    if (pos > 0 && pos < N_JOBS)
    {
        // Obtenemos la información del último trabajo
        memset(jobs_list[pos].cmd, '\0', COMMAND_LINE_SIZE);
        jobs_list[pos].pid = '\0';
        jobs_list[pos].status = '\0';

        // Actualizamos el active_jobs
        active_jobs--;

        // Añadimos el último proceso de la lista a la posición que hemos vaciado
        if (active_jobs != 0)
        {
            strcpy(jobs_list[pos].cmd, jobs_list[active_jobs].cmd);
            jobs_list[pos].pid = jobs_list[active_jobs].pid;
            jobs_list[pos].status = jobs_list[active_jobs].status;
        }

        return EXIT_SUCCESS;
    }
    else
    {
        fprintf(stderr, "La posición es incorrecta. No se puede acceder a jobs_list[%d]\n", pos);
        return EXIT_FAILURE;
    }
}

/**
 * Método que borra un caracter de un "array/puntero"
 */
void characterEraser(char *args, char caracter)
{
    int index = 0;
    int new_index = 0;

    while (args[index])
    {
        if (args[index] != caracter)
        {
            args[new_index] = args[index];
            new_index++;
        }

        index++;
    }

    args[new_index] = '\0';
}

/**
 * Método para remplazar una subcadena por otra subcadena en una cadena
 */
char *replaceWord(const char *cadena, const char *cadenaAntigua, const char *nuevaCadena)
{
    char *result;
    int i, cnt = 0;
    int newWlen = strlen(nuevaCadena);
    int oldWlen = strlen(cadenaAntigua);

    // Contando el número de veces palabra antigua que sale en el String
    for (i = 0; cadena[i] != '\0'; i++)
    {
        if (strstr(&cadena[i], cadenaAntigua) == &cadena[i])
        {
            cnt++;
            // Saltar al índice después de la palabra antigua.
            i += oldWlen - 1;
        }
    }

    // Reserva de espacio suficiente para la nueva cadena
    if ((result = malloc(i + cnt * (newWlen - oldWlen) + 1)))
    {
        i = 0;
        while (*cadena)
        {
            // Comparar la subcadena con el resultado
            if (strstr(cadena, cadenaAntigua) == cadena)
            {
                strcpy(&result[i], nuevaCadena);
                i += newWlen;
                cadena += oldWlen;
            }
            else
            {
                result[i++] = *cadena++;
            }
        }

        result[i] = '\0';
    }
    else
    {
        perror("Error");
    }

    return result;
}