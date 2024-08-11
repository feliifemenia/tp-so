#include <utils/hello.h>


void decir_hola(char* quien) {
    printf("Hola desde %s!!\n", quien);
}

int iniciar_socket(char* Puerto, char* server_name)
{
    int Nuevo_Socket;

    struct addrinfo hints, *ServerInfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, Puerto, &hints, &ServerInfo);

    Nuevo_Socket = socket(ServerInfo->ai_family, ServerInfo->ai_socktype, ServerInfo->ai_protocol);
    bind(Nuevo_Socket, ServerInfo->ai_addr, ServerInfo->ai_addrlen);


    listen(Nuevo_Socket, SOMAXCONN);

    freeaddrinfo(ServerInfo);
    log_trace(logger, "%s listo para escuchar al cliente", server_name);

    return Nuevo_Socket;
}

int esperar_Socket(t_log* logger, char* name, int server_Socket){
    // Aceptamos un nuevo cliente
	int socket_Aceptado = accept(server_Socket, NULL, NULL);
	log_info(logger, "Cliente conectado (a %s)", name);
    return socket_Aceptado;
}

int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// Ahora vamos a crear el socket.
	int socket_cliente = socket(server_info->ai_family,
                    server_info->ai_socktype,
                    server_info->ai_protocol);
	//int socket_cliente = 0;
	socket_cliente = socket(server_info->ai_family,
                    server_info->ai_socktype,
                    server_info->ai_protocol);

	// Ahora que tenemos el socket, vamos a conectarlo
	connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

	freeaddrinfo(server_info);

	return socket_cliente;
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}

//PROTOCOLOS


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(int cod_op)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = cod_op;
	crear_buffer(paquete);
	return paquete;
}


void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);


	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void paquete(int conexion)
{
	// Ahora toca lo divertido!
	char* leido;
	t_paquete* paquete = crear_paquete(PAQUETE);

	// Leemos y esta vez agregamos las lineas al paquete

	leido = readline("> ");
	while(leido[0]!='\0'){
		agregar_a_paquete(paquete,leido,strlen(leido)+1);
		leido = readline("> ");
	}
	enviar_paquete(paquete,conexion);
	printf("paquete enviado");
	// ¡No te olvides de liberar las líneas y el paquete antes de regresar!
	free(leido);
	free(paquete);
}

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void recibir_mensaje(t_log* logger, int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Me llego el mensaje %s", buffer);
	free(buffer);
} 

int recibir_operacion(int socket_cliente){
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}



void enviar_mensaje(char* mensaje, int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);
	
	free(a_enviar);
	eliminar_paquete(paquete);
}

void iterator(char* value) {
	log_info(logger,"%s", value);
}

void entradas_salidas(t_log* logger, t_dictionary* diccionario, char* name, int socket){

    log_info(logger,"%s listo para escuchar I/O", name); 
    
    while(esperar_cliente(diccionario, name, socket)){

        log_info(logger,"%s listo para escuchar I/O", name); 
    }

}

int esperar_cliente(t_dictionary* diccionario, char* name, int server_Socket){
    int Sock_fd = esperar_Socket(logger, name, server_Socket);

    if(Sock_fd != -1){ 
        agregar_dic_io(Sock_fd, diccionario);
        return 1;
    }
    return 0;
}

void agregar_dic_io(int socket, t_dictionary* diccionario){
    int interfaz;
    char* nombre;
    interfaz = recibir_operacion(socket);
    nombre = recv_nombre_entradasalida(socket);
    switch (interfaz) { 
        case GENERICA : 
			
            dictionary_put(diccionario, "nombreGenerica", nombre);
            dictionary_put(diccionario, "socketGenerica", socket);
            dictionary_put(diccionario, "estadoGenerica", 1);
            break;
        case STDIN :
            dictionary_put(diccionario, "nombreStdin", nombre);
            dictionary_put(diccionario, "socketStdin", socket);
            dictionary_put(diccionario, "estadoStdin", 1);
            break;
        case STDOUT :
            dictionary_put(diccionario, "nombreStdout", nombre);
            dictionary_put(diccionario, "socketStdout", socket);
            dictionary_put(diccionario, "estadoStdout", 1);
            break;
        case DIALFS:
            dictionary_put(diccionario, "nombreFilesystem", nombre);
            dictionary_put(diccionario, "socketFilesystem", socket);
            dictionary_put(diccionario, "estadoFilesystem", 1);
            break;
        default: 
            log_error (logger, "Hubo un error!");
			exit(EXIT_FAILURE);
            break;
    }
}

void liberar_pcb(t_pcb* pcb){
    free(pcb -> contexto -> Registros_CPU);
    free(pcb -> contexto);
    free(pcb);
}

//Funciones de deserializacion 

t_pcb* deserializar_pcb(int socket, void* buffer, int* desplazamiento, int size){
	
	int tamanio = 0;
	t_pcb* pcb = malloc(sizeof(t_pcb));

	pcb -> contexto = malloc(sizeof(t_contexto));
	pcb -> contexto -> Registros_CPU = malloc(sizeof(t_registros_cpu));

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	memcpy(&(pcb->quantum), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto->pid), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

    // Deserializar el estado
	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto->estado), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

    // Deserializar el motivo_exit
	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto->motivo_exit), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

    // Deserializar el motivo_bloqueo
	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> motivo_bloqueo), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	//registros

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU -> pc), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->ax), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->bx), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->cx), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->dx), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->eax), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->ebx), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->ecx), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->edx), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->si), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
    memcpy(&(pcb -> contexto -> Registros_CPU ->di), buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;


	return pcb;
} 

char* deserializar_path(void* buffer, int* desplazamiento){

	int tamanio;
	
	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);

	char* path = malloc(sizeof(tamanio));

	memcpy(path, buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;
	
	return path;
}

int deserializar_int(void* buffer, int* desplazamiento){
    int tamanio = 0;
    int nuevo_int = 0;

    memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	memcpy(&nuevo_int, buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

    return nuevo_int;
}

char* deserializar_string(void* buffer, int* desplazamiento){

	int tamanio = 0;
	
	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);

	char* string = malloc(tamanio);

	memcpy(string, buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;
	
	return string;
}

void* deserializar_void(void* buffer, int* desplazamiento){

	int tamanio,dato;
	
	memcpy(&tamanio, buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);

	void* void_ = malloc(sizeof(tamanio));
	
	memcpy(void_, buffer + *desplazamiento, tamanio);
    *desplazamiento += tamanio;

	memcpy(&dato,void_,tamanio);

	return void_;
}

//Funciones de serializacion

void empaquetar_registros(t_paquete* paquete, t_registros_cpu* registros){
	agregar_a_paquete(paquete, &(registros -> pc), sizeof(u_int32_t));
	agregar_a_paquete(paquete, &(registros -> ax), sizeof(u_int8_t));
	agregar_a_paquete(paquete, &(registros -> bx), sizeof(u_int8_t));
	agregar_a_paquete(paquete, &(registros -> cx), sizeof(u_int8_t));
	agregar_a_paquete(paquete, &(registros -> dx), sizeof(u_int8_t));
	agregar_a_paquete(paquete, &(registros -> eax), sizeof(u_int32_t));
	agregar_a_paquete(paquete, &(registros -> ebx), sizeof(u_int32_t));
	agregar_a_paquete(paquete, &(registros -> ecx), sizeof(u_int32_t));
	agregar_a_paquete(paquete, &(registros -> edx), sizeof(u_int32_t));
	agregar_a_paquete(paquete, &(registros -> si), sizeof(u_int32_t));
	agregar_a_paquete(paquete, &(registros -> di), sizeof(u_int32_t));
}

void empaquetar_contexto_ejecucion(t_paquete* paquete, t_contexto* contexto){
	agregar_a_paquete(paquete, &(contexto -> pid), sizeof(int));
	agregar_a_paquete(paquete, &(contexto -> estado), sizeof(int));
	agregar_a_paquete(paquete, &(contexto -> motivo_exit), sizeof(int));
	agregar_a_paquete(paquete, &(contexto -> motivo_bloqueo), sizeof(int));
	empaquetar_registros(paquete, contexto -> Registros_CPU);
}

void empaquetar_pcb(t_pcb* pcb, t_paquete* paquete){
	agregar_a_paquete(paquete, &(pcb -> quantum), sizeof(u_int64_t));
	empaquetar_contexto_ejecucion(paquete, pcb -> contexto);
}

//Funciones de protocolos

//Funciones para mandar y "crear" pcbs

void send_crear_pcb(int socket, t_pcb* pcb, char* path){
	t_paquete* paquete = crear_paquete(CREAR_PCB);
	empaquetar_pcb(pcb, paquete);
	agregar_a_paquete(paquete, path, strlen(path) + 1);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

char* recv_crear_pcb(int socket, t_pcb** pcb){

	int desplazamiento = 0;
	int size;
	void* buffer;

	buffer = recibir_buffer(&size, socket);

	*pcb = deserializar_pcb(socket, buffer, &desplazamiento, size);    
	
	desplazamiento = 0;
	return deserializar_path(buffer + 120, &desplazamiento);
}

//Funciones para mandar la cant. de unidades de trabajo y entradasalida lo recive

void send_io_gen_sleep(int socket_cliente, int cantidad_unidades_trabajo, int mensaje, int pid){
	t_paquete* paquete = crear_paquete(mensaje);
	agregar_a_paquete(paquete, &cantidad_unidades_trabajo, sizeof(int));
	agregar_a_paquete(paquete, &pid, sizeof(int));
	enviar_paquete(paquete, socket_cliente);
	eliminar_paquete(paquete);
}

int recv_sleep(int socket, int* pid){
	int size;
	int desplazamiento = 0;
	void * buffer;
	int tamanio;
	int cantidad_unidades_trabajo;
	
	buffer = recibir_buffer(&size, socket);
	
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(&cantidad_unidades_trabajo, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(pid, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
	free(buffer);

	return cantidad_unidades_trabajo;
}

//Funcion para mandar nombre de entradasalida y recibirla en kernel

void send_nombre_entradasalida(char* nombre, int socket, int codigo_operacion){
	t_paquete* paquete = crear_paquete(codigo_operacion);
	agregar_a_paquete(paquete, nombre, strlen(nombre) + 1);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

char* recv_nombre_entradasalida(int socket){
	
	int size, desplazamiento = 0;
	
	void* buffer = recibir_buffer(&size, socket);
	return deserializar_string(buffer,&desplazamiento);
}

//Funciones para mandar solo un pcb y recibir un pcb

void send_pcb(t_pcb* pcb, int socket, COD_OP codigo_operacion){
	t_paquete* paquete = crear_paquete(codigo_operacion);
	empaquetar_pcb(pcb, paquete);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

t_pcb* recv_pcb(int socket){
	int desplazamiento = 0;
	int size = 0;
	int cod_op = recibir_operacion(socket);
	if(cod_op == -1)
		return NULL;
		
	void* buffer = recibir_buffer(&size, socket);
	t_pcb* pcb = deserializar_pcb(socket, buffer, &desplazamiento, size);
	free(buffer);
	return pcb;
}

t_pcb* recv_pcb_sin_op(int socket){
	int desplazamiento = 0;
	int size = 0;
	void* buffer = recibir_buffer(&size, socket);
	t_pcb* pcb = deserializar_pcb(socket, buffer, &desplazamiento, size);
	free(buffer);
	return pcb;
}

//Funciones para enviar o recibir "eliminar procesos"
void send_limpiar_memoria(int pid,int socket){
	t_paquete* paquete = crear_paquete(ELIM_PCB);
	agregar_a_paquete(paquete, &pid, sizeof(pid));
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

int recv_limpiar_memoria(int socket){
	int size;
	int desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
    int number =  deserializar_int(buffer, &desplazamiento);
	
	free(buffer);
	
	return number;
}

//Funciones para enviar o recibir dos int
void send_dos_int(int pc, int pid,int socket,int codop){
    t_paquete* paquete = crear_paquete(codop);
    agregar_a_paquete(paquete, &pc, sizeof(pc));
 	agregar_a_paquete(paquete, &pid, sizeof(pid));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void recv_dos_int(int* pid,int* pc, int socket){
    int size;
	int desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
    *pc = deserializar_int(buffer, &desplazamiento);
	*pid = deserializar_int(buffer, &desplazamiento);

    free(buffer);
}

//Funciones para enviar y recibir el resize
void send_resize(int pid, int size,int socket){
    t_paquete* paquete = crear_paquete(RESIZE);
 	agregar_a_paquete(paquete, &pid, sizeof(pid));
    agregar_a_paquete(paquete, &size, sizeof(size));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

int recv_resize(int* pid, int socket){
	int size, desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
    *pid = deserializar_int(buffer, &desplazamiento);
	int resize = deserializar_int(buffer, &desplazamiento);

    free(buffer);
	return resize;
}

//Funciones para enviar o recibir instrucciones

void send_instruccion(char* instruccion, int socket,int cod_op){
    t_paquete* paquete = crear_paquete(cod_op);
    agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

char* recv_instruccion(int socket){
	char* instruccion = NULL;
    int size = 0, desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
    instruccion = deserializar_string(buffer, &desplazamiento);
    free(buffer);

	return instruccion;
}

//Funciones para mandar instruccion io_gen_sleep de procesador a kernel

void send_ins_io_gen_sleep(char* nombre_io, int cantidad_unidades_trabajo, int socket){
	t_paquete* paquete = crear_paquete(MANEJAR_IO_GEN_SLEEP);
	agregar_a_paquete(paquete, &cantidad_unidades_trabajo, sizeof(int));
	agregar_a_paquete(paquete, nombre_io, strlen(nombre_io) + 1);
	
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

char* recv_ins_io_gen_sleep(int* cantidad_unidades_trabajo, int socket){
	int size;
	int tamanio;
	int desplazamiento = 0;
	
	void* buffer = recibir_buffer(&size, socket);
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(cantidad_unidades_trabajo, buffer + desplazamiento, tamanio);
	desplazamiento+=sizeof(int);

	return deserializar_string(buffer, &desplazamiento);
}

//Funciones para mandar y recibir la interrupcion por quantum

void send_interrupt(int socket){
	t_paquete* paquete = crear_paquete(FIN_QUANTUM);
	enviar_paquete(paquete, socket);
	// agregar_a_paquete(paquete,"Hola",5);
	eliminar_paquete(paquete);
}

int recv_interrupt(int socket_cliente){
	int cod_op;
	int resultado = -100;

	resultado = recv(socket_cliente, &cod_op, sizeof(int), MSG_DONTWAIT);

	if(resultado > 0)
		return cod_op;
	else{
		return -1;
	}
}

//-------

void send_notificacion(char* instruccion, int socket,int cod_op){
    t_paquete* paquete = crear_paquete(cod_op);
    agregar_a_paquete(paquete, instruccion, strlen(instruccion)+1);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

char* recv_notificacion(int socket){
	char* instruccion;
    int size, desplazamiento = 0;
	recibir_operacion(socket);
    void* buffer = recibir_buffer(&size, socket);
    instruccion = deserializar_string(buffer, &desplazamiento);
    free(buffer);

	return instruccion;
}

//
void send_void(void* instruccion, int tamanio, int socket,int cod_op){
    t_paquete* paquete = crear_paquete(cod_op);
    agregar_a_paquete(paquete, instruccion, tamanio);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

void* recv_void(int socket){
	void* instruccion;
    int size, desplazamiento = 0;
	recibir_operacion(socket);
    void* buffer = recibir_buffer(&size, socket);
    instruccion = deserializar_void(buffer, &desplazamiento);
    free(buffer);

	return instruccion;
}

//FUNCIONES PARA ENVIAR INT

void send_int(int int_a_enviar, int socket, int cod_op){
	t_paquete* paquete = crear_paquete(cod_op);
    agregar_a_paquete(paquete, &int_a_enviar, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

int recv_int(int socket){
	int size = 0, int_nuevo = 0, desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
    int_nuevo = deserializar_int(buffer, &desplazamiento);
    free(buffer);

	return int_nuevo;
}

// FUNCIONES PARA STD

//Kernel - IO

void send_std_kernel(int pid, int direccion_fisica, int tamanio, int socket, int cod_op){
	t_paquete* paquete = crear_paquete(cod_op);
	agregar_a_paquete(paquete,&pid, sizeof(pid));
	agregar_a_paquete(paquete, &direccion_fisica , sizeof (direccion_fisica));
	agregar_a_paquete(paquete, &tamanio , sizeof (tamanio));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

int recv_std_kernel(int* direccion_fisica, int* tamanio, int socket){
	int size, desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
    int pid = deserializar_int(buffer, &desplazamiento);
	*direccion_fisica = deserializar_int(buffer, &desplazamiento);
	*tamanio = deserializar_int(buffer, &desplazamiento);
    free(buffer);

	return pid;
}

//IO - Memoria

void send_stdin(char* texto_a_guardar, int pid, int direccion_fisica, int tamanio, int socket, int cod_op){
	t_paquete* paquete = crear_paquete(cod_op);
    agregar_a_paquete(paquete, texto_a_guardar, strlen (texto_a_guardar) + 1);
	agregar_a_paquete(paquete, &pid , sizeof (pid));
	agregar_a_paquete(paquete, &direccion_fisica , sizeof (direccion_fisica));
	agregar_a_paquete(paquete, &tamanio , sizeof (tamanio));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

char* recv_stdin(int* direccion_fisica, int* pid, int* tamanio, int socket){
	int size, desplazamiento = 0;
	char* texto;
    void* buffer = recibir_buffer(&size, socket);
	texto = deserializar_string(buffer, &desplazamiento); 
    *pid = deserializar_int(buffer,  &desplazamiento);
	*direccion_fisica = deserializar_int(buffer,  &desplazamiento);
	*tamanio = deserializar_int(buffer, &desplazamiento);
    free(buffer);

	return texto;
}

void send_stdout(int pid, int direccion_fisica, int tamanio, int socket, int cod_op){
	t_paquete* paquete = crear_paquete(cod_op);
	agregar_a_paquete(paquete, &pid , sizeof (pid));
	agregar_a_paquete(paquete, &direccion_fisica , sizeof (direccion_fisica));
	agregar_a_paquete(paquete, &tamanio , sizeof (tamanio));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

int recv_stdout(int* direccion_fisica,  int* tamanio, int socket){
	int size, pid, desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
    pid = deserializar_int(buffer,  &desplazamiento);
	*direccion_fisica = deserializar_int(buffer, &desplazamiento);
	*tamanio = deserializar_int(buffer, &desplazamiento);
    free(buffer);

	return pid;
}

// DE CPU A KERNEL

void send_std_cpu(int direccion_fisica, int tamanio, char* nombre_io, int socket, int cod_op){
	t_paquete* paquete = crear_paquete(cod_op);
	agregar_a_paquete(paquete, &direccion_fisica, sizeof(direccion_fisica));
	agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
	agregar_a_paquete(paquete, nombre_io, strlen(nombre_io) + 1);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

char* recv_std_cpu(int* direccion_fisica, int* tamanio, int socket){
	int size, desplazamiento = 0;
	char* texto;
    void* buffer = recibir_buffer(&size, socket);
	*direccion_fisica = deserializar_int(buffer, &desplazamiento);
	*tamanio = deserializar_int(buffer, &desplazamiento);
	texto = deserializar_string(buffer, &desplazamiento);
    free(buffer);

	return texto;
}

//

void send_df_dato(int direccion_fisica, void* dato, int tamanio, int socket, int cod_op){
	t_paquete* paquete = crear_paquete(cod_op);
	agregar_a_paquete(paquete, &direccion_fisica, sizeof(direccion_fisica));
	agregar_a_paquete(paquete, dato, tamanio);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

int recv_df_dato(void** dato, int socket){
	int size, desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
	int direccion_fisica = deserializar_int(buffer, &desplazamiento);
	*dato = deserializar_string(buffer, &desplazamiento);
    free(buffer);
	return direccion_fisica;
}
//
void send_mov_out(int direccion_fisica, int dato, int tamanio, int socket, int cod_op){
	t_paquete* paquete = crear_paquete(cod_op);
	agregar_a_paquete(paquete, &direccion_fisica, sizeof(direccion_fisica));
	agregar_a_paquete(paquete, &dato, sizeof(int));
	agregar_a_paquete(paquete, &tamanio, sizeof(int));
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

int recv_mov_out(int* dato, int* tamanio, int socket){
	int size, desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
	int direccion_fisica = deserializar_int(buffer, &desplazamiento);
	*dato = deserializar_int(buffer, &desplazamiento);
	*tamanio = deserializar_int(buffer, &desplazamiento);
    free(buffer);
	return direccion_fisica;
}

//Enviar paquete de direcciones fisicas

void send_paquete_df(t_list* lista_df, t_list* lista_tam, int cant_accesos, char* nombre_io, int cod_op, int socket){
	t_paquete* paquete = crear_paquete(cod_op);
	int direccion_fisica, tamanio;

	agregar_a_paquete(paquete, nombre_io, strlen(nombre_io) + 1);

	agregar_a_paquete(paquete, &cant_accesos, sizeof(int));

	for(int i = 0; i < cant_accesos; i++){
		direccion_fisica = list_get(lista_df, i);
		agregar_a_paquete(paquete, &direccion_fisica, sizeof(int));

		int tamanio = list_get(lista_tam, i);
		agregar_a_paquete(paquete, &tamanio, sizeof(int));
	}

	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

char* recv_paquete_df(int* cant_accesos, t_list* lista_df, t_list* lista_tam, int socket){
	int size = 0;
    void* buffer = recibir_buffer(&size, socket);

	int desplazamiento = 0;

	char* nombre_io = deserializar_string(buffer, &desplazamiento);

	*cant_accesos = deserializar_int(buffer, &desplazamiento);

	int direccion_fisica = 0, tamanio = 0;

	for(int i = 0; i < *cant_accesos; i++){
		direccion_fisica = deserializar_int(buffer, &desplazamiento);
		list_add(lista_df, direccion_fisica);
		tamanio = deserializar_int(buffer, &desplazamiento);
		list_add(lista_tam, tamanio);
	}

    free(buffer);

	return nombre_io;
}

//

void send_int_char(int entero, char* cadena, int cod_op, int socket){
	t_paquete* paquete = crear_paquete(cod_op);
	agregar_a_paquete(paquete, &entero, sizeof(int));
	agregar_a_paquete(paquete, &cadena, strlen(cadena) + 1);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

int recv_int_char(char* cadena, int socket){
	int size, desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket);
	int entero = deserializar_int(buffer, &desplazamiento);
	*cadena = deserializar_string(buffer, &desplazamiento);
    free(buffer);
	return entero;
}
