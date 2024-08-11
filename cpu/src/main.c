#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

int main(int argc, char* argv[]) {

    logger = log_create("cpu.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    logger_obligatorio = log_create("cpu_obligatorio.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    config = iniciar_config();

    copiar_config();
    
    crear_conexiones();

    //PARA SACAR OPERACION
    recibir_operacion(memoria_fd);
    tamaño_pagina = recv_int(memoria_fd);

    iniciar_tlb();
    if(cantidad_entradas_TLB == 0)
        log_warning(logger_obligatorio, "La TLB no se encuentra activa");

    procesar_proceso();

    readline ("> ");
    terminar_CPU(memoria_fd);
    return 0;
}

t_config* iniciar_config(){
    t_config* nuevo_config = config_create("cpu.config");

    if (nuevo_config == NULL) {
        log_error(logger, "Config no encontrado");
        config_destroy(nuevo_config);
        exit(1);
    }
    return nuevo_config;
}

void copiar_config(){
    ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    log_info(logger, "CPU lista para escuchar Kernel");    
    puerto_escucha_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    puerto_escucha_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");
    cantidad_entradas_TLB = config_get_int_value(config, "CANTIDAD_ENTRADAS_TLB");
    algoritmo_TLB = config_get_string_value(config, "ALGORITMO_TLB");
}

void crear_conexiones(){
    cpu_dispatch_fd = iniciar_socket(puerto_escucha_dispatch, "CPU Dispatch");
    cpu_interrupt_fd = iniciar_socket(puerto_escucha_interrupt, "CPU Interrupt");
    memoria_fd = crear_conexion(ip_memoria, puerto_memoria);
    kernel_dispatch_fd = esperar_Socket(logger, "CPU Dispatch", cpu_dispatch_fd);
    kernel_interrupt_fd = esperar_Socket(logger, "CPU Interrupt", cpu_interrupt_fd);
}

void procesar_proceso(){
    int cod_op = -1;
    t_pcb* pcb = NULL;
    bool no_bloqueado = true;
    t_temporal* quantum = NULL;
    char* instruccion = NULL;
    
    while (1){

        if(pcb == NULL){
            no_bloqueado = true;
            pcb = recv_pcb(kernel_dispatch_fd);
            if(pcb == NULL)
                exit(EXIT_FAILURE);
                
            log_info(logger,"Nuevo PCB recibido: %d", pcb -> contexto -> pid);
            quantum = temporal_create();
        }

        instruccion = fase_fetch (pcb, &cod_op);
        
        pcb -> contexto -> Registros_CPU -> pc += 1; 
        
        fase_decode_execute(&pcb,instruccion, cod_op, &no_bloqueado, quantum);

        if(!check_interrupt(kernel_interrupt_fd) && no_bloqueado){
            pcb -> quantum = 0;
            temporal_destroy(quantum);
            send_pcb(pcb,kernel_dispatch_fd,FIN_QUANTUM);
            liberar_pcb(pcb);
            pcb = NULL;
        }
        free(instruccion);
        instruccion = NULL;
    }
}

bool check_interrupt(int socket){
    int result = recv_interrupt(socket);//devuelve -1 o 0 si no es interrumpido
    return result <= 0;
}

char* fase_fetch(t_pcb* pcb, int* cod_op){

    log_info(logger,"PID: <%d> - FETCH - Program Counter: <%d>",pcb -> contexto -> pid, pcb -> contexto -> Registros_CPU -> pc);
    send_dos_int(pcb->contexto->Registros_CPU->pc, pcb -> contexto -> pid, memoria_fd, NUEVA_INSTRUCCION);
    *cod_op = recibir_operacion(memoria_fd);
    
    if(*cod_op == -1){
        log_error(logger,"Desconexion de memoria");
        exit(EXIT_FAILURE);
    }

    char* instruccion = recv_instruccion(memoria_fd);
    return instruccion;
}

int comprobar_valor(t_pcb* pcb, char* a_comprobar){
    if(a_comprobar[0] >= '0' && a_comprobar[0] <= '9'){//verifica si el primer caracter de la segunda parte de la instruccion es un numero...
        return atoi(a_comprobar);                    //Ejemplo: "AX 10", agarra el 1 y se fija si es un numero
    }else if(strcmp(a_comprobar,"AX") == 0){         //sino tiene que ser un registro...
        return pcb -> contexto -> Registros_CPU -> ax;
    }else if(strcmp(a_comprobar,"BX") == 0){
        return pcb -> contexto -> Registros_CPU -> bx;
    }else if(strcmp(a_comprobar,"CX") == 0){
        return pcb -> contexto -> Registros_CPU -> cx;
    }else if(strcmp(a_comprobar,"DX") == 0){
        return pcb -> contexto -> Registros_CPU -> dx;
    }else if(strcmp(a_comprobar,"EAX") == 0){
        return pcb -> contexto -> Registros_CPU -> eax;
    }else if(strcmp(a_comprobar,"EBX") == 0){
        return pcb -> contexto -> Registros_CPU -> ebx;
    }else if(strcmp(a_comprobar,"ECX") == 0){
        return pcb -> contexto -> Registros_CPU -> ecx;
    }else if(strcmp(a_comprobar,"EDX") == 0){
        return pcb -> contexto -> Registros_CPU -> edx;
    }else if(strcmp(a_comprobar,"SI") == 0){
        return pcb -> contexto -> Registros_CPU -> si;
    }else if(strcmp(a_comprobar,"DI") == 0){
        return pcb -> contexto -> Registros_CPU -> di;
    }else return -1; //valor invalido
}

void fase_decode_execute(t_pcb** pcb, char* instrucciones, int cod_op, bool* no_bloqueado, t_temporal* quantum){

    log_info(logger,"PID: <%d> - Ejecutando: <%s> - <%s>",(*pcb) -> contexto -> pid,array_de_instrucciones[cod_op],instrucciones);

    char** valores = string_split(instrucciones," ");

    int valor = -1;
    
        switch (cod_op){
            case SET:
                *no_bloqueado = true;

                if((valor = comprobar_valor(*pcb,valores[1])) == -1){
                    log_error(logger,"Valor invalido ingresado, terminando proceso");
                    (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                    goto END_PROCESS;
                }

                operar_set(*pcb,valores[0],valor);

                break;
            case SUM:
                *no_bloqueado = true;

                if((valor = comprobar_valor(*pcb,valores[1])) == -1){
                    log_error(logger,"Valor invalido ingresado, terminando proceso");
                    (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                    goto END_PROCESS;
                }

                operar_sum(*pcb,valores[0],valor);
                
                break;
            case SUB:
                *no_bloqueado = true;
                if((valor = comprobar_valor(*pcb,valores[1])) == -1){
                    log_error(logger,"Valor invalido ingresado, terminando proceso");
                    (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                    goto END_PROCESS;
                }
                operar_sub(*pcb,valores[0],valor);

                break;
            case MOV_IN:
                *no_bloqueado = true;
                operar_mov_in(*pcb, valores[0], valores[1]);

                break;
            case MOV_OUT:
                *no_bloqueado = true;
                operar_mov_out(*pcb, valores[0], valores[1]);

                break;
            case RESIZE:
                *no_bloqueado = true;

                if((valor = comprobar_valor(*pcb,valores[0])) == -1){
                    log_error(logger,"Valor invalido ingresado, terminando proceso");
                    (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                    goto END_PROCESS;
                }

                send_resize((*pcb)-> contexto -> pid,valor,memoria_fd);
                char* valid = recv_notificacion(memoria_fd);
                if(strcmp(valid, "OK") != 0){

                    (*pcb) -> contexto -> motivo_exit = OUT_OF_MEMORY;
                    modificar_quantum(pcb,quantum);
                    send_pcb(*pcb,kernel_dispatch_fd,EXIT_PCB);
                    liberar_pcb(*pcb);
                    *pcb = NULL;
                    *no_bloqueado = false;

                }
                free(valid);

                break;
            case JNZ:
                *no_bloqueado = true;

                if((valor = comprobar_valor(*pcb,valores[0])) == -1){
                    log_error(logger,"Valor invalido ingresado, terminando proceso");
                    (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                    goto END_PROCESS;
                }

                operar_jnz(*pcb,valores[0],valor);
                break;
            case COPY_STRING:
                *no_bloqueado = true;

                if((valor = comprobar_valor(*pcb,valores[0])) == -1){
                    log_error(logger,"Valor invalido ingresado, terminando proceso");
                    (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                    goto END_PROCESS;
                }

                operar_copy_string(*pcb, valor);
                break;
            case IO_GEN_SLEEP:
                *no_bloqueado = false;

                if((valor = comprobar_valor(*pcb,valores[1])) == -1){
                    log_error(logger,"Valor invalido ingresado, terminando proceso");
                    (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                    goto END_PROCESS;
                }

                modificar_quantum(pcb, quantum);
                send_pcb(*pcb,kernel_dispatch_fd,MANEJAR_IO_GEN_SLEEP);
                send_ins_io_gen_sleep(valores[0],valor,kernel_dispatch_fd);
                liberar_pcb(*pcb);
                *pcb = NULL;

                break;
            case IO_STDIN_READ:
                *no_bloqueado = false;

                modificar_quantum(pcb, quantum);
                operar_io_std(*pcb, valores[0], valores[1], valores[2], kernel_dispatch_fd, MANEJAR_IO_STDIN_READ);

                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
            case IO_STDOUT_WRITE:
                *no_bloqueado = false;
                modificar_quantum(pcb, quantum);
                operar_io_std(*pcb, valores[0], valores[1], valores[2], kernel_dispatch_fd, MANEJAR_IO_STDOUT_WRITE);
                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
            case IO_FS_CREATE:
                *no_bloqueado = false;

                modificar_quantum(pcb, quantum);

                send_pcb(*pcb, kernel_dispatch_fd, MANEJAR_FS);
                send_instruccion(valores[0], kernel_dispatch_fd, IO_FS_CREATE);
                send_notificacion(valores[1], kernel_dispatch_fd, 0);

                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
            case IO_FS_DELETE:
                *no_bloqueado = false;

                modificar_quantum(pcb, quantum);

                send_pcb(*pcb, kernel_dispatch_fd, MANEJAR_FS);
                send_instruccion(valores[0], kernel_dispatch_fd, IO_FS_DELETE);
                send_notificacion(valores[1], kernel_dispatch_fd, 0);

                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
            case IO_FS_TRUNCATE:
                *no_bloqueado = false;
                valor = comprobar_valor(*pcb,valores[2]);

                if(valor == -1){
                    log_error(logger,"Valor invalido ingresado, terminando proceso");
                    (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                    goto END_PROCESS;
                }

                modificar_quantum(pcb, quantum);

                send_pcb(*pcb, kernel_dispatch_fd, MANEJAR_FS);
                send_instruccion(valores[0], kernel_dispatch_fd, IO_FS_TRUNCATE);
                send_notificacion(valores[1], kernel_dispatch_fd, 0);
                send_int(valor,kernel_dispatch_fd,0);

                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
            case IO_FS_WRITE:
                *no_bloqueado = false;

                modificar_quantum(pcb, quantum);

                operar_io_fs(*pcb, valores[0], valores[1], valores[2], valores[3], valores[4], MANEJAR_FS, IO_FS_WRITE);

                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
            case IO_FS_READ:
                *no_bloqueado = false;

                modificar_quantum(pcb, quantum);

                operar_io_fs(*pcb, valores[0], valores[1], valores[2], valores[3], valores[4], MANEJAR_FS, IO_FS_READ);

                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
            case WAIT:
                *no_bloqueado = true;
                
                if((*pcb) -> quantum - temporal_gettime(quantum) >= 0)
                    (*pcb) -> quantum = (*pcb) -> quantum - temporal_gettime(quantum);
                 else 
                    (*pcb) -> quantum = 0;

                operar_semaforo(*pcb, valores[0], MANEJAR_WAIT);


                char* consulta = recv_notificacion(kernel_dispatch_fd);
                if(strcmp(consulta,"BLOQUEANTE") == 0){
                    *no_bloqueado = false;
                    temporal_destroy(quantum);
                    liberar_pcb(*pcb);
                    *pcb=NULL;
                }

                free(consulta);
                break;
            case SIGNAL:
                *no_bloqueado = true;
                
                operar_semaforo(*pcb, valores[0], MANEJAR_SIGNAL);

                break;
            case EXIT:
            
            (*pcb) -> contexto -> motivo_exit = SUCCESS;
                END_PROCESS:
                *no_bloqueado = false;

                log_info(logger,"EXIT");
                
                modificar_quantum(pcb, quantum);

                send_pcb(*pcb,kernel_dispatch_fd,EXIT_PCB);
                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
            default:
                *no_bloqueado = false;

                log_error (logger, "Operación desconocida, terminando programa.");

                (*pcb) -> contexto -> motivo_exit = INVALID_RESOURCE;
                modificar_quantum(pcb, quantum);

                send_pcb(*pcb,kernel_dispatch_fd,EXIT_PCB);
                liberar_pcb(*pcb);
                *pcb = NULL;
                break;
        }
        string_array_destroy(valores);
}

void operar_set(t_pcb* pcb,char* registro, int valor){
    cambiar_valor_registro(pcb,registro,valor);
}

void operar_sum(t_pcb* pcb, char* registro, int valor){
    if(strcmp(registro,"AX") == 0){
        pcb -> contexto -> Registros_CPU -> ax = pcb -> contexto -> Registros_CPU -> ax + valor;
    }else if(strcmp(registro,"BX") == 0){
        pcb -> contexto -> Registros_CPU -> bx = pcb -> contexto -> Registros_CPU -> bx + valor;
    }else if(strcmp(registro,"CX") == 0){
        pcb -> contexto -> Registros_CPU -> cx = pcb -> contexto -> Registros_CPU -> cx + valor;
    }else if(strcmp(registro,"DX") == 0){
        pcb -> contexto -> Registros_CPU -> dx = pcb -> contexto -> Registros_CPU -> dx + valor;
    }else if(strcmp(registro,"EAX") == 0){
        pcb -> contexto -> Registros_CPU -> eax = pcb -> contexto -> Registros_CPU -> eax + valor;
    }else if(strcmp(registro,"EBX") == 0){
        pcb -> contexto -> Registros_CPU -> ebx = pcb -> contexto -> Registros_CPU -> ebx + valor;
    }else if(strcmp(registro,"ECX") == 0){
        pcb -> contexto -> Registros_CPU -> ecx = pcb -> contexto -> Registros_CPU -> ecx + valor;
    }else if(strcmp(registro,"EDX") == 0){
        pcb -> contexto -> Registros_CPU -> edx = pcb -> contexto -> Registros_CPU -> edx + valor;
    }else if(strcmp(registro,"SI") == 0){
        pcb -> contexto -> Registros_CPU -> si = pcb -> contexto -> Registros_CPU -> si + valor;
    }else if(strcmp(registro,"DI") == 0){
        pcb -> contexto -> Registros_CPU -> di = pcb -> contexto -> Registros_CPU -> di + valor;
    }
}

void operar_sub(t_pcb* pcb, char* registro, int valor){
    if(strcmp(registro,"AX") == 0){
        pcb -> contexto -> Registros_CPU -> ax = pcb -> contexto -> Registros_CPU -> ax - valor;
    }else if(strcmp(registro,"BX") == 0){
        pcb -> contexto -> Registros_CPU -> bx = pcb -> contexto -> Registros_CPU -> bx - valor;
    }else if(strcmp(registro,"CX") == 0){
        pcb -> contexto -> Registros_CPU -> cx = pcb -> contexto -> Registros_CPU -> cx - valor;
    }else if(strcmp(registro,"DX") == 0){
        pcb -> contexto -> Registros_CPU -> dx = pcb -> contexto -> Registros_CPU -> dx - valor;
    }else if(strcmp(registro,"EAX") == 0){
        pcb -> contexto -> Registros_CPU -> eax = pcb -> contexto -> Registros_CPU -> eax - valor;
    }else if(strcmp(registro,"EBX") == 0){
        pcb -> contexto -> Registros_CPU -> ebx = pcb -> contexto -> Registros_CPU -> ebx - valor;
    }else if(strcmp(registro,"ECX") == 0){
        pcb -> contexto -> Registros_CPU -> ecx = pcb -> contexto -> Registros_CPU -> ecx - valor;
    }else if(strcmp(registro,"EDX") == 0){
        pcb -> contexto -> Registros_CPU -> edx = pcb -> contexto -> Registros_CPU -> edx - valor;
    }else if(strcmp(registro,"SI") == 0){
        pcb -> contexto -> Registros_CPU -> si = pcb -> contexto -> Registros_CPU -> si - valor;
    }else if(strcmp(registro,"DI") == 0){
        pcb -> contexto -> Registros_CPU -> di = pcb -> contexto -> Registros_CPU -> di - valor;
    }
}

int obtener_valor_de_registro(t_pcb* pcb, char* registro){
    if(strcmp(registro,"AX") == 0){
        return pcb -> contexto -> Registros_CPU -> ax;
    }else if(strcmp(registro,"BX") == 0){
        return pcb -> contexto -> Registros_CPU -> bx;
    }else if(strcmp(registro,"CX") == 0){
        return pcb -> contexto -> Registros_CPU -> cx;
    }else if(strcmp(registro,"DX") == 0){
        return pcb -> contexto -> Registros_CPU -> dx;
    }else if(strcmp(registro,"EAX") == 0){
        return pcb -> contexto -> Registros_CPU -> eax;
    }else if(strcmp(registro,"EBX") == 0){
        return pcb -> contexto -> Registros_CPU -> ebx;
    }else if(strcmp(registro,"ECX") == 0){
        return pcb -> contexto -> Registros_CPU -> ecx;
    }else if(strcmp(registro,"EDX") == 0){
        return pcb -> contexto -> Registros_CPU -> edx;
    }else if(strcmp(registro,"SI") == 0){
        return pcb -> contexto -> Registros_CPU -> si;
    }else if(strcmp(registro,"DI") == 0){
        return pcb -> contexto -> Registros_CPU -> di;
    }
}

void cambiar_valor_registro(t_pcb* pcb, char* registro, int valor){
    if(strcmp(registro,"AX") == 0){
        pcb -> contexto -> Registros_CPU -> ax = valor;
    }else if(strcmp(registro,"BX") == 0){
        pcb -> contexto -> Registros_CPU -> bx = valor;
    }else if(strcmp(registro,"CX") == 0){
        pcb -> contexto -> Registros_CPU -> cx = valor;
    }else if(strcmp(registro,"DX") == 0){
        pcb -> contexto -> Registros_CPU -> dx = valor;
    }else if(strcmp(registro,"EAX") == 0){
        pcb -> contexto -> Registros_CPU -> eax = valor;
    }else if(strcmp(registro,"EBX") == 0){
        pcb -> contexto -> Registros_CPU -> ebx = valor;
    }else if(strcmp(registro,"ECX") == 0){
        pcb -> contexto -> Registros_CPU -> ecx = valor;
    }else if(strcmp(registro,"EDX") == 0){
        pcb -> contexto -> Registros_CPU -> edx = valor;
    }else if(strcmp(registro,"SI") == 0){
        pcb -> contexto -> Registros_CPU -> si = valor;
    }else if(strcmp(registro,"DI") == 0){
        pcb -> contexto -> Registros_CPU -> di = valor;
    }else if(strcmp(registro,"PC") == 0)
        pcb -> contexto -> Registros_CPU -> pc = valor;
}

int obtener_cantidad_accesos(int direccion_logica_inicial, int direccion_logica_final){

    int numero_pagina_inicial = floor(direccion_logica_inicial / tamaño_pagina);
    int numero_pagina_final = floor(direccion_logica_final / tamaño_pagina);

    return (numero_pagina_final - numero_pagina_inicial) + 1;
}

void operar_jnz(t_pcb* pcb, char* registro, int valor){

    if(obtener_valor_de_registro(pcb, registro) != 0 )
        cambiar_valor_registro(pcb,"PC",valor);
}

void operar_mov_in(t_pcb* pcb, char* registro_datos, char* registro_direccion){

    int direccion_logica_inicial = obtener_valor_de_registro(pcb,registro_direccion);
    
    int tamanio = obtener_tamanio_registro(registro_datos);

    int cant_accesos;

    if(tamanio == 1)
        cant_accesos = 1;
    else cant_accesos = obtener_cantidad_accesos(direccion_logica_inicial,direccion_logica_inicial + tamanio);

    int direccion_fisica, desplazamiento, numero_pagina = 0;
    u_int32_t* dato_32;
    u_int8_t* dato_8;

    void* void_;

    if(cant_accesos > 1){
        int cant_espacio_leido = 0;
        
        bool primer_acceso = true;
        for(int i = 0; i < cant_accesos; i++){
            numero_pagina = floor((direccion_logica_inicial + cant_espacio_leido) / tamaño_pagina);

            desplazamiento = direccion_logica_inicial + cant_espacio_leido - numero_pagina * tamaño_pagina;
            int tamanio_sacado = tamaño_pagina - desplazamiento;

            if(cantidad_entradas_TLB != 0)
                direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
            else direccion_fisica = -1;
            

            if(direccion_fisica < 0){
                direccion_fisica = MMU(pcb,numero_pagina,desplazamiento);
                if(cantidad_entradas_TLB != 0)
                    agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
            }
            
            if(primer_acceso){
                send_dos_int(direccion_fisica, tamanio_sacado, memoria_fd, MOV_IN);
                send_int(pcb ->contexto ->pid,memoria_fd,0);
                void_ = recv_void(memoria_fd); 
                cant_espacio_leido = tamanio_sacado;
                primer_acceso = false;

            } 
            else{
                void* void_aux;
                send_dos_int(direccion_fisica, tamanio - cant_espacio_leido, memoria_fd, MOV_IN);
                send_int(pcb ->contexto ->pid,memoria_fd,0);
                void_aux = recv_void(memoria_fd);
                memcpy(void_ + cant_espacio_leido, void_aux, tamanio - cant_espacio_leido);
                cant_espacio_leido = tamanio - cant_espacio_leido;
            }
        }
        dato_32 = void_;
    } 
    else{
        numero_pagina = floor(direccion_logica_inicial / tamaño_pagina);
        desplazamiento = direccion_logica_inicial - numero_pagina * tamaño_pagina;

        if(cantidad_entradas_TLB != 0)
                direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
        else direccion_fisica = -1;

        if(direccion_fisica < 0){
            direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
            if(cantidad_entradas_TLB != 0)
                agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
        }

        send_dos_int(direccion_fisica, tamanio, memoria_fd, MOV_IN);
        send_int(pcb ->contexto ->pid,memoria_fd,0);

        if(tamanio == 1)
            dato_8 = recv_void(memoria_fd);
        else dato_32 = recv_void(memoria_fd);
    }
    if (tamanio == 1){
        log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%d>",pcb -> contexto -> pid, "Lectura", direccion_fisica, *dato_8);
        cambiar_valor_registro(pcb, registro_datos, *dato_8);
    }
    else{
        log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%d>",pcb -> contexto -> pid, "Lectura", direccion_fisica, *dato_32);
        cambiar_valor_registro(pcb, registro_datos, *dato_32);
    }
}

void operar_mov_out(t_pcb* pcb, char* registro_direccion, char* registro_datos){
    u_int8_t* dato_8 = malloc(sizeof(u_int8_t));
    u_int32_t* dato_32 = malloc(sizeof(u_int32_t));

    int direccion_logica = obtener_valor_de_registro(pcb,registro_direccion);

    int tamanio_dato = obtener_tamanio_registro(registro_datos);

    if(tamanio_dato == 1)
        *dato_8 = obtener_valor_de_registro(pcb, registro_datos);
    else *dato_32 = obtener_valor_de_registro(pcb, registro_datos);

    int direccion_logica_final = direccion_logica + tamanio_dato;

    int cant_accesos;

    if(tamanio_dato == 1)
        cant_accesos = 1;
    else cant_accesos = obtener_cantidad_accesos(direccion_logica, direccion_logica_final);

    int direccion_fisica,desplazamiento;

    int numero_pagina = 0;

    if(cant_accesos > 1){
        int tamanio_restante_a_sacar, tamanio_sacado = 0;
        bool primer_acceso = true;
        for(int i = 0; i < cant_accesos; i++){

            numero_pagina = floor((direccion_logica + tamanio_sacado) / tamaño_pagina);
            desplazamiento = direccion_logica + tamanio_sacado - numero_pagina * tamaño_pagina;

            if(cantidad_entradas_TLB != 0)
                direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
            else direccion_fisica = -1;

            if(direccion_fisica < 0){
                direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
                if(cantidad_entradas_TLB != 0)
                    agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
            }

            int tamanio_restante = tamaño_pagina - desplazamiento; //Los bytes disponibles para escribir de la pagina

            if(tamanio_dato == 1)
                log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%d>",pcb -> contexto -> pid, "Escritura", direccion_fisica, *dato_8);
            else log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%d>",pcb -> contexto -> pid, "Escritura", direccion_fisica, *dato_32);

            if(primer_acceso ){
                tamanio_restante_a_sacar = tamanio_dato - tamanio_restante;
                send_mov_out(direccion_fisica, *((char*)dato_32), tamanio_restante, memoria_fd, MOV_OUT);
                send_int(pcb -> contexto -> pid,memoria_fd, 0);
                tamanio_sacado = tamanio_restante;
                primer_acceso = false;

            }
            else{
                if(tamanio_restante > tamanio_restante_a_sacar){ //hay lugar de sobra en la pagina
                    send_mov_out(direccion_fisica, *((char*)dato_32 + tamanio_sacado), tamanio_restante_a_sacar, memoria_fd, MOV_OUT);
                    send_int(pcb -> contexto -> pid,memoria_fd, 0);
                    tamanio_sacado += tamanio_restante_a_sacar;
                }
                else{
                    tamanio_restante = tamanio_dato - tamanio_restante;
                    send_mov_out(direccion_fisica, *((char*)dato_32 + tamanio_sacado), tamanio_restante_a_sacar, memoria_fd, MOV_OUT);
                    send_int(pcb -> contexto -> pid,memoria_fd, 0);
                    tamanio_sacado += tamanio_restante_a_sacar;
                }
            }
        }
    } 
    else{
        numero_pagina = floor(direccion_logica / tamaño_pagina);

        desplazamiento = direccion_logica - numero_pagina * tamaño_pagina;

        if(cantidad_entradas_TLB != 0)
            direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
        else direccion_fisica = -1;

        if(direccion_fisica < 0){
            direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
            if(cantidad_entradas_TLB != 0)
                agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
        }
        if(tamanio_dato == 1){
            log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%d>",pcb -> contexto -> pid, "Escritura", direccion_fisica, *dato_8);
            send_mov_out(direccion_fisica, *dato_8, tamanio_dato, memoria_fd, MOV_OUT);
            send_int(pcb -> contexto -> pid,memoria_fd, 0);
        }
        else{
            log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%d>",pcb -> contexto -> pid, "Escritura", direccion_fisica, *dato_32);
            send_mov_out(direccion_fisica, *dato_32, tamanio_dato, memoria_fd, MOV_OUT);
            send_int(pcb -> contexto -> pid,memoria_fd, 0);
        }
    }
    free(dato_8);
    free(dato_32);
}

void operar_copy_string(t_pcb* pcb, int tamanio){

    int direccion_logica_inicial = obtener_valor_de_registro(pcb,"SI");
    int cantidad_accesos = obtener_cantidad_accesos(direccion_logica_inicial,direccion_logica_inicial + tamanio);
    int desplazamiento;
    int direccion_fisica, numero_pagina = 0;
    char* string;
    int cant_caracteres_leidos = 0;

    if(cantidad_accesos > 1){
        bool primer_acceso = true;
        for(int i = 0; i < cantidad_accesos; i++){

            numero_pagina = floor((direccion_logica_inicial + cant_caracteres_leidos) / tamaño_pagina);
            desplazamiento = direccion_logica_inicial + cant_caracteres_leidos - numero_pagina * tamaño_pagina;

            if(cantidad_entradas_TLB != 0)
                direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
            else direccion_fisica = -1;

            if(direccion_fisica < 0){
                direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
                if(cantidad_entradas_TLB != 0)
                    agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
            }

            int tamanio_sacado = tamaño_pagina - desplazamiento;

            if(primer_acceso){
                send_dos_int(direccion_fisica, tamanio_sacado, memoria_fd, LECTURA);
                send_int(pcb -> contexto -> pid,memoria_fd,0);
                string = recv_notificacion(memoria_fd); 
                cant_caracteres_leidos = tamanio_sacado;
                string[cant_caracteres_leidos] = '\0';
                primer_acceso = false;
                log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%s>",pcb -> contexto -> pid, "Lectura", direccion_fisica, string);
            }
            else{
                if(tamanio - cant_caracteres_leidos > tamaño_pagina){
                    send_dos_int(direccion_fisica, tamaño_pagina , memoria_fd, LECTURA);
                    send_int(pcb -> contexto -> pid,memoria_fd,0);
                    cant_caracteres_leidos += tamaño_pagina;
                }else{
                    send_dos_int(direccion_fisica, tamanio - cant_caracteres_leidos, memoria_fd, LECTURA);
                    send_int(pcb -> contexto -> pid,memoria_fd,0);
                    cant_caracteres_leidos += tamanio - cant_caracteres_leidos;
                }
                char* string_nuevo = recv_notificacion(memoria_fd);
                string_append(&string, string_nuevo);
                string[cant_caracteres_leidos] = '\0';
                
                free(string_nuevo);
                log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%s>",pcb -> contexto -> pid, "Lectura", direccion_fisica, string);
            }
        }
        
    }
    else{
        numero_pagina = floor(direccion_logica_inicial / tamaño_pagina);
        desplazamiento = direccion_logica_inicial - numero_pagina * tamaño_pagina;

        if(cantidad_entradas_TLB != 0)
            direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
        else direccion_fisica = -1;

        if(direccion_fisica < 0){
            direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
            if(cantidad_entradas_TLB != 0)
                agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
        }
        send_dos_int(direccion_fisica, tamanio, memoria_fd, LECTURA);
        send_int(pcb -> contexto -> pid,memoria_fd,0);
        string = recv_notificacion(memoria_fd);
        log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%s>",pcb -> contexto -> pid, "Lectura", direccion_fisica, string);
    }
    
    direccion_logica_inicial = obtener_valor_de_registro(pcb,"DI");
    cantidad_accesos = obtener_cantidad_accesos(direccion_logica_inicial,direccion_logica_inicial + tamanio);
    numero_pagina = 0;

    if(cantidad_accesos > 1){
        int tamanio_restante_a_sacar, cant_caracteres_sacados = 0;
        bool primer_acceso = true;
        char* sub_string;
        for(int i = 0; i < cantidad_accesos; i++){

            numero_pagina = floor((direccion_logica_inicial + cant_caracteres_sacados) / tamaño_pagina);
            desplazamiento = direccion_logica_inicial + cant_caracteres_sacados - numero_pagina * tamaño_pagina;

            if(cantidad_entradas_TLB != 0)
                direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
            else direccion_fisica = -1;

            if(direccion_fisica < 0){
                direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
                if(cantidad_entradas_TLB != 0)
                    agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
            }

            int tamanio_sacado = tamaño_pagina - desplazamiento; //Los bytes disponibles para escribir de la pagina

            if(primer_acceso){
                tamanio_restante_a_sacar = tamanio - tamanio_sacado;
                sub_string = string_substring(string, cant_caracteres_sacados, tamanio_sacado);
                send_df_dato(direccion_fisica,sub_string, tamanio_sacado, memoria_fd, ESCRIBIR);
                send_int(tamanio_sacado,memoria_fd,0);
                send_int(pcb -> contexto -> pid,memoria_fd,0);
                free(sub_string);
                log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%s>",pcb -> contexto -> pid, "Escritura", direccion_fisica, string);
                cant_caracteres_sacados = tamanio_sacado;
                primer_acceso = false;
                free(recv_notificacion(memoria_fd));
            }
            else if (tamanio_restante_a_sacar > tamaño_pagina){
                sub_string = string_substring(string, cant_caracteres_sacados, tamaño_pagina);
                cant_caracteres_sacados += tamaño_pagina;
                tamanio_restante_a_sacar -= tamaño_pagina;
                log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%s>",pcb -> contexto -> pid, "Escritura", direccion_fisica, string);
                send_df_dato(direccion_fisica, sub_string, tamaño_pagina,memoria_fd, ESCRIBIR);
                send_int(tamaño_pagina,memoria_fd,0);
                send_int(pcb -> contexto -> pid,memoria_fd,0);
                free(sub_string);
                free(recv_notificacion(memoria_fd));
            }else{
                sub_string = string_substring(string, cant_caracteres_sacados, tamanio_restante_a_sacar);
                log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%s>",pcb -> contexto -> pid, "Escritura", direccion_fisica, string);
                send_df_dato(direccion_fisica, sub_string, tamanio_restante_a_sacar,memoria_fd, ESCRIBIR);
                send_int(tamanio_restante_a_sacar,memoria_fd,0);
                send_int(pcb -> contexto -> pid,memoria_fd,0);
                free(sub_string);
                free(recv_notificacion(memoria_fd));
            }
        }
    } else{
        numero_pagina = floor(direccion_logica_inicial / tamaño_pagina);
        desplazamiento = direccion_logica_inicial - numero_pagina * tamaño_pagina;

        if(cantidad_entradas_TLB != 0)
            direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
        else direccion_fisica = -1;

        if(direccion_fisica < 0){
            direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
            if(cantidad_entradas_TLB != 0)
                agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
        }

        log_info(logger,"PID: <%d> - Acción: <%s> - Dirección Física: <%d> - Valor: <%s>",pcb -> contexto -> pid, "Escritura", direccion_fisica, string);
        send_df_dato(direccion_fisica, string, tamanio, memoria_fd, ESCRIBIR);
        send_int(tamanio,memoria_fd,0);
        send_int(pcb -> contexto -> pid,memoria_fd,0);
        free(recv_notificacion(memoria_fd));
    }

    free(string);
}

void operar_io_std(t_pcb* pcb, char* nombre_io, char* registro_direccion, char* registro_tamanio, int socket, int cod_op){
    pcb -> contexto -> motivo_bloqueo = INTERFAZ;
    int direccion_logica = obtener_valor_de_registro(pcb,registro_direccion);

    int tamanio_original = obtener_valor_de_registro(pcb,registro_tamanio);

    int cant_accesos = obtener_cantidad_accesos(direccion_logica, direccion_logica + tamanio_original);

    t_list* lista_direcciones_fisicas = list_create();
    t_list* lista_tamanios = list_create();

    int direccion_fisica;

    int numero_pagina = 0;
    int desplazamiento;

    if(cant_accesos > 1){
        int tamanio_restante_a_sacar = 0, tamanio_sacado = 0;
        bool primer_acceso = true;
        for(int i = 0; i < cant_accesos; i++){

            numero_pagina = floor((direccion_logica + tamanio_sacado) / tamaño_pagina);
            desplazamiento = direccion_logica + tamanio_sacado - numero_pagina * tamaño_pagina;

            if(cantidad_entradas_TLB != 0)
                direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
            else direccion_fisica = -1;

            if(direccion_fisica < 0){
                direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
                if(cantidad_entradas_TLB != 0)
                    agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
            }
            

            list_add(lista_direcciones_fisicas, direccion_fisica);
            
            if(primer_acceso){
                tamanio_sacado = tamaño_pagina - desplazamiento;//Los bytes disponibles para escribir/leer de la pagina
                
                tamanio_restante_a_sacar = tamanio_original - tamanio_sacado;
                list_add(lista_tamanios, tamanio_sacado);
                primer_acceso = false;

            } else {
                if(tamanio_restante_a_sacar < tamaño_pagina){//si entra aca es porque hay lugar de sobra para ese tamanio_restante_a_sacar
                    list_add(lista_tamanios, tamanio_restante_a_sacar);
                }
                else{
                    list_add(lista_tamanios, tamaño_pagina);
                    tamanio_restante_a_sacar = tamanio_restante_a_sacar - tamaño_pagina;
                    tamanio_sacado += tamaño_pagina;
                }            
            }        
        }
    } else{

        numero_pagina = floor(direccion_logica / tamaño_pagina);
        desplazamiento = direccion_logica - numero_pagina * tamaño_pagina;

        if(cantidad_entradas_TLB != 0)
            direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
        else direccion_fisica = -1;

        if(direccion_fisica < 0){
            direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
            if(cantidad_entradas_TLB != 0)
                agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
        }

        list_add(lista_direcciones_fisicas, direccion_fisica);
        list_add(lista_tamanios, tamanio_original);
    }

    send_pcb(pcb, kernel_dispatch_fd, cod_op);
    send_paquete_df(lista_direcciones_fisicas, lista_tamanios, cant_accesos, nombre_io, cod_op, kernel_dispatch_fd);

    list_destroy(lista_direcciones_fisicas);
    list_destroy(lista_tamanios);

}

void operar_io_fs(t_pcb* pcb, char* nombre_io, char* nombre_archivo, char* registro_direccion, char* registro_tamanio, char* registro_puntero, int cod_op1, int cod_op2){
    
    int puntero_archivo = obtener_valor_de_registro(pcb, registro_puntero);

    pcb -> contexto -> motivo_bloqueo = INTERFAZ;

    int direccion_logica = obtener_valor_de_registro(pcb,registro_direccion);

    int tamanio_original = obtener_valor_de_registro(pcb,registro_tamanio);

    int cant_accesos = obtener_cantidad_accesos(direccion_logica, direccion_logica + tamanio_original);

    t_list* lista_direcciones_fisicas = list_create();
    t_list* lista_tamanios = list_create();

    int direccion_fisica;

    int numero_pagina = 0;
    int desplazamiento;

    if(cant_accesos > 1){
        int tamanio_restante_a_sacar = 0, tamanio_sacado = 0;
        bool primer_acceso = true;
        for(int i = 0; i < cant_accesos; i++){

            numero_pagina = floor((direccion_logica + tamanio_sacado) / tamaño_pagina);
            desplazamiento = direccion_logica + tamanio_sacado - numero_pagina * tamaño_pagina;

            if(cantidad_entradas_TLB != 0)
                direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
            else direccion_fisica = -1;

            if(direccion_fisica < 0){
                direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
                if(cantidad_entradas_TLB != 0)
                    agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
            }
            

            list_add(lista_direcciones_fisicas, direccion_fisica);
            
            if(primer_acceso){
                tamanio_sacado = tamaño_pagina - desplazamiento;//Los bytes disponibles para escribir/leer de la primer pagina
                
                tamanio_restante_a_sacar = tamanio_original - tamanio_sacado;
                list_add(lista_tamanios, tamanio_sacado);
                primer_acceso = false;
            } 
            else{
                if(tamanio_restante_a_sacar < tamaño_pagina)//si entra aca es porque hay lugar de sobra para ese tamanio_restante_a_sacar
                    list_add(lista_tamanios, tamanio_restante_a_sacar);
                else{
                    list_add(lista_tamanios, tamaño_pagina);
                    tamanio_restante_a_sacar = tamanio_restante_a_sacar - tamaño_pagina;
                    tamanio_sacado += tamaño_pagina;
                }
            }    
        }
    } 
    else{

        numero_pagina = floor(direccion_logica / tamaño_pagina);
        desplazamiento = direccion_logica - numero_pagina * tamaño_pagina;

        if(cantidad_entradas_TLB != 0)
            direccion_fisica = TLB(pcb -> contexto -> pid, numero_pagina) + desplazamiento;
        else direccion_fisica = -1;

        if(direccion_fisica < 0){
            direccion_fisica = MMU(pcb, numero_pagina, desplazamiento);
            if(cantidad_entradas_TLB != 0)
                agregar_a_tlb(numero_pagina, pcb -> contexto -> pid, direccion_fisica);
        }

        list_add(lista_direcciones_fisicas, direccion_fisica);
        list_add(lista_tamanios, tamanio_original);
    }

    send_pcb(pcb, kernel_dispatch_fd, cod_op1);

    send_instruccion(nombre_io,kernel_dispatch_fd,cod_op2);

    send_paquete_df(lista_direcciones_fisicas, lista_tamanios, cant_accesos, nombre_archivo, cod_op2, kernel_dispatch_fd);

    send_int(puntero_archivo, kernel_dispatch_fd, 0);

    list_destroy(lista_direcciones_fisicas);
    list_destroy(lista_tamanios);
}

void operar_semaforo(t_pcb* pcb, char* recurso, int cod_op){
    send_pcb(pcb, kernel_dispatch_fd, cod_op);
    send_notificacion(recurso, kernel_dispatch_fd, 0);
}

int MMU(t_pcb* pcb, int numero_pagina, int desplazamiento){

    send_dos_int(numero_pagina, pcb -> contexto -> pid, memoria_fd, DIRECCION_FISICA);
    recibir_operacion(memoria_fd);
    int marco = recv_int(memoria_fd); 

    log_info(logger,"PID: <%d> - OBTENER MARCO - Página: <%d> - Marco: <%d>", pcb -> contexto -> pid, numero_pagina, marco / tamaño_pagina);

    return marco + desplazamiento;
}

void iniciar_tlb(){
    lista_tlb = list_create();
}

int TLB(int pid, int pagina){


    if(list_is_empty(lista_tlb)){
        log_info(logger_obligatorio, "PID: <%d> - TLB MISS - Pagina: <%d>", pid, pagina);
        return -100;
    }
    else{
        
        int index = 0;
        t_list_iterator* iterator = list_iterator_create(lista_tlb);
        struct t_direcciones* paquete;

        while(list_iterator_has_next(iterator)){
            
            paquete = list_iterator_next(iterator);

            if(pid == paquete -> pid && pagina == paquete -> pagina){
                log_info(logger_obligatorio, "PID: <%d> - TLB HIT - Pagina: <%d>", pid, pagina);
                if(strcmp(algoritmo_TLB, "LRU") == 0){
                    list_add(lista_tlb, list_remove(lista_tlb, index));
                }
                list_iterator_destroy(iterator);
                return paquete -> marco * tamaño_pagina;
            }
            index += 1;
        }
        list_iterator_destroy(iterator);
        log_info(logger_obligatorio, "PID: <%d> - TLB MISS - Pagina: <%d>", pid, pagina);
        return -100;
    }
    
}

void agregar_a_tlb(int pagina, int pid, int direccion_fisica){

    struct t_direcciones* paquete = malloc(sizeof(struct t_direcciones));
    struct t_direcciones* nodo_sacado;

    int marco = floor(direccion_fisica/tamaño_pagina);

    paquete -> pagina = pagina;
    paquete -> pid = pid;
    paquete -> marco = marco;

    if(list_size(lista_tlb) < cantidad_entradas_TLB)
        list_add(lista_tlb, paquete);
    else if(strcmp(algoritmo_TLB, "FIFO") == 0){

        nodo_sacado = list_remove(lista_tlb, 0);
        list_add(lista_tlb, paquete);
        free(nodo_sacado);
    }
    else{// Entonces es LRU

        nodo_sacado = list_remove(lista_tlb, 0);
        list_add(lista_tlb, paquete);
        free(nodo_sacado);
    }
}

int obtener_tamanio_registro(char* registro){
    if(strcmp(registro,"AX") == 0){
        return 1;
    }else if(strcmp(registro,"BX") == 0){
        return 1;
    }else if(strcmp(registro,"CX") == 0){
        return 1;
    }else if(strcmp(registro,"DX") == 0){
        return 1;
    }else if(strcmp(registro,"EAX") == 0){
        return 4;
    }else if(strcmp(registro,"EBX") == 0){
        return 4;
    }else if(strcmp(registro,"ECX") == 0){
        return 4;
    }else if(strcmp(registro,"EDX") == 0){
        return 4;
    }else if(strcmp(registro,"SI") == 0){
       return 4;
    }else if(strcmp(registro,"DI") == 0){
       return 4;
    }
}

void modificar_quantum(t_pcb** pcb, t_temporal* quantum){
    temporal_stop(quantum);
    int sobrante = (*pcb) -> quantum - temporal_gettime(quantum);

    if(sobrante >= 0)
        (*pcb) -> quantum = sobrante;
    else 
        (*pcb) -> quantum = 0;
    temporal_destroy(quantum);
}

void terminar_CPU(){

    liberar_conexion(memoria_fd);
    liberar_conexion(kernel_dispatch_fd);
    liberar_conexion(kernel_interrupt_fd);
    log_destroy(logger);
    config_destroy(config);
}
