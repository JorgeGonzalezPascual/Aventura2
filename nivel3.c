/**
 * Sistemas Operativos - AVENTURA 2
 * 
 * NIVEL 3
 * 
 * Jorge González Pascual - Lluís Barca Pons - Joan Martorell Ferriol
 */

//Librerias
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

//Constantes
#define DEBUG0 0
#define DEBUG1 0
#define DEBUG2 0
#define DEBUG3 1

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

// -----------------
int chdir();
int getcwd();
int setenv();
int fork();
int execvp();
int getppid();
int getpid();
// -----------------

/**
 * Main del programa
 **/
int main()
{
    while (1)
        if (read_line(line))
            execute_line(line);
    return 0;
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

/**
 * Método para imprimir el PROMPT
**/
void imprimir_prompt()
{

    //Obetener USERNAME
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

        //Printeamos el el PROMPT "personalizado"
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
**/
char *read_line(char *line)
{
    imprimir_prompt();
    char* ptr = fgets(line, COMMAND_LINE_SIZE, stdin);
   
    //Leer la entrada introducida en stdin por el usuario
    // Control de errores
    if(ptr == NULL){
        printf("\r\n");
        if(feof(stdin)){
            printf("Adeu\n");
            exit(0);
        }else{
            perror("Error");
        }
    }

    return line;
}

/**
**
**/
int execute_line(char *line)
{

    //Reservamos memoria para los tokens
    char **args = malloc(sizeof(char *) * ARGS_SIZE);

    if (args != NULL)
    {
        //Parseamos
        parse_args(args, line);
        if (args[0])
        {
            if (!check_internal(args))
            {
                int state;
                pid_t pid = fork();

                //Hijo
                if (pid == 0)
                {
                    #if DEBUG3
                        printf("[execute_line() → PID padre: %d]\n", getppid());
                        printf("[execute_line() → PID hijo: %d]\n", getpid());
                    #endif
                    
                    if(execvp(args[0], args)){
                        fprintf(stderr, "Error al leer el comando externo: %s.\n",args[0]);
                        //Terminación anormal
                        exit(EXIT_FAILURE);
                    }
                    //Salimos sin errores (Terminación Normal)
                    exit(EXIT_SUCCESS);
                    
                }
                //Padre
                else if (pid > 0)
                {
                    pid  = wait(&state);
                    //Hijo ha terminado de manera normal
                    if(WIFEXITED(state)){
                        #if DEBUG3
                            printf("[EL proceso hijo %d ha finalizado con exit(), estado: %d]\n",pid ,WEXITSTATUS(state));
                        #endif
                    }
                    //Hijo ha finalizado por señal
                    if(WIFSIGNALED(state)){
                        #if DEBUG3
                            printf("[EL proceso hijo %d ha finalizado por final, estado: %d]\n",pid ,WTERMSIG(state));
                        #endif
                    }
                }
                //Error de fork()
                else{
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
 * 
 * 
 **/
int parse_args(char **args, char *line)
{   
    char lineaux[COMMAND_LINE_SIZE];
    strcpy(lineaux, line);  // Dejamos line sin modificar con el comando entero
    int i = 0;
    char *token = strtok(lineaux, " \n\r\t");

    while (token != NULL) {

        args[i] = token;

        printf("[parse_args() --> token%i = %s] \n",i,token);

        // Si no es un comentario lo añadimos como argumento
        if (strncmp(args[i],"#",1) == 0) { 

            break;   
        }

        i++;
        // Ponemos NULL para no sobreescribir
        token = strtok(NULL, " \n\r\t");
    }

    // Null al final, ya que no habrá nada más que trocear
    args[i] =  0; 
    // Le quitamos el salto de línea a line
    strtok(line, "\n\r"); 

    return i;    
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
    int indice = 0;
    int indiceNuevo = 0;

    while (args[indice])
    {
        if (args[indice] != caracter)
        {
            args[indiceNuevo] = args[indice];
            indiceNuevo++;
        }
        indice++;
    }
    args[indiceNuevo] = 0;
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
 * -----------------------------------------------------------------
 * RELLENO
 * 
 * Input:
 * Output:
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

    if (linea) {
        // Declaramos, instanciamos y creamos el enlace al fichero a leer
        FILE *fichero = fopen(args[1], "r");

        // Si existe el fichero
        if(fichero) {
            // Leemos las lineas y las ejecutamos
            while (fgets(linea, COMMAND_LINE_SIZE, fichero)) {
                execute_line(linea);
                fflush(fichero);
            }

            fclose(fichero);
            free(linea);

            return EXIT_SUCCESS;
        } else {
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
#if DEBUG1
    printf("[internal_jobs() → Esta función mostrará el PID de los procesos que no estén en foreground]\n");
#endif
    return 1;
}

int internal_fg(char **args)
{
#if DEBUG1
    printf("[internal_fg() → Esta función parsará/activará a primer plano procesos]\n");
#endif
    return 1;
}

int internal_bg(char **args)
{
#if DEBUG1
    printf("[internal_bg() → Esta función parsará/activará a segundo plano procesos]\n");
#endif
    return 1;
}

int internal_exit(char **args)
{
#if DEBUG1
    printf("[internal_exit() → Esta función sale del mini shell]\n");
#endif
    exit(0);
    return 1;
}