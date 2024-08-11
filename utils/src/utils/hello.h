#ifndef UTILS_HELLO_H_
#define UTILS_HELLO_H_

#include <stdlib.h>
#include <stdio.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include<commons/config.h>
#include<readline/readline.h>
#include<pthread.h>
#include<semaphore.h>
#include<commons/string.h>
#include<commons/temporal.h>
#include<commons/bitarray.h>
#include<commons/memory.h>
#include<math.h>
//Orden de inicio
// MEMO/CPU/KERNEL/ENTRADASALIDA

extern t_log* logger;
extern t_config* config;

//------------------------Enums--------------------------------

typedef enum {
    //estos enums son de kernel
    EJECUTAR_SCRIPT,
    INICIAR_PROCESO,
    FINALIZAR_PROCESO,
    DETENER_PLANIFICACION,
    INICIAR_PLANIFICACION,
    MULTIPROGRAMACION,
    PROCESO_ESTADO,
    //enums que usamos todos
    MENSAJE, 
    PAQUETE, 
    MANEJAR_IO_GEN_SLEEP,
    MANEJAR_IO_STDIN_READ,
    MANEJAR_IO_STDOUT_WRITE, 
    MANEJAR_WAIT,
    MANEJAR_SIGNAL,
    MANEJAR_FS,
    //enums de memoria
    CREAR_PCB,
    ELIM_PCB,
    //enums de cpu
    EXEC_PCB,
    EXIT_PCB,
    FIN_QUANTUM
}COD_OP; // GLOBAL

typedef enum{
    NA,
    SUCCESS,
    INVALID_RESOURCE,
    INVALID_INTERFACE,
    OUT_OF_MEMORY,
    INTERRUPTED_BY_USER
}MOTIVO_EXIT; // CPU - KERNEL

typedef enum{
    NO_BLOQUEADO,
    INTERFAZ,
    RECURSO
}MOTIVO_BLOQUEO; // CPU - KERNEL

typedef enum {
    SET, SUM, SUB,
    MOV_IN, MOV_OUT, RESIZE, 
    JNZ, COPY_STRING, IO_GEN_SLEEP, 
    IO_STDIN_READ, IO_STDOUT_WRITE, IO_FS_CREATE,
    IO_FS_DELETE, IO_FS_TRUNCATE, IO_FS_WRITE,
    IO_FS_READ, WAIT, SIGNAL,
    EXIT, NUEVA_INSTRUCCION, DIRECCION_FISICA, 
    ESCRIBIR, LECTURA
}e_instrucciones; // MEMORIA - CPU

typedef enum{
    GENERICA,
    STDIN,
    STDOUT,
    DIALFS
}INTERFACES; // KERNEL - IO
                        
//----------------------Struct procesos---------------------
typedef struct{
    u_int32_t pc;
    u_int8_t ax;
    u_int8_t bx;
    u_int8_t cx;
    u_int8_t dx;
    u_int32_t eax;
    u_int32_t ebx;
    u_int32_t ecx;
    u_int32_t edx;
    u_int32_t si;
    u_int32_t di;
}t_registros_cpu;

typedef struct{
    int pid;
    int estado;
    int motivo_exit;
    int motivo_bloqueo;
    t_registros_cpu* Registros_CPU;
}t_contexto;

typedef struct{
    u_int64_t quantum;
    t_contexto* contexto;
}t_pcb;

//-----------------------Structs Paquetes----------------------------
typedef struct
{
	int size;
	void* stream;
}t_buffer;

typedef struct
{
	int codigo_operacion;
	t_buffer* buffer;
}t_paquete;

//---------------------Argumentos de hilos--------------------
typedef struct {
    t_log* log;
    int Socket_fd;
    char* server_name;
}t_pthread_arguments_conexion;


void decir_hola(char* quien);
int iniciar_socket(char* Puerto, char* Mensaje);
int esperar_Socket(t_log* logger, char* name, int server_Socket);
int crear_conexion(char *ip, char* puerto);
void liberar_conexion(int socket_cliente);
void crear_buffer(t_paquete* paquete);
t_paquete* crear_paquete(int cod_op);
void* serializar_paquete(t_paquete* paquete, int bytes);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void paquete(int);
t_list* recibir_paquete(int);
int recibir_operacion(int);
void* recibir_buffer(int*, int);
void recibir_mensaje(t_log*,int);
void enviar_mensaje(char* mensaje, int socket_cliente);
void iterator(char* value);
void entradas_salidas(t_log* logger,t_dictionary*, char* name, int socket);
int esperar_cliente(t_dictionary* diccionario, char* name, int server_Socket);
void agregar_dic_io(int socket,t_dictionary* diccionario);
void liberar_pcb(t_pcb* pcb);

//Funciones de deserializacion
t_registros_cpu* deserializar_registros_cpu(void* buffer, int size, int *desplazamiento);
t_contexto* deserializar_contexto(void* buffer, int size, int *desplazamiento);
t_pcb* deserializar_pcb(int socket, void* buffer, int *desplazamiento, int size);
char* deserializar_path(void* buffer, int* desplazamiento);
char* deserializar_string(void* buffer, int* desplazamiento);
void* deserializar_void(void* buffer, int* desplazamiento);
int deserializar_int(void* buffer, int* desplazamiento);

//Funciones de serializacion
void empaquetar_registros(t_paquete* paquete, t_registros_cpu* registros);
void empaquetar_contexto_ejecucion(t_paquete* paquete, t_contexto* contexto);
void empaquetar_pcb(t_pcb* pcb, t_paquete* paquete);


//PROTOCOLOS
void send_crear_pcb(int socket, t_pcb* pcb, char* path);
char* recv_crear_pcb(int socket, t_pcb** pcb);
void send_io_gen_sleep(int socket_cliente, int cantidad_unidades_trabajo, int mensaje, int pid);
int recv_sleep(int socket, int* pid);
void send_nombre_entradasalida(char* nombre, int socket, int codigo_operacion);
char* recv_nombre_entradasalida(int socket);
void send_pcb(t_pcb* pcb, int socket, COD_OP codigo_operacion);
t_pcb* recv_pcb(int socket);
t_pcb* recv_pcb_sin_op(int socket);
void send_limpiar_memoria(int pid,int socket);
int recv_limpiar_memoria(int socket);
void send_dos_int(int pc,int pid, int socket,int codop);
void recv_dos_int(int* pid,int* pc, int socket);
void send_resize(int pid, int size,int socket);
int recv_resize(int* pid, int socket);
void send_instruccion(char* instruccion, int socket,int cod_op);
char* recv_instruccion(int socket);
void send_ins_io_gen_sleep(char* nombre_io, int cantidad_unidades_trabajo, int socket);
char* recv_ins_io_gen_sleep(int* cantidad_unidades_trabajo, int socket);
void send_notificacion(char* instruccion, int socket,int cod_op);
char* recv_notificacion(int socket);
void send_void(void* instruccion, int tamanio, int socket,int cod_op);
void* recv_void(int socket);
void send_interrupt(int socket);
int recv_interrupt(int socket_cliente);
void send_int(int int_a_enviar, int socket, int cod_op);
int recv_int(int socket);
void send_std_kernel(int pid, int direccion_fisica, int tamanio, int socket, int cod_op);
int recv_std_kernel(int* direccion_fisica, int* tamanio, int socket);
void send_stdin(char* texto_a_guardar, int pid, int direccion_fisica, int tamanio, int socket, int cod_op);
char* recv_stdin(int* direccion_fisica, int* pid, int* tamanio, int socket);
void send_stdout(int pid, int direccion_fisica, int tamanio, int socket, int cod_op);
int recv_stdout(int* direccion_fisica,  int* tamanio, int socket);
void send_std_cpu(int direccion_fisica, int tamanio, char* nombre_io, int socket, int cod_op);
char* recv_std_cpu(int* direccion_fisica, int* tamanio, int socket);
void send_df_dato(int direccion_fisica, void* dato, int tamanio, int socket, int cod_op);
int recv_df_dato(void** dato, int socket);
void send_mov_out(int direccion_fisica, int dato, int tamanio, int socket, int cod_op);
int recv_mov_out(int* dato, int* tamanio, int socket);
void send_paquete_df(t_list* lista_df, t_list* lista_tam, int cant_accesos, char* nombre_io, int cod_op, int socket);
char* recv_paquete_df(int* cant_accesos, t_list* lista_df, t_list* lista_tam, int socket);
void send_int_char(int entero, char* cadena, int cod_op, int socket);
int recv_int_char(char* cadena, int socket);
#endif
