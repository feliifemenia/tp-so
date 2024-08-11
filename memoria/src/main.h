t_log* logger;
t_log* logger_obligatorio;
t_config* config;

char* puerto_escucha;
int tamaño_memoria;
int tamaño_pagina;
char* path_instrucciones;
int retardo_respuesta;

int memoria_fd;
int cpu_fd;
int kernel_fd;
int stdin_fd;
int stdout_fd;
int filesystem_fd;
//array de MEMORIA y CPU

const char* array_de_instrucciones[19] = {"SET", "SUM", "SUB", "MOV_IN", "MOV_OUT", "RESIZE", "JNZ", "COPY_STRING", "IO_GEN_SLEEP", "IO_STDIN_READ", 
                        "IO_STDOUT_WRITE", "IO_FS_CREATE","IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE","IO_FS_READ", "WAIT", 
                        "SIGNAL", "EXIT"};

void* bits;
t_bitarray* bit_map;

struct t_tabla_instrucciones{
    int pid;
    t_list *instrucciones;
    t_list *paginas;
};

int cant_paginas;
t_list *lista_tabla_instrucciones;
void *espacio_memoria;

void entradas_salidas_memoria(t_log* logger, char* name, int socket);
int esperar_cliente_memoria(char* name, int server_Socket);
void hacer_hilo(int* socket);
t_config* iniciar_config();
void inicializar_variables();
void copiar_config();
void crear_conexiones();
int agregar_proceso(t_pcb* pcb, FILE* archivo);
int dar_pagina_libre();
char* cambiar_instruccion(char* inst, int* operacion);
int cant_de_elem_en_string(char* string, char elem);
void liberar_memoria_por_pid(int pid_actual);
void liberar_procesos();
int cantidad_paginas_asignadas(int pid);
int verificar_out_of_memory();
void hacer_resize(int pid, int size);
int agregar_pagina_al_proceso(int pid, int size);
void quitar_pagina_al_proceso(int pid,int size);
void funcion_kernel();
void procesar_kernel();
void clientecpu();
void procesar_cpu();
void procesar_stdin(int* socket);
void cliente_stdin(void* args);
void procesar_stdout(int* socket);
void cliente_stdout(void* args);
void procesar_fs(int* socket);
void cliente_fs(void* args);
int buscar_marco(int numero_pagina, int pid);
void agregar_dato_a_pagina(int direccion_fisica, void* dato, int tamanio, int pid);
void* leer_dato_de_pagina(int tamanio, int direccion_fisica, int pid);
void terminar_MEMORIA();


pthread_mutex_t mutex_lista_TIP;
pthread_mutex_t mutex_void;