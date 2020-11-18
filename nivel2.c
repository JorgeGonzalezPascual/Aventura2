// NIVEL 2

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEBUG 1
#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define PROMPT '$'

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
char line[COMMAND_LINE_SIZE];

// Funciones
char *read_line(char *line);
int execute_line(char *line);
int parse_args(char **args, char *line);
int check_internal(char **args);
int internal_cd(char **args);
int internal_export(char **args);
int internal_source(char **args);
int internal_jobs(char **args);

/*
* Main del programa.
*/
int main()
{
    while (1)
        if (read_line(line))
            execute_line(line);

    return 0;
}

/*
*   Método para imprimir el PROMPT
*/
void imprimir_prompt()
{

    //Get USERNAME
    char *user = getenv("USER");

    char *prompt = malloc(sizeof(char) * COMMAND_LINE_SIZE);

    // Gets the current work directory.
    getcwd(prompt, COMMAND_LINE_SIZE);
    // Prints the prompt and the separator.
    printf(BLOND RED "%s:" BLUE "%s " COLOR_RESET YELLOW "%c: " COLOR_RESET, user, prompt, PROMPT);

    // frees the memory for prompt.
    free(prompt);

    fflush(stdout);
}

/*
Leer una linea de la consola
*/
char *read_line(char *line)
{
    imprimir_prompt();

    // Reads input introduced in stdin by the user.
    // Control de errores
    if (fgets(line, COMMAND_LINE_SIZE, stdin) == NULL)
    {
        perror("Error");
    }

    return line;
}

/*
*
*/
int execute_line(char *line)
{

    //Reservamos memoria para los tokens

    char **args = malloc(sizeof(char *) * ARGS_SIZE);
    if (args == NULL)
    {
        fprintf(stderr, "Memoria dinamica llena.");
    }

    if (args)
    {
        //Parseamos
        parse_args(args, line);
        if (args[0])
        {
            check_internal(args);
        }
    }
    //Liberamos memoria
    free(args);
}

//{line hacer esto #para esto} args[x][0]
int parse_args(char **args, char *line)
{
    int nToken = 0;
    const char s[5] = " \t\r\n";
    char *token;

    token = strtok(line, s);
    args[nToken] = token;

    while (token != NULL)
    {

#if DEBUG
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
#if DEBUG
            printf("[parse_args() → token %d corregido: %s]\n", nToken, token);
#endif
        }

        //Siguiete
        token = strtok(NULL, s);
        nToken++;
    }
    return nToken;
}

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

/*
Utiliza la llamada al sistema chdir() para cambiar de directorio
*/
int internal_cd(char **args)
{
    // falta control de error
    char *linea = malloc(sizeof(char) * COMMAND_LINE_SIZE);
    // Separadores: comilla,comillas, barra
    const int sep[] = {34, 39, 92};

    if (args[2]) {
        fprintf(stderr, "Error: Too much arguments\n");
    } else {
        if (args[1] == NULL) {
            if (chdir(getenv("HOME"))) {
                perror("Error");
            }
        } else {
            if (chdir(args[1])) {
                perror("Error");
            }
        }
    }

    free(linea);

    return 1;
}

int internal_export(char **args) {
    const char *separador= "=";
    char *nombre, *valor;

    if (args[1]) {
        nombre = strtok(args[1], separador);
        valor = strtok(NULL, separador);
    }

    if (nombre == NULL || valor == NULL){
        fprintf(stderr, "Error de sintaxis");
    }else { 
        printf("[internal_export() → nombre: %s]\n", nombre);
        printf("[internal_export() → valor: %s]\n", valor);
        printf("[internal_export() → antiguo valor para %s: %s]\n", nombre, getenv(nombre));
        setenv(nombre, valor, 1);
        printf("[internal_export() → nuevo valor para %s: %s]\n", nombre, getenv(nombre));
    }

    return 1;
}

int internal_source(char **args)
{
#if DEBUG
    printf("[internal_source() → Esta función ejecutará un fichero de líneas de comandos]\n");
#endif
    return 1;
}

int internal_jobs(char **args)
{
#if DEBUG
    printf("[internal_jobs() → Esta función mostrará el PID de los procesos que no estén en foreground]\n");
#endif
    return 1;
}

int internal_fg(char **args)
{
#if DEBUG
    printf("[internal_fg() → Esta función parsará/activará a primer plano procesos]\n");
#endif
    return 1;
}

int internal_bg(char **args)
{
#if DEBUG
    printf("[internal_bg() → Esta función parsará/activará a segundo plano procesos]\n");
#endif
    return 1;
}

int internal_exit(char **args)
{
#if DEBUG
    printf("[internal_exit() → Esta función sale del mini shell]\n");
#endif
    exit(0);
    return 1;
}