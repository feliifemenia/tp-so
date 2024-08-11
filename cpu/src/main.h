//void recibir_mensaje(int socket_cliente);
t_config* iniciar_config();
void copiar_config();
void crear_conexiones();
void procesar_proceso();
void aumentar_pc(t_pcb** pcb, int cod_op,char* instruccion);
bool check_interrupt(int socket);
char* fase_fetch(t_pcb* pcb, int* cod_op);
int comprobar_valor(t_pcb* pcb, char* a_comprobar);
void fase_decode_execute(t_pcb** pcb,char* instrucciones, int cod_op,bool* estado, t_temporal* quantum);
int obtener_valor_de_registro(t_pcb* pcb, char* registro);
void cambiar_valor_registro(t_pcb* pcb, char* registro, int valor);
int obtener_tamanio_registro(char* registro);
void operar_set(t_pcb* pcb,char* registro, int valor);
void operar_sum(t_pcb* pcb, char* registro, int valor);
void operar_sub(t_pcb* pcb, char* registro, int valor);
void operar_jnz(t_pcb* pcb, char* registro, int valor);
void operar_mov_in(t_pcb* pcb, char* registro_datos, char* registro_direccion);
void operar_mov_out(t_pcb* pcb, char* registro_direccion, char* registro_datos);
void operar_copy_string(t_pcb* pcb, int tamanio);
void operar_io_std(t_pcb* pcb, char* nombre_io, char* registro_direccion, char* registro_tamanio, int socket, int cod_op);
void operar_io_fs(t_pcb* pcb, char* nombre_io, char* nombre_archivo, char* registro_direccion, char* registro_tamanio, char* registro_puntero, int cod_op1, int cod_op2);
void operar_semaforo(t_pcb* pcb, char* recurso, int cod_op);
int MMU(t_pcb* pcb, int pagina, int desplazamiento);
void iniciar_tlb();
int TLB(int pid, int pagina);
void agregar_a_tlb(int pagina, int pid, int direccion_fisica);
int obtener_cantidad_accesos(int direccion_logica_inicial, int direccion_logica_final);
void modificar_quantum(t_pcb** pcb, t_temporal* quantum);
void terminar_CPU();

//array de MEMORIA y CPU

const char* array_de_instrucciones[19] = {"SET", "SUM", "SUB", "MOV_IN", "MOV_OUT", "RESIZE", "JNZ", "COPY_STRING", "IO_GEN_SLEEP", "IO_STDIN_READ", 
                        "IO_STDOUT_WRITE", "IO_FS_CREATE","IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE","IO_FS_READ", "WAIT", 
                        "SIGNAL", "EXIT"};

t_log* logger;
t_log* logger_obligatorio;
t_config* config;

int memoria_fd;
int kernel_dispatch_fd;
int kernel_interrupt_fd;
int cpu_dispatch_fd;
int cpu_interrupt_fd;

char* ip_memoria;
char* puerto_memoria;
char* puerto_escucha_dispatch;
char* puerto_escucha_interrupt;
int cantidad_entradas_TLB;
char* algoritmo_TLB;
int tamaño_pagina;

t_list* lista_tlb;

struct t_direcciones{
    int pid;
    int pagina;
    int marco;
};
