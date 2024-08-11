t_log* logger;
t_log* logger_obligatorio;
t_config* config;

const char* interfaces[4] = {"GENERICA", "STDIN", "STDOUT", "DIALFS"};
const char* nombres[4] = {"GENERICA", "TECLADO", "MONITOR", "DIALFS"};

char* tipo_interfaz;
int tiempo_unidad_trabajo;
char* ip_kernel; 
char* puerto_kernel;
char* ip_memoria;
char* puerto_memoria;
char* path_base_dialfs;
int block_size;
int block_count;
int retraso_compactacion;

int memoria_fd;
int kernel_fd;

t_bitarray* bit_map;

typedef struct{
    char* nombre;
    int bloque_inicial;
    int tamanio_archivo;
}t_fcb;

t_list* lista_fcbs;
void* fs;
void* fs_bloques;

int fd_bit_map;
int tamanio_bit_map;

int fd_bloques;

t_config* iniciar_config(char* path_config);
void copiar_config();
void copiar_config_dialfs();
void crear_conexiones();
char* recibir_parametros_interfaz();
int caso_interfaz(char* nombre_interfaz);
void interfaz_generica(int conexionKernel);
void interfaz_stdin (int conexionKernel, int conexionMemoria);
void interfaz_stdout(int conexionKernel, int conexionMemoria);
void interfaz_filesystem(int kernel_fd, int memoria_fd);
void copiar_config_metadata(int* bloque_inicial, int* tamanio_archivo, t_config* config_al_archivo);
void crear_bit_map();
void crear_archivo_bloques();
void asignar_bloques_al_archivo(int* bloque_inicial, int* tamanio_archivo, int cantidad_bloques);
void asignar_contiguos(int bloque_final, int cantidad_bloques);
void leer_bloque (int bloque_a_leer, char* bloque_leido,int tamanio, int desplazamiento);
void escribir_bloque(int bloque_a_escribir, char* contenido, int tamanio, int desplazamiento);
bool buscar_contiguos(int cantidad_bloques, int* index);
t_fcb* buscar_fcb(char* nombre_archivo, int* index);
void liberar_bloques (int bloque_inicial, int tamanio_archivo, int cant_bloques_a_eliminar);
int espacio_disponible();
int dar_bloque_libre();
void hacer_compactacion(t_fcb* fcb_no_truncar);
void sincronizar_archivo_bloques();
void sincronizar_bitmap();
bool comprobar_contiguos(int bloques_a_agregar, t_fcb* fcb);
int recibir_io_gen_sleep(int conexionKernel);
int file_exists_in_directory(const char* directory, const char* filename);
void recuperar_lista();
void asignar_bloques(int bloque, int cantidad_bloques);
void terminar_fs();
void terminar_ENTRADASALIDA();