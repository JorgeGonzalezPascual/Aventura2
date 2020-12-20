// NIVEL 5

//Librerias
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

//Constantes
#define DEBUG0 0
#define DEBUG1 0
#define DEBUG2 0
#define DEBUG3 0
#define DEBUG4 0
#define DEBUG5 1

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

//Variables
// Estructura para almacenar los procesos
struct info_process
{
    pid_t pid;
    char status;                 //N (Ninguno), E (Ejecutando), D (Detenido), F (Finalizado)
    char cmd[COMMAND_LINE_SIZE]; //Líniea de comando
};

static struct info_process jobs_list[N_JOBS];
static struct info_process foreground;
static struct info_process mini_shell;
static int active_jobs = 1;
int n_pids;

//Funciones
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

void borradorCaracter(char *args, char caracter);
char *replaceWord(const char *cadena, const char *cadenaAntigua, const char *nuevaCadena);
int is_background(char *line);

int jobs_list_add(pid_t pid, char status, char *cmd);
int jobs_list_find(pid_t pid);
int jobs_list_remove(int pos);

// -----------------
int chdir();
int getcwd();
int setenv();
int fork();
int execvp();
int getppid();
int getpid();
int pause();
// -----------------

/**
 * Método para imprimir el PROMPT
**/
void imprimir_prompt()
{
    // Obtenemos el nombre de usuario
    char *user = getenv("USER");
    char *prompt;

    if ((prompt = malloc((sizeof(char) * COMMAND_LINE_SIZE) - sizeof(user))))
    {
        //Obtener el directorio de trabajo actual.
        getcwd(prompt, COMMAND_LINE_SIZE);
        if (strcmp(prompt, getenv("HOME")))
        {
            if (strstr(prompt, getenv("HOME")))
            {
                prompt = replaceWord(prompt, getenv("HOME"), "~");
            }
        }

        //Imprimimos el el PROMPT "personalizado"
        printf(BLOND RED "%s:" BLUE "%s " COLOR_RESET YELLOW "%c: " COLOR_RESET, user, prompt, PROMPT);
    }
    else
    {
        perror("Error");
    }

    free(prompt);
    fflush(stdout);
}

/**
 * Leer una linea de la consola
 */
char *read_line(char *line)
{
    // Generamos el prompt
    imprimir_prompt();
    // Obtenemos la linea de la terminal
    char *ptr = fgets(line, COMMAND_LINE_SIZE, stdin);

    // Leer la entrada introducida en stdin por el usuario
    // Control de errores
    if (ptr)
    {
        printf("\r\n");
        if (feof(stdin))
        {
            #if DEBUG3
                printf("Adeu\n");
            #endif
            exit(0);
        }
        else
        {
            perror("Error");
        }
    }
    return line;
}

/**
 * 
 */
int execute_line(char *line)
{
    //Reservamos memoria para los tokens
    char **args = malloc(sizeof(char *) * ARGS_SIZE);

    if (args != NULL)
    {
        int bckgrd = is_background(line);
        //Copiamos en la "pila" la línea sin el caracter "\n" antes de parsear
        borradorCaracter(line, '\n');
        strcpy(jobs_list[FOREGROUND].cmd, line);

        //Parseamos
        parse_args(args, line);
        if (args[0])
        {
            if (!check_internal(args))
            {
                int state;
                pid_t pid = fork();

                #if DEBUG4
                    printf("jobs_list[0].command_line: %s\n", jobs_list[0].cmd);
                #endif

                //Hijo
                if (pid == 0)
                {
                    //Asignamos señales
                    //Si el proceso esta en backgroud
                    if (bckgrd)
                    {
                        signal(SIGTSTP, SIG_IGN);
                    }
                    else
                    {
                        signal(SIGTSTP, SIG_DFL);
                    }
                    signal(SIGINT, SIG_IGN);
                    signal(SIGCHLD, SIG_DFL);

#if DEBUG4
                    printf("[execute_line() → PID padre: %d] (%s)\n", getppid(), jobs_list[1].cmd);
                    printf("[execute_line() → PID hijo: %d] (%s)\n", getpid(), jobs_list[0].cmd);
#endif

                    if (execvp(args[0], args) < 0)
                    {
                        fprintf(stderr, "Error al leer el comando externo: %s.\n", args[0]);
                        //Terminación anormal
                        exit(EXIT_FAILURE);
                    }
                    // Salimos sin errores (Terminación Normal)
                    exit(EXIT_SUCCESS);
                }
                //Padre
                else if (pid > 0)
                {
                    //Si es un proceso está en segundo plano, se agrega a jobs_list.
                    if (bckgrd)
                    {
                        jobs_list_add(pid, EXECUTED, line);
                    }
                    else
                    {
                        //Copiamos el padre en la pila
                        jobs_list[FOREGROUND].pid = pid;
                        jobs_list[FOREGROUND].status = EXECUTED;
                        strcpy(jobs_list[FOREGROUND].cmd, line);

                        //Mientras haya un proceso foreground
                        while (jobs_list[FOREGROUND].pid > 0)
                        {
                            pause();
                        }

                        //Hijo ha terminado de manera normal
                        if (WIFEXITED(state))
                        {
                            #if DEBUG3
                                printf("[EL proceso hijo %d ha finalizado con exit(), estado: %d]\n", pid, WEXITSTATUS(state));
                            #endif
                        }
                        //Hijo ha finalizado por señal
                        if (WIFSIGNALED(state))
                        {
                            #if DEBUG3
                                printf("[El proceso hijo %d ha finalizado por señal, estado: %d]\n", pid, WTERMSIG(state));
                            #endif
                        }
                        /*
                        jobs_list[FOREGROUND].pid = foreground.pid;
                        jobs_list[FOREGROUND].status = foreground.status;
                        strcpy(jobs_list[FOREGROUND].cmd, line);
                        */
                    }
                }
                //Error de fork()
                else
                {
                    perror("Error fork");
                    //Terminación anormal
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "Memoria dinámica llena.\n");
    }
    //Liberamos memoria
    free(args);

    return EXIT_SUCCESS;
}

/**
 * Método que mira si es un comando en segundo plano, es decir que si tiene el & al final
 **/
int is_background(char *line)
{
    int numeroLetras = strlen(line);

    if (line[numeroLetras - 2] == '&')
    {
        line[numeroLetras - 2] = '\0';
#if DEBUG5
        printf("Es un proceso en backgroud\n");
#endif
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * 
 * 
 **/
int parse_args(char **args, char *line)
{
    int nToken = 0;
    const char s[5] = " \t\r\n";
    char *token;

    token = strtok(line, s);
    args[nToken] = token;

    while (token != NULL)
    {

#if DEBUG1
        printf("[parse_args() → token %d: %s]\n", nToken, token);
#endif
        //Descartamos comentarios
        if (*(token) != '#')
        {
            args[nToken] = token;
        }
        else
        {
            //Añadimos NULL
            token = NULL;
            args[nToken] = token;
#if DEBUG1
            printf("[parse_args() → token %d corregido: %s]\n", nToken, token);
#endif
        }

        //Siguiete
        token = strtok(NULL, s);
        nToken++;
    }

    return nToken;
}

/**
 * Chequeamos si es un comando interno
 **/
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
 * Método que borra un caracter de un "array/puntero"
 **/
void borradorCaracter(char *args, char caracter)
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
 * Función internal_cd
 * -----------------------------------------------------------------
 * Utiliza la llamada al sistema chdir() para cambiar de directorio
 * 
 * Input:
 * Output:
 **/

int internal_cd(char **args)
{
    // falta control de error
    char *linea = malloc(sizeof(char) * COMMAND_LINE_SIZE);
    if (linea == NULL)
    {
        fprintf(stderr, "No hay memoria dinámica disponible en este momento.\n");
    }

    //Concatenamos los args
    for (int i = 0; args[i]; i++)
    {
        strcat(linea, " ");
        strcat(linea, args[i]);
    }

    // Separadores en ASCII: barra,comillas,comilla, espacio
    const int sep[] = {92, 34, 39, 32};

    if (args[2] != NULL)
    {
        //Miramos si es un caso escepcional
        int numeroLetrasArgs1 = strlen(args[1]);
        int permitido = 1;
        //miramos comilla o comillas

        char *ruta;
        //comilla
        if (args[1][0] == (char)sep[1])
        {
            ruta = strchr(linea, (char)(sep[1]));
            borradorCaracter(ruta, (char)sep[1]);
        }
        //comillas
        else if (args[1][0] == (char)sep[2])
        {
            ruta = strchr(linea, (char)(sep[2]));
            borradorCaracter(ruta, (char)sep[2]);
        }
        //barra
        else if (args[1][numeroLetrasArgs1 - 1] == (char)sep[0])
        {
            ruta = strchr(linea, args[1][0]);
            borradorCaracter(ruta, (char)sep[0]);
        }
        else
        {
            permitido = 0;
        }

        //Si se permiten 2 palabras después del cd
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
    //Si es una palabra
    else
    {
        if (args[1] == NULL)
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
        // Gets the current work directory.
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
 * Función internal_export
 **/
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

int internal_source(char **args)
{
    // Creamos la variable y reservamos memoria para leer las lineas del fichero
    char *linea = (char *)malloc(sizeof(char) * COMMAND_LINE_SIZE);

    if (linea)
    {
        // Declaramos, instanciamos y creamos el enlace al fichero a leer
        FILE *fichero = fopen(args[1], "r");

        // Si existe el fichero
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

int internal_fg(char **args)
{
    if (args) 
    {
        // Obtenemos el índice del trabajo 
        int job_index = (int) *(args[1]);

        if (job_index > 0 && job_index < active_jobs)
        {
            // Si el trabajo está parado, enviamos la señal para efectuarlo
            if (jobs_list[job_index].status == STOPPED) 
            {
                kill(jobs_list[job_index].pid, SIGCONT);
            }

            // Eliminamos el '&' en caso de que esté presente
            /*
            char *caracter = strchr(jobs_list[FOREGROUND].cmd, '&');
            if (!caracter) 
            {
                *(caracter - 1) = '\0';
            }
            */
            borradorCaracter(jobs_list[FOREGROUND].cmd, '&');
           

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

        fprintf(stderr, "bg: El trabajo %d no existe\n", job_index);
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "Error\n");
    return EXIT_FAILURE;
}

int internal_bg(char **args)
{
    if (args[1]) 
    {
        int job_index = (int) *(args[1]);

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

                fprintf(stderr, "[internal_fg() -> Señal %d enviada a %d (%s)\n", SIGCONT, jobs_list[job_index].pid, jobs_list[job_index].cmd);
                return EXIT_SUCCESS;
            }
            fprintf(stderr, "El trabajo %d, ya está en segundo plano\n", job_index);
            return EXIT_FAILURE;
        }
        fprintf(stderr, "bg: El trabajo %d, no existe\n", job_index);
        return EXIT_FAILURE;
    }
    fprintf(stderr, "Error\n");
    return EXIT_FAILURE;
}

int internal_exit(char **args)
{
#if DEBUG1
    printf("[internal_exit() → Esta función sale del mini shell]\n");
#endif
    exit(0);
    return 1;
}

/**
 * Manejador propio para la señal SIGCHLD, (esta pendientre si finaliza un hijo)
 */
void reaper(int sig_num)
{
    pid_t ended;
    int status;

    //ESTO CREO QUE DEBERIA IR ABAJO
    //ADEMAS AHORA HABRIA QUE IMPLEMENTAR LO NUEVO (EN EL CASO DEL DRIVE LO QUE ESTA EN)
    //PARA RESETEAR EL JOBSLIST[0] PID = 0 Y MEMESET DEL CMD (podriamos hacer un metodo)
    jobs_list[FOREGROUND].pid = 0;
    jobs_list[FOREGROUND].status = FINALIZED;
    memset(jobs_list[FOREGROUND].cmd, '\0', strlen(jobs_list[0].cmd));

    while ((ended = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (ended == jobs_list[FOREGROUND].pid)
        {
            printf("[reaper() -> Proceso hijo %d (ps f) en foreground (%s) finalizado con exit code %d]\n", ended, jobs_list[FOREGROUND].cmd, WEXITSTATUS(status));

            // Reseteamos el jobs_list[FOREGROUND]
            jobs_list[FOREGROUND].pid = foreground.pid;
            jobs_list[FOREGROUND].status = foreground.status;
            strcpy(jobs_list[FOREGROUND].cmd, foreground.cmd);
        }
        else
        {
            int posicion = jobs_list_find(ended);

            if (WIFEXITED(status))
            {
#if DEBUG4
                printf("[reaper() -> Proceso hijo %d (ps f) en background (%s) finalizado con exit code %d]\n", ended, jobs_list[posicion].cmd, WEXITSTATUS(status));
#endif
            }
            else if (WIFSIGNALED(status))
            {
#if DEBUG4
                printf("[reaper() -> Proceso hijo %d (ps f) en background (%s) finalizado con exit code %d]\n", ended, jobs_list[posicion].cmd, WTERMSIG(status));
#endif
            }

            jobs_list_remove(posicion);
        }
    }
    // Establecemos de nuevo la señal SIGCHLD para el reaper()
    signal(SIGCHLD, reaper);
}

/**
 * Manejador propio de la señal SIGINT(Ctrl + C)
 */
void ctrlc(int signum)
{
    signal(SIGINT, ctrlc);

    // Si es un proceso hijo
    if (jobs_list[FOREGROUND].pid > 0)
    {
        // Verificamos si es la minishell
        if (strcmp(jobs_list[FOREGROUND].cmd, mini_shell.cmd))
        {
            kill(jobs_list[FOREGROUND].pid, SIGTERM);
        }
        else
        {
#if DEBUG4
            fprintf(stderr, "\nSeñal SIGTERM no enviada debido a que el proceso en foreground es el shell\n");
#endif
        }
    }
    else
    {
#if DEBUG4
        fprintf(stderr, "\nSeñal SIGTERM no enviada debido a que no hay proceso en foreground\n");
#endif
    }

    printf("\n");
    fflush(stdout);
}

void ctrlz(int signum)
{
    printf("\n[ctrlz() -> Soy el proceso con PID %d, el proceso en foreground es %d (%s]\n", getpid(), jobs_list[FOREGROUND].pid, jobs_list[FOREGROUND].cmd);

    // Comprobamos si se trata de un proceso en foreground
    if (jobs_list[FOREGROUND].pid != foreground.pid)
    {
        // Comprobamos si el hijo en el foreground no es la minishell
        if (strcmp(jobs_list[FOREGROUND].cmd, mini_shell.cmd))
        {
            // Detenemos el proceso foreground
            kill(jobs_list[FOREGROUND].pid, SIGTSTP);

            // Actualizamos el proceso detenido y lo añadimos a la lista de jobs
            jobs_list[FOREGROUND].status = STOPPED;
            jobs_list_add(jobs_list[FOREGROUND].pid, jobs_list[FOREGROUND].status, jobs_list[FOREGROUND].cmd);

            // Actualizamos el foreground con sus propiedades de serie
            jobs_list[FOREGROUND].pid = foreground.pid;
            jobs_list[FOREGROUND].status = foreground.status;
            strcpy(jobs_list[FOREGROUND].cmd, foreground.cmd);

            printf("[ctrlz() -> Señal %d (SIGTSTP) enviada a %d (%s) por %d (%s)]\n", signum, jobs_list[FOREGROUND].pid, jobs_list[FOREGROUND].cmd, getpid(), mini_shell.cmd);
        }
        else
        {
            // Visualizamos el error
            printf("ctrlz() -> Señal %d (SIGTSTP) no enviada debido a que el proveso en foreground es el shell\n", signum);
        }
    }
    else
    {
        // Visualizamos el error
        printf("ctrlz() -> Señal %d (SIGTSTP) no enviada debido a que no hay proceso en el foreground\n", signum);
    }

    signal(SIGTSTP, ctrlz);
}

/**
 * Método para remplazar una subcadena por otra subcadena en una cadena
 **/
char *replaceWord(const char *cadena, const char *cadenaAntigua, const char *nuevaCadena)
{
    char *result;
    int i, cnt = 0;
    int newWlen = strlen(nuevaCadena);
    int oldWlen = strlen(cadenaAntigua);

    // Contando el número de veces palabra antigua
    // que sale en el String
    for (i = 0; cadena[i] != '\0'; i++)
    {
        if (strstr(&cadena[i], cadenaAntigua) == &cadena[i])
        {
            cnt++;
            //Saltar al índice después de la palabra antigua.
            i += oldWlen - 1;
        }
    }

    //Reserva de espacio suficiente para la nueva cadena
    if ((result = malloc(i + cnt * (newWlen - oldWlen) + 1)))
    {

        i = 0;
        while (*cadena)
        {
            //Comparar la subcadena con el resultado
            if (strstr(cadena, cadenaAntigua) == cadena)
            {
                strcpy(&result[i], nuevaCadena);
                i += newWlen;
                cadena += oldWlen;
            }
            else
                result[i++] = *cadena++;
        }

        result[i] = '\0';
    }
    else
    {
        perror("Error");
    }

    return result;
}

int jobs_list_add(pid_t pid, char status, char *cmd)
{
    if (n_pids < N_JOBS)
    {
        n_pids++;
        jobs_list[n_pids].status = status;
        jobs_list[n_pids].pid = pid;
        strcpy(jobs_list[n_pids].cmd, cmd);
        return 1;
    }
    else
    {
        fprintf(stderr, "Se ha llegado al máximo de procesos permitidos.");
        return 0;
    }
}

int jobs_list_find(pid_t pid)
{
    for (int i = 0; i < N_JOBS; i++)
    {
        if (jobs_list[i].pid == pid)
        {
            return i;
        }
    }
    return -1;
}

int jobs_list_remove(int pos)
{
    if (pos >= 0)
    {
        //Eliminamos el proceso de la posición indicada por parametro
        memset(jobs_list[pos].cmd, '\0', COMMAND_LINE_SIZE);
        jobs_list[pos].pid = '\0';
        jobs_list[pos].status = '\0';

        //Añadimos el último proceso de la lista a la posición que hemos vaciado
        n_pids--;
        strcpy(jobs_list[n_pids].cmd, jobs_list[pos].cmd);
        jobs_list[pos].pid = jobs_list[n_pids].pid;
        jobs_list[pos].status = jobs_list[n_pids].status;
        return 1;
    }
    else
    {
        fprintf(stderr, "La posición introducida es incorrecta. Introduce un valor válido\n");
        return 0;
    }
}

/**
 * Main del programa
 **/
int main(int argc, char *argv[])
{
    // Inicializamos la pila del minishell
    mini_shell.pid = getpid();
    mini_shell.status = EXECUTED;
    strcpy(mini_shell.cmd, argv[0]);

    // Inicializamos la pila de foreground
    foreground.pid = FOREGROUND;
    foreground.status = EXECUTED;
    foreground.cmd[0] = '\0';

    //Manejadores de señales para el padre o para el shell
    signal(SIGCHLD, reaper);
    signal(SIGINT, ctrlc);
    signal(SIGTSTP, ctrlz);

    //Inicializamos la pila de jobs_list
    jobs_list[FOREGROUND].pid = 0;
    jobs_list[FOREGROUND].status = NONE;
    memset(jobs_list[FOREGROUND].cmd, '\0', strlen(jobs_list[FOREGROUND].cmd));

    char *cmd = (char *)malloc(sizeof(char) * COMMAND_LINE_SIZE);

    // Si hay memoria para el cmd
    if (cmd)
    {
        while (read_line(cmd))
        {
            execute_line(cmd);
        }
    }

    // Liberamos la memoria al finalizar
    free(cmd);

    return EXIT_FAILURE;
}