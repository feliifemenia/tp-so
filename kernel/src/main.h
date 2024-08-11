const char* cod_op_array[7] = {"EJECUTAR_SCRIPT", "INICIAR_PROCESO", "FINALIZAR_PROCESO", "DETENER_PLANIFICACION", "INICIAR_PLANIFICACION", "MULTIPROGRAMACION","PROCESO_ESTADO"};

const char* array_motivo_exit[6] = {"NA","SUCCESS","INVALID_RESOURCE","INVALID_INTERFACE","OUT_OF_MEMORY","INTERRUPTED_BY_USER"};

const char* array_estados[5] = {"NEW","READY","BLOCKED","EXECUTE","EXIT"};
typedef enum {
    NEW = 0,
    READY,
    BLOCKED,
    EXEC,
    EXIT_PROCESO
}ESTADO_PROCESO;

typedef enum{
    FIFO,
    RR,
    VRR
}ALGORITMO;

const char* algoritmo_array[3] = {"FIFO", "RR", "VRR"};
struct t_consola{
    int codigo_operacion;
    char* parametro;
};

struct t_interfazData{
    char* nombre;
    int socket;
};

//FUNCIONES
t_config* iniciar_config();
void copiar_config();
void inicializar_variables();
void crear_conexiones();
void consola();
static void procesar_consola();
void iniciar_proceso(char* path);
void guardar_script(char* path,bool* script_activo);
void detener_planificacion();
void iniciar_planificacion();
void mostrar_proceso_estado();
struct t_consola obtener_codop_consola(char* codop);
char* cod_op_script(bool* script_activo);
void modificar_grado_multiprogramacion(int grado_nuevo);
void registros(t_contexto* contexto);
struct t_pcb* crear_pcb(char* path);
void planificar();
void planificar_largo_plazo();
void FuncionReadyConsola();
void Funcionblock();
void FuncionExit();
void liberar_recursos(t_pcb* pcb);
void planificar_corto_plazo();
void funcion_usleep(void* args);
int elegir_algoritmo(char* algoritmo_config);
void guardar_segun_algoritmo(t_pcb* pcb);
t_pcb* verificar_lista_aux();
void finalizar_proceso(int pid);
bool buscar_pid(int pid);
void procesar_conexiones();
void actualizar_sleep();
void manejar_io_stdin(void* void_args);
void manejar_io_stdout(void* void_args);
void manejar_io_gen_sleep(void* void_args);
void manejar_wait(void* void_args);
void manejar_fs(void* void_args);
void bloquear_segun_recurso(t_pcb* pcb, char* recurso, int cardinal);
void manejar_signal(void* void_args);
void desbloquear_segun_recurso(char* recurso, int cardinal);
void manejar_fin_quantum(void* void_args);
void conexiones();
bool verificar_io(char* nombreIo, int* socket);
void logear_ready(bool param);
void liberar_diagrama();
void print_new(void* data);
void print_ready(void* data);
void print_bloq_gen(void* data);
void print_bloq_stdin(void* data);
void print_bloq_stdout(void* data);
void print_bloq_recursos(void* data);
void liberar_diccionario();
void terminar_kernel();


//VARIABLES GLOBALES

bool planificacion_activa;
bool finalizar_un_proceso = false;

bool no_presente = false;
bool bool_sleep = false;

t_log* logger;
t_log* logger_obligatorio;
t_config* config;

t_list* lista_new;
t_list* lista_ready;
t_list* lista_exit;
t_pcb* exec_global = NULL;

t_list* lista_aux_vrr;
t_list* lista_generica;
t_list* lista_stdin;
t_list* lista_stdout;
t_list* lista_io_filesystem;

t_list* lista_recursos;
t_list* lista_script;
struct t_nodo_recurso{
    char* recurso;
    t_list* bloqueados_x_recursos;
    int* pids_utilizando;
};

t_dictionary* diccionario_entradasalidas;

char* ip_memoria;
char* ip_cpu;
char* algoritmo_planificacion;
char** recursos;
char** instancias_recursos;
int* instancias_recursos_int;
char* puerto_escucha;
char* puerto_memoria;
char* puerto_cpu_dispatch;
char* puerto_cpu_interrupcion;
int Quantum;
int grado_multiprogramacion;
int grado_multiprogramacion_disminuido;
char* path_scripts;

int memoria_fd;
int cpu_dispatch_fd;
int cpu_interrupt_fd;
int kernel_fd;
int contador_pid;


//SEMAFOROS
sem_t sem_multiprogramacion;
sem_t sem_nuevo_proceso;
sem_t sem_pasar_procesador;
sem_t sem_recibirCpu;
sem_t sem_exit_pcb;
sem_t sem_ready;

sem_t sem_fin_proceso;
sem_t sem_sleep;

//SEMAFOROS FRENAR PLANIFICACION

sem_t detener_planificador_corto_plazo;//detiene el paso de ready a exec
sem_t detener_planificador_new;//detiene el paso de new a ready
sem_t detener_manejo_motivo;//detiene el manejo del motivo de desalojo del pcb que estaba en exec
sem_t detener_ready_generica;//corta el paso de blocked generica a ready
sem_t detener_ready_stdin;//corta el paso de blocked stdin a ready
sem_t detener_ready_stdout;//corta el paso de blocked stdout a ready
sem_t detener_ready_recursos;//corta el paso de blocked recurso a ready si es bloqueante
sem_t detener_ready_filesystem;//corta el paso de blocked fs a ready

int contador_generica;
int contador_stdin;
int contador_stdout;
int contador_recursos;
int contador_filesystem;
int pid_a_finalizar;

pthread_mutex_t mutex_lista_ready;
pthread_mutex_t mutex_lista_new;
pthread_mutex_t mutex_lista_exit;
pthread_mutex_t mutex_lista_aux_vrr;
pthread_mutex_t mutex_lista_recursos;
pthread_mutex_t mutex_contador_pid;
pthread_mutex_t* mutex_recursos;

pthread_mutex_t mutex_sleep;

pthread_mutex_t mutex_cola_generica;
pthread_mutex_t mutex_cola_stdin;
pthread_mutex_t mutex_cola_stdout;
pthread_mutex_t mutex_cola_filesystem;
pthread_mutex_t mutex_send_generica;
pthread_mutex_t mutex_send_stdin;
pthread_mutex_t mutex_send_stdout;
pthread_mutex_t mutex_send_filesystem;
pthread_mutex_t mutex_cola_fs;
