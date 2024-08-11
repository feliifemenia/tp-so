#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

int main(int argc, char* argv[]) {
    

    logger = log_create("memoria.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    logger_obligatorio = log_create("memoriaObligatorio.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    config = iniciar_config();

    copiar_config();
    
    inicializar_variables();
    
	log_info(logger, "Memoria lista para recibir");

    crear_conexiones();

    procesar_cpu();
    procesar_kernel();

    entradas_salidas_memoria(logger, "Memoria", memoria_fd);

	terminar_MEMORIA();
    return EXIT_SUCCESS;
}

void entradas_salidas_memoria(t_log* logger, char* name, int socket){

    log_info(logger,"%s listo para escuchar I/O", name); 
    
    while(esperar_cliente_memoria(name, socket)){

        log_info(logger,"%s listo para escuchar I/O", name); 
    }

}

int esperar_cliente_memoria(char* name, int server_Socket){
    int* Sock_fd = malloc(sizeof(int));
    *Sock_fd = esperar_Socket(logger, name, server_Socket);

    if(*Sock_fd != -1){ 
        hacer_hilo(Sock_fd);
        return 1;
    }
    return 0;
}

void hacer_hilo(int* socket){
    int interfaz;

    interfaz = recibir_operacion(*socket);
    free(recv_instruccion(*socket));

    switch (interfaz) { 
        case GENERICA : 
            //Esta conexion no hace falta manejarla, porque no necesita nada de memoria. La dejo por las dudas

            break;
        case STDIN :

            procesar_stdin(socket);

            break;
        case STDOUT :

            procesar_stdout(socket);

            break;
        case DIALFS:

            procesar_fs(socket);

            break;
        default: 
            log_error (logger, "Hubo un error de conexion IO");
            free(socket);
            terminar_MEMORIA();
            exit(EXIT_FAILURE);
            break;
    }
}

t_config* iniciar_config(){
    t_config* nuevo_config = config_create("memoria.config");

    if (nuevo_config == NULL) {
        log_error(logger, "Config no encontrado");
        config_destroy(nuevo_config);
        exit(1);
    }
    return nuevo_config;
}

void inicializar_variables(){

    cant_paginas = tamaño_memoria/tamaño_pagina;
    lista_tabla_instrucciones = list_create();
    bits = malloc(cant_paginas);
    bit_map = bitarray_create_with_mode(bits,cant_paginas, LSB_FIRST); //Empiezan en FALSE todos
                                                                       //False = Libre(clean), True = Ocupado(setted)
    for(int i=0; i < cant_paginas; i++){
        bitarray_clean_bit(bit_map,i);
    }

    espacio_memoria = malloc(tamaño_memoria);

    pthread_mutex_init(&mutex_lista_TIP,NULL);
    pthread_mutex_init(&mutex_void,NULL);
}

void copiar_config(){
    puerto_escucha = config_get_string_value(config,"PUERTO_ESCUCHA");
    tamaño_memoria = config_get_int_value(config,"TAM_MEMORIA");
    tamaño_pagina = config_get_int_value(config,"TAM_PAGINA");
    path_instrucciones = config_get_string_value(config,"PATH_INSTRUCCIONES");
    retardo_respuesta = config_get_int_value(config,"RETARDO_RESPUESTA");
}

void crear_conexiones(){
    memoria_fd = iniciar_socket(puerto_escucha,"memoria");
    cpu_fd = esperar_Socket(logger,"Memoria",memoria_fd);
    send_int(tamaño_pagina, cpu_fd, 1);
    kernel_fd = esperar_Socket(logger,"Memoria",memoria_fd);
}

int agregar_proceso(t_pcb* pcb, FILE * archivo){
    char caracter;

    struct t_tabla_instrucciones* paquete = malloc(sizeof(struct t_tabla_instrucciones));
    
    pcb -> contexto -> Registros_CPU -> pc = 0;
    
    paquete -> pid = pcb -> contexto -> pid;
    paquete -> instrucciones = list_create();
    paquete -> paginas = list_create();
    log_info(logger_obligatorio,"PID: <%d> - Tamaño: <%d>", pcb ->contexto ->pid, list_size(paquete -> paginas));

    pthread_mutex_lock(&mutex_lista_TIP);
    list_add(lista_tabla_instrucciones,paquete);
    pthread_mutex_unlock(&mutex_lista_TIP);

    int i = 0;

    char* instruccion = malloc(60);
    
    while ((caracter = fgetc(archivo)) != EOF){

        if(caracter =='\n'){
            instruccion[i] = '\0';
            pthread_mutex_lock(&mutex_lista_TIP);
            list_add(paquete -> instrucciones,instruccion);
            pthread_mutex_unlock(&mutex_lista_TIP);
            i=0;
            instruccion = malloc(60);
        }else{
            instruccion[i] = caracter;
            i++;
        }
    }
    instruccion[i] = '\0';
    pthread_mutex_lock(&mutex_lista_TIP);
    list_add(paquete -> instrucciones,instruccion);
    pthread_mutex_unlock(&mutex_lista_TIP);

    fclose(archivo);
    return 0;
}

struct t_tabla_instrucciones* buscar_pid(int pid_actual){
    t_list_iterator* iterator = list_iterator_create(lista_tabla_instrucciones);
    struct t_tabla_instrucciones* paquete;

    while(list_iterator_has_next(iterator)){
        
        paquete = list_iterator_next(iterator);

        if(paquete -> pid == pid_actual){
            list_iterator_destroy(iterator);
            return paquete;
        }
    }
    list_iterator_destroy(iterator);
    return NULL;
}

char* sacar_instruccion(int pc,int pid){

    struct t_tabla_instrucciones* paquete = buscar_pid(pid);
    if(paquete != NULL)
        return list_get(paquete -> instrucciones,pc);
    else return "EXIT";
}

int dar_pagina_libre(){

    int index;
    for(index = 0; index < cant_paginas; index++){
        if(bitarray_test_bit(bit_map,index) == false){
            bitarray_set_bit(bit_map,index);
            log_warning(logger,"bit_map cambiado a set en: %d",index);
            break;
        }
    }
    return index;
}

char* cambiar_instruccion(char* inst, int* operacion){
    char* datos = NULL;
   
    if(strcmp(inst,"EXIT")!=0){
        int espacios = cant_de_elem_en_string(inst,' ');
        char** valores = string_split(inst," ");

        datos = string_duplicate(valores[1]);

        for(int j = 2; j < espacios + 1; j++){// AGREGA LOS DATOS EN UN STRING DISTINTO
            string_append(&datos," ");
            string_append(&datos,valores[j]);
        }  

        for(int i= 0; i < 19; i++){//VERIFICA EL CODIGO DE OPERACION
            if(strcmp(valores[0],array_de_instrucciones[i])==0){
                *operacion = i;
                string_array_destroy(valores);
                return datos;
            }
        }
    }
    *operacion = EXIT;
    return "NULL";
}

int cant_de_elem_en_string(char* string, char elem){
    int i = 0;
    int cant = 0;
    char letra = string[i];
    while(i < strlen(string)){
        if(letra == elem){
            cant ++;
        }
        i++;
        letra = string[i];
    }
    return cant;
}

void liberar_memoria_por_pid(int pid_actual){
    int index,cantidad_paginas, cantidad_instrucciones;
    
    struct t_tabla_instrucciones *tabla_proceso = buscar_pid(pid_actual);

    cantidad_paginas = list_size(tabla_proceso -> paginas);
    cantidad_instrucciones = list_size(tabla_proceso -> instrucciones);
    quitar_pagina_al_proceso(tabla_proceso -> pid, cantidad_paginas * tamaño_pagina);

    log_info(logger_obligatorio,"PID: <%d> - Tamaño: <%d>",tabla_proceso -> pid,cantidad_paginas);
    
    t_list_iterator* iterador_proceso = list_iterator_create(lista_tabla_instrucciones);
    while(list_iterator_has_next(iterador_proceso)){
        struct t_tabla_instrucciones* tabla = list_iterator_next(iterador_proceso);
        
        if(tabla -> pid == pid_actual){
            index = list_iterator_index(iterador_proceso);
            struct t_tabla_instrucciones* paquete = list_remove(lista_tabla_instrucciones,index);
    
            for(int i = 0; i < cantidad_instrucciones; i++){
                char* instruccion = list_remove(paquete -> instrucciones, 0);
                free(instruccion);
            }

            list_destroy(paquete -> instrucciones);
            list_destroy(paquete -> paginas);
            free(paquete);
            
            list_iterator_destroy(iterador_proceso);
            
            return;
        }
    }
    
}

int cantidad_paginas_asignadas(int pid){
    struct t_tabla_instrucciones* paquete = buscar_pid(pid);
    
    return list_size(paquete -> paginas);
}

int verificar_out_of_memory(){

    int cantidad_paginas_libre = 0;
    for(int i = 0; i < cant_paginas;i++){
        if(bitarray_test_bit(bit_map,i) == false)
           cantidad_paginas_libre++;
    }
    return cantidad_paginas_libre * tamaño_pagina;
}

void hacer_resize(int pid, int size){
    int tamanio_proceso = tamaño_pagina * cantidad_paginas_asignadas(pid);

    if(tamanio_proceso - size < 0){
        int tamanio_restante = verificar_out_of_memory();
        if(tamanio_restante >= size - tamanio_proceso){
            agregar_pagina_al_proceso(pid, size - tamanio_proceso);
            log_info(logger_obligatorio,"PID: <%d> - Tamaño Actual: <%d> - Tamaño a Ampliar: <%d>",pid,tamanio_proceso,size);
            send_notificacion("OK",cpu_fd,OUT_OF_MEMORY);
        }
        else{
            send_notificacion("NOT OK",cpu_fd,OUT_OF_MEMORY);
        }
    }
    else{

        quitar_pagina_al_proceso(pid, tamanio_proceso - size);
        log_info(logger_obligatorio,"PID: <%d> - Tamaño Actual: <%d> - Tamaño a Reducir: <%d>",pid,tamanio_proceso,size);
        send_notificacion("OK",cpu_fd,OUT_OF_MEMORY);
    }
}

int agregar_pagina_al_proceso(int pid, int size){
    struct t_tabla_instrucciones *tabla = buscar_pid(pid);
    int cantidad_paginas_nuevas = size/tamaño_pagina;
    int resto = size % tamaño_pagina;
    if(resto > 0)
        cantidad_paginas_nuevas++;

    for (int i = 0; i < cantidad_paginas_nuevas ; i++){
        int* index_pagina = malloc(sizeof(int));
        *index_pagina = dar_pagina_libre();
        pthread_mutex_lock(&mutex_lista_TIP);
        list_add(tabla -> paginas, index_pagina);
        pthread_mutex_unlock(&mutex_lista_TIP);
    }
    
    return 0;
}

void quitar_pagina_al_proceso(int pid,int size){
    struct t_tabla_instrucciones *tabla = buscar_pid(pid);

    for (int i = 0; i < size/tamaño_pagina; i++){
        pthread_mutex_lock(&mutex_lista_TIP);
        int *pos = list_get(tabla -> paginas,list_size(tabla -> paginas) - 1);
        bitarray_clean_bit(bit_map,*pos);
        log_warning(logger,"Se libera el bit en <%d>",*pos);
        list_remove(tabla -> paginas,list_size(tabla -> paginas) - 1);
        free(pos);
        pthread_mutex_unlock(&mutex_lista_TIP);
    }
}

void procesar_kernel(){
    pthread_t hkernel;

    pthread_create(&hkernel,NULL,(void*) funcion_kernel, NULL);
    pthread_detach(hkernel);
}

void funcion_kernel(){
    while(1){

        int cod_op = recibir_operacion(kernel_fd);
        t_pcb *pcb;

        switch(cod_op){

            case CREAR_PCB:
                char* path = string_duplicate(path_instrucciones);
                char* path_archivo = recv_instruccion(kernel_fd);
                string_append(&path,path_archivo);
                FILE* archivo = fopen(path, "r");

                if(archivo == NULL){
                    send_notificacion("NOT OK",kernel_fd,CREAR_PCB);
                    log_error(logger,"Archivo no encontrado");
                }
                else{
                    send_notificacion("OK", kernel_fd, CREAR_PCB);
                    pcb = recv_pcb(kernel_fd);
                    agregar_proceso(pcb,archivo);
                    send_pcb(pcb,kernel_fd,CREAR_PCB);
                    liberar_pcb(pcb);   
                }
                free(path_archivo);
                free(path);
                break;

            case ELIM_PCB:
                int pid = recv_limpiar_memoria(kernel_fd);
                liberar_memoria_por_pid(pid);
                break;

            default:
                log_error(logger,"Problema en Memoria-Kernel");
                terminar_MEMORIA();
                exit(EXIT_FAILURE);
        }   
    }
}

void procesar_cpu(){
    pthread_t hcpu;

    pthread_create(&hcpu, NULL, (void*)clientecpu, NULL);
    pthread_detach(hcpu);
}

void clientecpu(){
    
    while(1){
        int cod_op = recibir_operacion(cpu_fd);
        int pc,pid, direccion_fisica, tamanio_a_leer;
        int dato_int;
        void* dato_void;
        switch(cod_op){
            case NUEVA_INSTRUCCION:
            
                recv_dos_int(&pid,&pc,cpu_fd);
                char* instruccion = sacar_instruccion(pc,pid); //Consigue la instruccion con el string completo
                int operacion; 
                char* datos = cambiar_instruccion(instruccion,&operacion);//divide la instruccion con los datos
                usleep(retardo_respuesta * 1000);
                send_instruccion(datos,cpu_fd,operacion); 
                
                if(strcmp(datos,"NULL") != 0)
                    free(datos);
                
                break;

            case RESIZE:

                int size = recv_resize(&pid,cpu_fd);
                hacer_resize(pid,size); //tiene en cuenta si reduce o aumenta espacio

                break;

            case DIRECCION_FISICA:

                int numero_pagina; 
                recv_dos_int(&pid,&numero_pagina,cpu_fd);
                int marco = buscar_marco(numero_pagina, pid);
                usleep(retardo_respuesta * 1000);
                send_int(marco, cpu_fd, 1);
                break;

            case MOV_OUT:
                int tamanio;
                direccion_fisica = recv_mov_out(&dato_int, &tamanio, cpu_fd);
                recibir_operacion(cpu_fd);
                pid = recv_int(cpu_fd);

                agregar_dato_a_pagina(direccion_fisica, &dato_int, tamanio, pid);
                break;

            case MOV_IN:
            
                recv_dos_int(&tamanio_a_leer, &direccion_fisica, cpu_fd);
                recibir_operacion(cpu_fd);
                pid = recv_int(cpu_fd);

                dato_void = leer_dato_de_pagina(tamanio_a_leer, direccion_fisica, pid);
                memcpy(&dato_int,dato_void,tamanio_a_leer);
                usleep(retardo_respuesta * 1000);
                send_void(dato_void,tamanio_a_leer,cpu_fd,0);
                free(dato_void);
                break;

            case ESCRIBIR:

                direccion_fisica = recv_df_dato(&dato_void, cpu_fd);
                recibir_operacion(cpu_fd);
                tamanio = recv_int(cpu_fd);
                recibir_operacion(cpu_fd);
                pid = recv_int(cpu_fd);

                agregar_dato_a_pagina(direccion_fisica, dato_void, tamanio, pid);
                usleep(retardo_respuesta * 1000);
                send_notificacion("listo", cpu_fd, 0);
                free(dato_void);
                break;
                
            case LECTURA:
                recv_dos_int(&tamanio_a_leer, &direccion_fisica, cpu_fd);
                char* dato_char = malloc(tamanio_a_leer);
                recibir_operacion(cpu_fd);
                pid = recv_int(cpu_fd);

                dato_void = leer_dato_de_pagina(tamanio_a_leer, direccion_fisica,pid);
                usleep(retardo_respuesta * 1000);
                memcpy(dato_char,dato_void,tamanio_a_leer);
                send_notificacion(dato_char, cpu_fd, 0);
                free(dato_char);
                free(dato_void);
                break;

            default: 
                log_error(logger,"Problema en Memoria-CPU");
                terminar_MEMORIA();
                exit(EXIT_FAILURE);
        }
    }
}

void procesar_stdin(int* socket){
    pthread_t hilo_stdin;
    pthread_create(&hilo_stdin,NULL,(void*) cliente_stdin, socket);
    pthread_detach(hilo_stdin);
}

void cliente_stdin(void* args){
    int pid,cod_op = 0;
    int direccion_fisica;
    int* stdin_fd = (int*) args;
    while(1){
        
        cod_op = recibir_operacion(*stdin_fd);
        switch(cod_op){
            case IO_STDIN_READ:
                char* datos = NULL;
                direccion_fisica = recv_df_dato(&datos,*stdin_fd);
                recibir_operacion(*stdin_fd);
                pid = recv_int(*stdin_fd);
                agregar_dato_a_pagina(direccion_fisica,datos, strlen(datos),pid);
                usleep(retardo_respuesta * 1000);
                send_notificacion("LISTO",*stdin_fd,0);
                free(datos);
                break;

            default:
                log_error(logger,"Stdin desconectado");
                pthread_exit(NULL);//El null es un valor que se puede pasar para que devuelva el hilo. Como no necesito nada pongo null. 
                free(stdin_fd);
                break;

        }
    }
    
}

void procesar_stdout(int* socket){
    pthread_t hilo_stdout;
    pthread_create(&hilo_stdout,NULL,(void*)cliente_stdout, socket);
    pthread_detach(hilo_stdout);
}

void cliente_stdout(void* args){
    int pid,cod_op = 0;
    int* stdout_fd = (int*) args;
    while(1){

        cod_op = recibir_operacion(*stdout_fd);
        switch(cod_op){
            case IO_STDOUT_WRITE:
                int direccion_fisica, tamanio;
                recv_dos_int(&tamanio, &direccion_fisica, *stdout_fd);
                recibir_operacion(*stdout_fd);
                pid = recv_int(*stdout_fd);
                char* dato = NULL;
                dato = leer_dato_de_pagina(tamanio,direccion_fisica,pid);
                dato[tamanio] = '\0';
                usleep(retardo_respuesta * 1000);
                send_notificacion(dato, *stdout_fd, 0);
                free(dato);
                break;

            default:

                free(stdout_fd);
                log_error(logger,"Stdout desconectado");
                pthread_exit(NULL);//El null es un valor que se puede pasar para que devuelva el hilo. Como no necesito nada pongo null.
                break;

        }
    }
    
}

void procesar_fs(int* socket){
    pthread_t hilo_fs;
    pthread_create(&hilo_fs,NULL,(void*)cliente_fs, socket);
    pthread_detach(hilo_fs);
}

void cliente_fs(void* args){
    int* filesystem_fd = (int*) args;
    int pid,cod_op = 0;
     while(1){

        cod_op = recibir_operacion(*filesystem_fd);
        switch(cod_op){
            case IO_FS_WRITE:
                int direccion_fisica, tamanio;
                recv_dos_int(&tamanio, &direccion_fisica, *filesystem_fd);
                recibir_operacion(*filesystem_fd);
                pid = recv_int(*filesystem_fd);

                char* dato = leer_dato_de_pagina(tamanio,direccion_fisica,pid);
                usleep(retardo_respuesta * 1000);
                send_notificacion(dato, *filesystem_fd, 0);
                free(dato);
                break;
        
            case IO_FS_READ:
                char* datos = string_new();
                direccion_fisica = recv_df_dato(&datos,*filesystem_fd);
                recibir_operacion(*filesystem_fd);
                pid = recv_int(*filesystem_fd);
                agregar_dato_a_pagina(direccion_fisica,datos, strlen(datos),pid);
                usleep(retardo_respuesta * 1000);
                send_notificacion("LISTO",*filesystem_fd,0);

                break;

            case -1:
                free(filesystem_fd);
                log_error(logger,"FileSystem desconectado");
                pthread_exit(NULL);//El null es un valor que se puede pasar para que devuelva el hilo. Como no necesito nada pongo null.

                break;
            default:
                free(filesystem_fd);
                log_error(logger,"Error en FILESYSTEM");
                terminar_MEMORIA();
                pthread_exit(NULL);
        }
    }
}

void agregar_dato_a_pagina(int direccion_fisica, void* dato, int tamanio, int pid){

    log_info(logger,"PID: <%d> - Accion: <ESCRITURA> - Direccion fisica: <%d> - Tamaño <%d>",pid,direccion_fisica,tamanio);

    pthread_mutex_lock(&mutex_void);
    memcpy(espacio_memoria + direccion_fisica, dato, tamanio);
    pthread_mutex_unlock(&mutex_void);
}

void* leer_dato_de_pagina(int tamanio, int direccion_fisica, int pid){
    void* dato = malloc(tamanio);

    log_info(logger,"PID: <%d> - Accion: <LECTURA> - Direccion fisica: <%d> - Tamaño <%d>",pid,direccion_fisica,tamanio);

    pthread_mutex_lock(&mutex_void);
    memcpy(dato, (char*)espacio_memoria + direccion_fisica, tamanio);
    pthread_mutex_unlock(&mutex_void);
    return dato;
}


int buscar_marco(int numero_pagina, int pid){
    struct t_tabla_instrucciones* tabla_proceso = buscar_pid(pid);

    int *marco = list_get(tabla_proceso -> paginas, numero_pagina);
    
    log_info(logger_obligatorio,"PID: <%d> - Pagina: <%d> - Marco: <%d>",pid,numero_pagina,*marco);
    return *marco * tamaño_pagina;
}

void liberar_procesos(){
    
    t_list_iterator *iterador = list_iterator_create(lista_tabla_instrucciones);
    while(list_iterator_has_next(iterador)){
        struct t_tabla_instrucciones* tabla_grande = list_iterator_next(iterador);

        int* marco = NULL;
        t_list_iterator* iterador_paginas = list_iterator_create(tabla_grande -> paginas);
        while(list_iterator_has_next(iterador_paginas)){
            marco = list_iterator_next(iterador_paginas);
            free(marco);
        }

        list_iterator_destroy(iterador_paginas);
        list_destroy(tabla_grande ->paginas);

        char* instruccion = NULL;
        t_list_iterator* iterador_instrucciones = list_iterator_create(tabla_grande -> instrucciones);
        while(list_iterator_has_next(iterador_instrucciones)){
            instruccion = list_iterator_next(iterador_instrucciones);
            free(instruccion);
        }

        list_iterator_destroy(iterador_instrucciones);
        list_destroy(tabla_grande ->instrucciones);
        free(tabla_grande);
    }
    list_iterator_destroy(iterador);
}

void terminar_MEMORIA(){
    liberar_conexion(kernel_fd);
    liberar_conexion(cpu_fd);

    liberar_procesos();

	log_destroy(logger);
    log_destroy(logger_obligatorio);
	config_destroy(config);
    bitarray_destroy(bit_map);
    free(espacio_memoria);
    free(bits);
    list_destroy(lista_tabla_instrucciones);
    pthread_mutex_destroy(&mutex_lista_TIP);
    pthread_mutex_destroy(&mutex_void);
}