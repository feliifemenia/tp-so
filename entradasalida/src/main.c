#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "commons/txt.h"
#include "commons/string.h"
#include <dirent.h>
#include <string.h>
#include "main.h"


int main(int argc, char* argv[]) {

    logger = log_create("entradasalida.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    
    logger_obligatorio = log_create("loggersObligatorios.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    char* path = recibir_parametros_interfaz();
    config = iniciar_config(path); 
    free(path);
    
    while (config == NULL )
    {
        log_info(logger, "Ingrese archivo valido!");
        path = recibir_parametros_interfaz();
        config = iniciar_config(path); 
        free(path);
    }
    
    copiar_config();

    int interfaz = caso_interfaz(tipo_interfaz);
    
    crear_conexiones();

    switch (interfaz) { 
        case GENERICA : 
        //copiar aca
            log_info(logger, "INTERFAZ GENERICA");
            send_nombre_entradasalida(nombres[GENERICA], kernel_fd, GENERICA);
            send_nombre_entradasalida(nombres[GENERICA], memoria_fd, GENERICA);
            interfaz_generica(kernel_fd);
            break;
        case STDIN :
            log_info(logger, "INTERFAZ STDIN");
            send_nombre_entradasalida(nombres[STDIN], kernel_fd, STDIN);
            send_nombre_entradasalida(nombres[STDIN], memoria_fd, STDIN);
            interfaz_stdin(kernel_fd, memoria_fd);
            break;
        case STDOUT :
            log_info(logger, "INTERFAZ STDOUT");
            send_nombre_entradasalida(nombres[STDOUT], kernel_fd, STDOUT);
            send_nombre_entradasalida(nombres[STDOUT], memoria_fd, STDOUT);
            interfaz_stdout(kernel_fd,memoria_fd);
            break;
        case DIALFS :
            log_info(logger, "INTERFAZ DIALFS");
            send_nombre_entradasalida(nombres[DIALFS], kernel_fd, DIALFS);
            send_nombre_entradasalida(nombres[DIALFS], memoria_fd, DIALFS);
            copiar_config_dialfs();
            interfaz_filesystem(kernel_fd, memoria_fd);
            break;
        case -1:
            log_error (logger, "Hubo un error!");
            terminar_ENTRADASALIDA();
            exit(EXIT_FAILURE);
            break;

        default: 
            log_error (logger, "Hubo un error!");
            break;
    }

    terminar_ENTRADASALIDA(kernel_fd,memoria_fd);
    return 0;
}

t_config* iniciar_config(char* path_config){
    t_config* nuevo_config = config_create(path_config);

    if (nuevo_config == NULL) {
        log_error(logger, "Config no encontrado");
        nuevo_config = NULL;
        return nuevo_config;
    }
    return nuevo_config;
}

void copiar_config(){ //generica, stdin, stdout copian las mismas propiedades
    tipo_interfaz = config_get_string_value(config, "TIPO_INTERFAZ") ;
    ip_kernel = config_get_string_value(config, "IP_KERNEL"); 
    puerto_kernel = config_get_string_value(config, "PUERTO_KERNEL") ;
    ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    

}

void copiar_config_dialfs(){
    tipo_interfaz = config_get_string_value(config, "TIPO_INTERFAZ") ;
    tiempo_unidad_trabajo = config_get_int_value(config, "TIEMPO_UNIDAD_TRABAJO") ;
    ip_kernel = config_get_string_value(config, "IP_KERNEL"); 
    puerto_kernel = config_get_string_value(config, "PUERTO_KERNEL") ;
    ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    path_base_dialfs= config_get_string_value(config, "PATH_BASE_DIALFS");
    block_size = config_get_int_value(config, "BLOCK_SIZE");
    block_count = config_get_int_value(config, "BLOCK_COUNT");
    retraso_compactacion = config_get_int_value(config, "RETRASO_COMPACTACION");
}

void crear_conexiones(){
    kernel_fd = crear_conexion(ip_kernel, puerto_kernel);
    memoria_fd = crear_conexion(ip_memoria, puerto_memoria); 
}

char* recibir_parametros_interfaz(){
    log_info(logger, "Ingresar Path de Archivo de Configuracion: ");
    char* path = readline("> ");
    return path;
}

int caso_interfaz(char* nombre_interfaz) {
    for(int i = 0; i < 4; i++)
        if(strcmp(nombre_interfaz, interfaces[i]) == 0)
            return i;

    return -1;
}

void interfaz_generica(int kernel_fd){
    int pid = 0;
    int cod_op,cantidad_unidades_trabajo = 0; 

    while(1){
        cod_op = recibir_operacion(kernel_fd);
       
        switch (cod_op){
        case MANEJAR_IO_GEN_SLEEP:
            cantidad_unidades_trabajo = recv_sleep(kernel_fd, &pid);
            log_info(logger_obligatorio, "PID: <%d> - Operacion: <SLEEP: %d>", pid, cantidad_unidades_trabajo);
            sleep(cantidad_unidades_trabajo);
            send_notificacion("Ok", kernel_fd, 0);
            break;
        
        default:
            log_error(logger,"Desconexion de Kernel");
            terminar_ENTRADASALIDA();
            exit(EXIT_FAILURE);
            break;
        }
        
    }
}

void interfaz_stdin (int kernel_fd, int memoria_fd) {
    int pid = 0, tamanio = 0, direccion_fisica = 0, cant_accesos = 0,cod_op;
    char* texto_ingresado = NULL;

    while(1) {

        cod_op = recibir_operacion(kernel_fd);
        if(cod_op == -1){
            terminar_ENTRADASALIDA();
            exit(EXIT_FAILURE);
        }
        pid = recv_int(kernel_fd);

        t_list* lista_df = list_create();
        t_list* lista_tam = list_create();

        recibir_operacion(kernel_fd);
        free(recv_paquete_df(&cant_accesos, lista_df, lista_tam, kernel_fd));

        for(int i = 0; i < cant_accesos; i++){
            tamanio += list_get(lista_tam, i);
        }

        log_info(logger_obligatorio, "PID: <%d> - Operacion: <STDIN> - Tamanio: <%d>", pid, tamanio);
        
        texto_ingresado = readline("> Ingrese un texto: ");
        
        while(strlen(texto_ingresado) > tamanio){
            free(texto_ingresado);
            texto_ingresado = readline("> Ingrese un texto de tamanio valido: ");
        }
        tamanio = 0;
        
        int caracteres_a_enviar, desplazamiento_palabra = 0;
        char* dato = NULL;

        for(int i = 0; i < cant_accesos; i++){
            caracteres_a_enviar = list_get(lista_tam, i);
            dato = string_substring(texto_ingresado, desplazamiento_palabra, caracteres_a_enviar);
            direccion_fisica = list_get(lista_df, i);
            send_df_dato(direccion_fisica, dato, caracteres_a_enviar, memoria_fd, IO_STDIN_READ); //FALTA HACER EL ENUM MEMORIA-IO
            send_int(pid,memoria_fd,0);
            desplazamiento_palabra += caracteres_a_enviar;
            free(recv_notificacion(memoria_fd));
            free(dato);
        }
        
        free(texto_ingresado);
        list_destroy(lista_df);
        list_destroy(lista_tam);
        send_notificacion("LISTO", kernel_fd, 0);
    }
}

void interfaz_stdout(int kernel_fd, int memoria_fd){
    int pid = 0, direccion_fisica = 0, tamanio = 0, cant_accesos = 0,cod_op;
    
    char* subString = NULL;

    while (1) {

        cod_op = recibir_operacion(kernel_fd);
        if(cod_op == -1){
            terminar_ENTRADASALIDA();
            exit(EXIT_FAILURE);
        }
        pid = recv_int(kernel_fd);
        int tamanio_total = 0;
        char* texto_leido = string_new();

        t_list* lista_df = list_create();
        t_list* lista_tam = list_create();

        recibir_operacion(kernel_fd);
        free(recv_paquete_df(&cant_accesos, lista_df, lista_tam, kernel_fd));

        log_info(logger_obligatorio, "PID: <%d> - Operacion: <STDOUT>", pid);
        
        for(int i = 0; i < cant_accesos; i++){
            direccion_fisica = list_get(lista_df, i);
            tamanio = list_get(lista_tam, i);
            send_dos_int(direccion_fisica, tamanio, memoria_fd, IO_STDOUT_WRITE);//FALTA HACER EL ENUM MEMORIA-IO
            send_int(pid,memoria_fd,0);
            tamanio_total += tamanio;

            subString = recv_notificacion(memoria_fd);
            subString[tamanio] = '\0';

            string_append(&texto_leido, subString);
            free(subString);
        }

        texto_leido[tamanio_total] = '\0';

        log_info(logger, "%s", texto_leido);
        send_notificacion("LISTO", kernel_fd, 100);

        free(texto_leido);
        list_destroy(lista_df);
        list_destroy(lista_tam);
    }
}

void interfaz_filesystem(int kernel_fd, int memoria_fd){

    crear_archivo_bloques();
    crear_bit_map();
    lista_fcbs = list_create();
    recuperar_lista();

    int pid = -1, cod_op = -1,bloque_inicial = 0, tamanio_archivo = 0, cantidad_accesos = 0, index = 0, puntero_archivo = 0, direccion_fisica = 0, cant_accesos = 0, numero_de_bloque = 0, desplazamiento_en_bloque = 0, posicion_bloque = 0, tamanio_total = 0, tamanio = 0;
    
    t_list* lista_df = NULL;
    t_list* lista_tam = NULL;
    t_fcb* fcb = NULL;

    while(1){
        cod_op = recibir_operacion(kernel_fd);

        if(cod_op == -1){
            terminar_fs();
            exit(EXIT_FAILURE);
        }
        
        pid = recv_int(kernel_fd);

        char* path = string_new();
        char* nombre_archivo = NULL;
        char* itoa_char = NULL;
        switch (cod_op)
        {
        case IO_FS_CREATE:
            
            nombre_archivo = recv_notificacion(kernel_fd);

            string_append(&path, path_base_dialfs);
            string_append(&path, nombre_archivo);

            t_fcb* paquete = malloc(sizeof(t_fcb));

            paquete -> nombre = nombre_archivo;

            paquete -> tamanio_archivo = 0;

            tamanio_archivo = 0;

            if(tamanio_archivo == 0 && espacio_disponible() >= block_size){//es la creacion de un archivo
                bloque_inicial = dar_bloque_libre();
                sincronizar_bitmap();
            }
            paquete -> bloque_inicial = bloque_inicial;

            list_add(lista_fcbs, paquete);

            FILE* archivo_nuevo = fopen(path, "w");

            char* string_bloque_inicial = string_new();
            string_append(&string_bloque_inicial, "BLOQUE_INICIAL=");
            itoa_char = string_itoa(bloque_inicial);
            string_append(&string_bloque_inicial, itoa_char);
            string_append(&string_bloque_inicial, "\n");

            free(itoa_char);

            char* string_tamanio = string_new();
            string_append(&string_tamanio, "TAMANIO_ARCHIVO=");
            itoa_char = string_itoa(tamanio_archivo);
            string_append(&string_tamanio, itoa_char);

            txt_write_in_file(archivo_nuevo, string_bloque_inicial);
            txt_write_in_file(archivo_nuevo, string_tamanio);

            log_info(logger, "PID: <%d> - Crear Archivo: <%s>", pid , nombre_archivo);

            fclose(archivo_nuevo);

            free(itoa_char);
            free(string_bloque_inicial);
            free(string_tamanio);
            free(path);
            send_notificacion("OK", kernel_fd, 0);

            break;
        case IO_FS_DELETE:
            index = 0;
            int bloques_a_eliminar = 0;
            nombre_archivo = recv_notificacion(kernel_fd);
            
            t_fcb* fcb_a_eliminar = buscar_fcb(nombre_archivo, &index);
            
            if (fcb_a_eliminar -> tamanio_archivo == 0)
            {
                bloques_a_eliminar = 1;
            }
            else {
                bloques_a_eliminar = (fcb_a_eliminar -> tamanio_archivo / block_size) ;
            }

            liberar_bloques(fcb_a_eliminar-> bloque_inicial, fcb_a_eliminar->tamanio_archivo, bloques_a_eliminar );
            string_append(&path , path_base_dialfs);
            string_append(&path , nombre_archivo);

            remove(path);

            log_info(logger, "PID: <%d> - Eliminar Archivo: <%s>", pid , nombre_archivo);

            fcb_a_eliminar = list_remove(lista_fcbs, index - 1);//le resto uno porq en buscar fcb tuve que correrlo
            free(fcb_a_eliminar ->nombre);
            free(fcb_a_eliminar);
            free(path);
            free(nombre_archivo);

            send_notificacion("OK", kernel_fd, 0);
            break;

        case IO_FS_TRUNCATE:
            int index_fcb_en_lista = 0;

            recibir_operacion(kernel_fd);
            int tamanio_nuevo = recv_int(kernel_fd);
            nombre_archivo = recv_notificacion(kernel_fd);

            int bloques_a_agregar = 0;
            
            t_fcb* fcb_a_truncar = buscar_fcb(nombre_archivo, &index_fcb_en_lista);
            
            int tamanio_actual = fcb_a_truncar -> tamanio_archivo;

            string_append(&path , path_base_dialfs);
            string_append(&path , nombre_archivo); 
            t_config* config_truncar = config_create(path);

            int bloques_actuales = 0;

            if(tamanio_archivo == 0 && tamanio_nuevo < block_size){// es que solo hay que modificar el tamanio, porque el nuevo tamanio es mas chico que el bloque q ya tiene
                tamanio_actual = 0;  
                fcb_a_truncar -> tamanio_archivo = tamanio_nuevo;
            }
            else if(tamanio_actual == 0){
                bloques_actuales = 1;
                tamanio_actual = 0;  
                fcb_a_truncar -> tamanio_archivo = block_size;
            }
            else {
                bloques_actuales = tamanio_actual / block_size;
            }

            log_info(logger_obligatorio, "PID: <%d> - Truncar Archivo: <%s> - Tamaño: <%d>", pid, nombre_archivo, tamanio_nuevo);
            
            if (tamanio_nuevo - tamanio_actual > 0){ // entonces hay que agrandar el archivo
                int tamanio_a_ampliar = tamanio_nuevo - tamanio_actual;
              
                if (espacio_disponible() >= tamanio_a_ampliar){ // se puede ampliar
                    bloques_a_agregar = tamanio_a_ampliar / block_size;

                    int bloques_totales = tamanio_nuevo / block_size;
                    int resto = tamanio_a_ampliar % block_size;

                    if(resto != 0 && tamanio_a_ampliar > block_size)
                        bloques_totales++;

                    int index = 0;

                    if (bloques_a_agregar == 0){

                        //usleep(retraso_compactacion * 1000);
                        itoa_char = string_itoa(fcb_a_truncar -> tamanio_archivo);
                        config_set_value(config_truncar, "TAMANIO_ARCHIVO", itoa_char);
                        config_save(config_truncar);
                        config_destroy(config_truncar);
                        free(itoa_char);
                    }
                    else if(comprobar_contiguos(bloques_a_agregar, fcb_a_truncar)) //si es verdadero tiene los bloques contiguos nececsarios, osea no hace falta truncar
                    {
                        //asignar_bloques_al_archivo(&(fcb_a_truncar -> bloque_inicial), &(fcb_a_truncar -> tamanio_archivo), bloques_a_agregar);
                        asignar_bloques(fcb_a_truncar -> bloque_inicial + 1, bloques_a_agregar);
                        fcb_a_truncar -> tamanio_archivo = tamanio_nuevo;

                        //usleep(retraso_compactacion * 1000);
                        itoa_char = string_itoa(fcb_a_truncar -> tamanio_archivo);
                        config_set_value(config_truncar, "TAMANIO_ARCHIVO", itoa_char);
                        config_save(config_truncar);
                        config_destroy(config_truncar);
                        free(itoa_char);
                    }
                    else if(buscar_contiguos(bloques_totales, &index)) {//busco si en algun otro lugar del FS hay la cantidad de bloques contiguas necesitadas

                        asignar_bloques(index, bloques_totales);
                        
                        int bloques_a_eliminar = fcb_a_truncar -> tamanio_archivo/ block_size;

                        // lo unico que falta aca es copiar el archivo a los nuevos bloques
                        void* contenido = malloc(block_size);
                        for (int i = 0; i < bloques_a_eliminar; i++)
                        {
                            leer_bloque(fcb_a_truncar -> bloque_inicial + i, contenido, block_size, 0);
                            escribir_bloque(index + i , contenido, block_size, 0);
                            bitarray_clean_bit(bit_map, fcb_a_truncar -> bloque_inicial + i);
                        }

                        sincronizar_bitmap();
                        
                        //liberar_bloques(fcb_a_truncar -> bloque_inicial, fcb_a_truncar -> tamanio_archivo, bloques_a_eliminar);
                        fcb_a_truncar -> bloque_inicial = index;
                        fcb_a_truncar -> tamanio_archivo = tamanio_nuevo;

                        usleep(retraso_compactacion * 1000);
                        itoa_char = string_itoa(fcb_a_truncar -> bloque_inicial);
                        config_set_value(config_truncar, "BLOQUE_INICIAL", itoa_char);
                        free(itoa_char);
                        itoa_char = string_itoa(fcb_a_truncar -> tamanio_archivo);
                        config_set_value(config_truncar, "TAMANIO_ARCHIVO", itoa_char);
                        config_save(config_truncar);
                        config_destroy(config_truncar);
                        free(contenido);
                    }
                    else{//hay que realizar compactacion
                        //leo el contenido del archivo que queremos truncar antes de hacer la compactacion, para no perderlo y despues agregarlo una vez hecha la compactacion. 
                        void* contenido_archivo = malloc(fcb_a_truncar -> tamanio_archivo);

                        memcpy(contenido_archivo, fs_bloques + (fcb_a_truncar -> bloque_inicial * block_size), fcb_a_truncar -> tamanio_archivo);
                        
                        log_info(logger_obligatorio, "PID: <%d> - Inicio Compactación", pid);
                        hacer_compactacion(fcb_a_truncar);
                        usleep(retraso_compactacion * 1000);
                        log_info(logger_obligatorio, "PID: <%d> - Fin Compactación.", pid);


                        //copio el archivo nuevo en el archivo original
                        int bloques_nuevos = tamanio_nuevo / block_size;
                        int resto = tamanio_nuevo % block_size;

                        if(resto != 0)
                            bloques_nuevos++;

                        index = 0;
                        buscar_contiguos(bloques_nuevos, &index);

                        memcpy(fs_bloques + (index * block_size), contenido_archivo, tamanio_archivo);
                        
                        fcb_a_truncar -> bloque_inicial = index;
                        fcb_a_truncar -> tamanio_archivo = tamanio_nuevo;

                        for(int i = 0; i < bloques_nuevos; i++){
                            bitarray_set_bit(bit_map, fcb_a_truncar -> bloque_inicial + i);
                        }

                        sincronizar_bitmap();

                        FILE* archivo_nuevo = fopen(path, "w");

                        char* string_bloque_inicial = string_new();
                        
                        string_append(&string_bloque_inicial, "BLOQUE_INICIAL=");
                        itoa_char = string_itoa(fcb_a_truncar -> bloque_inicial);
                        string_append(&string_bloque_inicial, itoa_char);
                        string_append(&string_bloque_inicial, "\n");
                        
                        txt_write_in_file(archivo_nuevo, string_bloque_inicial);
                        
                        free(itoa_char);
                        
                        char* string_tamanio = string_new();
                        string_append(&string_tamanio, "TAMANIO_ARCHIVO=");
                        itoa_char = string_itoa(fcb_a_truncar -> tamanio_archivo);
                        string_append(&string_tamanio, itoa_char);
                        
                        txt_write_in_file(archivo_nuevo, string_tamanio);
                        
                        free(itoa_char);

                        //free(string_bloque_inicial);
                        //free(string_tamanio);
                        fclose(archivo_nuevo);
                    }

                }
                else {
                    //no se puede ampliar
                }
            }
            else { //entonces hay que achicarlo
                int tamanio_a_achicar = abs (tamanio_nuevo - tamanio_actual);
                int cantidad_bloques_a_eliminar = tamanio_a_achicar / block_size;
                
                liberar_bloques(fcb_a_truncar -> bloque_inicial, fcb_a_truncar -> tamanio_archivo, cantidad_bloques_a_eliminar);
                fcb_a_truncar -> tamanio_archivo = tamanio_nuevo;
                t_config* config_a_truncar = iniciar_config(path);
                itoa_char = string_itoa(tamanio_nuevo);
                config_set_value (config_a_truncar , "TAMANIO_ARCHIVO", itoa_char);
                config_destroy(config_a_truncar);
                free(itoa_char);
                
            }
            free(path);
            free(nombre_archivo);
            send_notificacion("OK", kernel_fd, 0);
            break;

        case IO_FS_READ:

            nombre_archivo = recv_notificacion(kernel_fd);

            lista_df = list_create();
            lista_tam = list_create();

            recibir_operacion(kernel_fd);
            free(recv_paquete_df(&cantidad_accesos, lista_df, lista_tam, kernel_fd));

            recibir_operacion(kernel_fd);
            puntero_archivo = recv_int(kernel_fd);

            fcb = buscar_fcb(nombre_archivo, &index);

            posicion_bloque = floor(puntero_archivo / block_size); 
            desplazamiento_en_bloque = puntero_archivo - (numero_de_bloque*block_size);
            numero_de_bloque = fcb -> bloque_inicial + posicion_bloque;

            for (int i = 0; i < list_size(lista_tam); i++)
            {
                tamanio_total += list_get(lista_tam, i);
            }
            
            char* contenido = malloc(tamanio_total);

            char* parte = string_new();

            leer_bloque(numero_de_bloque, contenido, tamanio_total, desplazamiento_en_bloque);
            int inicio = 0;

            log_info(logger_obligatorio,"PID: <%d> - Leer Archivo: <%s> - Tamaño a Leer: <%d> - Puntero Archivo: <%d>",pid,nombre_archivo,tamanio_total,puntero_archivo);
            
            for(int i = 0; i < cant_accesos; i++){
                direccion_fisica = list_get(lista_df, i);
                tamanio = list_get(lista_tam, i);

                parte = string_substring(contenido, inicio, tamanio);

                inicio += tamanio;

                send_df_dato(direccion_fisica, parte, tamanio, memoria_fd, IO_FS_READ);
                send_int(pid,memoria_fd,0);
                free(recv_notificacion(memoria_fd));
            }

            tamanio_total = 0;

            list_destroy(lista_df);
            list_destroy(lista_tam);
            free(path);
            free(parte);
            free(contenido);  
            free(nombre_archivo); 
            send_notificacion("OK", kernel_fd, 0);
            break;
        case IO_FS_WRITE:

            nombre_archivo = recv_notificacion(kernel_fd);
            
            char* subString = NULL;
            char* texto_leido = string_new();
            
            lista_df = list_create();
            lista_tam = list_create();

            recibir_operacion(kernel_fd);
            free(recv_paquete_df(&cant_accesos, lista_df, lista_tam, kernel_fd));

            recibir_operacion(kernel_fd);
            puntero_archivo = recv_int(kernel_fd);

            fcb = buscar_fcb(nombre_archivo, &index);

            for(int i = 0; i < cant_accesos; i++){//leemos el dato entero de memoria
                direccion_fisica = list_get(lista_df, i);
                tamanio = list_get(lista_tam, i);
                send_dos_int(direccion_fisica, tamanio, memoria_fd, IO_FS_WRITE);
                send_int(pid,memoria_fd,0);

                tamanio_total += tamanio;

                subString = recv_notificacion(memoria_fd);
                subString[tamanio] = '\0';

                string_append(&texto_leido, subString);
                free(subString);
            }

            log_info(logger_obligatorio, "PID: <%d> - Escribir Archivo: <%s> - Tamaño a Escribir: <%d> - Puntero Archivo: <%d>", pid, nombre_archivo, tamanio_total, puntero_archivo);

            texto_leido[tamanio_total] = '\0';

            posicion_bloque = floor (puntero_archivo / block_size); 
            desplazamiento_en_bloque = puntero_archivo - (numero_de_bloque * block_size);

            numero_de_bloque = fcb -> bloque_inicial + posicion_bloque;

            escribir_bloque(numero_de_bloque, texto_leido, tamanio_total, desplazamiento_en_bloque);

            tamanio_total = 0;
            numero_de_bloque = 0;

            free(texto_leido);
            list_destroy(lista_df);
            list_destroy(lista_tam);
            free(nombre_archivo);
            free(path);
            send_notificacion("OK", kernel_fd, 0);
            break;

        case -1: 
            log_error(logger, "Desconexion del fs");
            terminar_fs();
            exit(EXIT_FAILURE);
            break;
        }
    }
}

void asignar_bloques(int bloque, int cantidad_bloques){
    for (int i=0 ; i<cantidad_bloques ; i++ ){
        bitarray_set_bit(bit_map, bloque + i); 
    }
    sincronizar_bitmap();
}

void crear_bit_map(){

    tamanio_bit_map = block_count / 8;
    
    void* bits = malloc(tamanio_bit_map);

    bit_map = bitarray_create_with_mode(bits, tamanio_bit_map, LSB_FIRST);

    char* path = string_new();
    string_append(&path, path_base_dialfs);
    string_append(&path, "bitmap.dat"); 

    bool ya_creado = true;

    if(!file_exists_in_directory(path_base_dialfs, "bitmap.dat")){
        FILE* archivo_bitmap = fopen(path, "wb");
        fclose(archivo_bitmap);

        ya_creado = false;
    }

    for(int i=0; i < block_count; i++){//Empiezan en FALSE todos
            bitarray_clean_bit(bit_map,i);  //False = Libre(clean), True = Ocupado(setted)
    }


    fd_bit_map = open(path, O_RDWR, S_IRUSR | S_IWUSR);//Se supone q el archivo ya existe, sino da error

    if (fd_bit_map == -1) {
        perror("Error al abrir o crear el archivo");
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_bit_map, tamanio_bit_map) == -1) {
        perror("Error al truncar el archivo");
        close(fd_bit_map);
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }

    fs = mmap(NULL, tamanio_bit_map, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bit_map, 0);
    if (fs == MAP_FAILED) {
        perror("Error en mmap");
        close(fd_bit_map);
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }
    
    if(ya_creado){
        memcpy(bit_map -> bitarray, fs, tamanio_bit_map);
        for(int i = 0; i < 32; i++){
            log_warning(logger, "Posicion %d: %d", i, bitarray_test_bit(bit_map, i));
        }
    }else{
        for(int i = 0; i < 32; i++){
            log_warning(logger, "Posicion %d: %d", i,bitarray_test_bit(bit_map,i));
        }
        sincronizar_bitmap();
    }

    free(path);
}

void sincronizar_bitmap(){
    memcpy(fs, bit_map -> bitarray, tamanio_bit_map);
    if (msync(fs, tamanio_bit_map, MS_SYNC) == -1) {
        perror("Error en msync");
        munmap(fs, tamanio_bit_map);
        close(fd_bit_map);
        terminar_fs();
        exit(EXIT_FAILURE);
    }
}

void crear_archivo_bloques(){

    int tamanio_archivo_bloques = block_size * block_count;

    char* path = string_new();
    string_append(&path, path_base_dialfs);
    string_append(&path, "bloques.dat"); 

    if(!file_exists_in_directory(path_base_dialfs, "bloques.dat")){
        FILE* archivo_bloques = fopen(path, "wb");
        fclose(archivo_bloques);
    }

    fd_bloques = open(path, O_RDWR, S_IRUSR | S_IWUSR);//Se supone q el archivo ya existe, sino da error

    if (fd_bloques == -1) {
        perror("Error al abrir o crear el archivo");
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_bloques, tamanio_archivo_bloques) == -1) {
        perror("Error al truncar el archivo");
        close(fd_bit_map);
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }

    fs_bloques = mmap(NULL, tamanio_archivo_bloques, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bloques, 0);
    if (fs == MAP_FAILED) {
        perror("Error en mmap");
        close(fd_bit_map);
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }

    free(path);

}

void recuperar_lista(){
    DIR *d;
    struct dirent *dir;
    d = opendir(path_base_dialfs);
    t_fcb* fcb = NULL;
    int tamanio = 0, bloque_inicial = 0;
    char* config = NULL;
    t_config* config_arch = NULL;
    char* nombre_archivo = NULL;

    if (d) {
        while ((dir = readdir(d)) != NULL) {
        
            if((strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)){

                if(strcmp(dir->d_name, "bitmap.dat") != 0 && strcmp(dir->d_name, "bloques.dat") != 0){
                    //printf("%s\n", dir->d_name); <--- muestra los archivos que estaban en el directorio
                    
                    config = string_new();
                    string_append(&config, path_base_dialfs);
                    string_append(&config, dir->d_name);

                    config_arch = config_create(config);
                    bloque_inicial = config_get_int_value(config_arch, "BLOQUE_INICIAL");
                    tamanio = config_get_int_value(config_arch, "TAMANIO_ARCHIVO");

                    fcb = malloc(sizeof(t_fcb));
                    nombre_archivo = string_duplicate(dir->d_name);
                    fcb -> nombre = nombre_archivo;
                    fcb -> bloque_inicial = bloque_inicial;
                    fcb -> tamanio_archivo = tamanio;
                    list_add(lista_fcbs, fcb);
                    config_destroy(config_arch);
                    free(config);
                }

            }
        }
        closedir(d);
    } else {
        perror("opendir");
    }
}

int file_exists_in_directory(const char* directory, const char* filename) {
    struct dirent *entry = NULL;
    DIR *dp = opendir(directory);

    if (dp == NULL) {
        perror("opendir");
        return 0;
    }

    while ((entry = readdir(dp))) {
        if (strcmp(entry->d_name, filename) == 0) {
            closedir(dp);
            return 1; // Archivo encontrado
        }
    }

    closedir(dp);
    return 0; // Archivo no encontrado
}

void asignar_bloques_al_archivo(int* bloque_inicial, int* tamanio_archivo, int cantidad_bloques){
    if(*tamanio_archivo == 0 && espacio_disponible() >= block_size){//es la creacion de un archivo
        *bloque_inicial = dar_bloque_libre();
        sincronizar_bitmap();
    }
    
    else {
        int bloque_final = *bloque_inicial + (*tamanio_archivo / block_size) - 1;
        asignar_contiguos(bloque_final + 1, cantidad_bloques);
        sincronizar_bitmap();
    }
}

void asignar_contiguos(int bloque_final, int cantidad_bloques){

    for (int i=0 ; i<=cantidad_bloques ; i++ ){
        bitarray_set_bit(bit_map, bloque_final + i); 
    }
    sincronizar_bitmap();

}

void leer_bloque (int bloque_a_leer, char* bloque_leido,int tamanio, int desplazamiento){
    int direccion_bloque = bloque_a_leer * block_size;
    
    memcpy (bloque_leido, fs_bloques + direccion_bloque + desplazamiento, tamanio);
    sincronizar_archivo_bloques();
}

void escribir_bloque(int bloque_a_escribir, char* contenido, int tamanio, int desplazamiento){
    int direccion_bloque = bloque_a_escribir * block_size;

    memcpy (fs_bloques + direccion_bloque + desplazamiento, contenido, tamanio);
    sincronizar_archivo_bloques();
}

bool buscar_contiguos(int cantidad_bloques, int* index){

    int i = 0;
    int bloques_libres = 0;

    while(i<block_count){
        
        if(bitarray_test_bit(bit_map, i ) == false){
            bloques_libres++;
            if(bloques_libres == cantidad_bloques){
                
                *index = i - cantidad_bloques + 1;
                return true;

            }
            
        }
        else {
            bloques_libres = 0;
        }
        i++;
    }
    return false;
}

t_fcb* buscar_fcb(char* nombre_archivo, int* index)
{    
    t_list_iterator* iterator = list_iterator_create(lista_fcbs);
    t_fcb* paquete = NULL;
    (*index) = 0;
    while(list_iterator_has_next(iterator)){
        
        paquete = list_iterator_next(iterator);
        (*index)++;
        if(strcmp(paquete -> nombre, nombre_archivo)  == 0){
            list_iterator_destroy(iterator);
            return paquete;
        }
    }
    list_iterator_destroy(iterator);
    return NULL;
}

void liberar_bloques (int bloque_inicial, int tamanio_archivo, int cant_bloques_a_eliminar)
{
    int cantidad_de_bloques = tamanio_archivo / block_size;
    int bloque_final = bloque_inicial + cantidad_de_bloques - 1;
    for (int i = 0; i < cant_bloques_a_eliminar; i++ )
    {
        bitarray_clean_bit(bit_map, bloque_final);
        bloque_final--;
    }
        sincronizar_bitmap();
    
}

int espacio_disponible(){
    
        int bloques_libres = 0;

        for(int i = 0; i < block_count ; i++){
            
            if (bitarray_test_bit(bit_map , i) == false){

                bloques_libres++;

            }

        }
        return bloques_libres * block_size;
}

int dar_bloque_libre(){
    for(int i = 0; i < block_count; i++ ){
        if (bitarray_test_bit(bit_map, i) == false)
        {
            bitarray_set_bit(bit_map, i);
            return i;
        }
        
    }
    return -1;
}

void hacer_compactacion(t_fcb* fcb_no_truncar){

    //creo el archivo auxiliar
    int tamanio_archivo_auxiliar = block_size * block_count;

    char* path = string_new();
    string_append(&path, path_base_dialfs);
    string_append(&path, "/auxiliar.dat"); 

    FILE* archivo_auxiliar = fopen(path, "w");
    fclose(archivo_auxiliar);

    int fd_auxiliar = open(path, O_RDWR, S_IRUSR | S_IWUSR);//Se supone q el archivo ya existe, sino da error

    if (fd_auxiliar == -1) {
        perror("Error al abrir o crear el archivo");
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_auxiliar, tamanio_archivo_auxiliar) == -1) {
        perror("Error al truncar el archivo");
        close(fd_bit_map);
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }

    void* fs_bloques_auxiliar = mmap(NULL, tamanio_archivo_auxiliar, PROT_READ | PROT_WRITE, MAP_SHARED, fd_auxiliar, 0);
    if (fs_bloques_auxiliar == MAP_FAILED) {
        perror("Error en mmap");
        close(fd_bit_map);
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }

    //copio el archivo original en el auxiliar de manera contigua
    int bloque_inicial, tamanio_archivo, cantidad_bloques;
    int bloques_recorridos = 0;
    
    for(int i=0; i < block_count; i++){//Empiezan en FALSE todos
            bitarray_clean_bit(bit_map,i);  //False = Libre(clean), True = Ocupado(setted)
    }

    for(int i = 0; i < list_size(lista_fcbs); i++){
        t_fcb* fcb_a_copiar = list_get(lista_fcbs, i);

        char* path_truncar = string_new();
        string_append(&path_truncar , path_base_dialfs);
        string_append(&path_truncar , "/");
        string_append(&path_truncar , fcb_a_copiar -> nombre); 
        
        t_config* config_truncar = config_create(path_truncar);

        bloque_inicial = fcb_a_copiar -> bloque_inicial;
        tamanio_archivo = fcb_a_copiar -> tamanio_archivo;
        cantidad_bloques = tamanio_archivo / block_size;
        int resto = tamanio_archivo % block_size;

        if(resto != 0)
            cantidad_bloques++;

        void* bloque_leido = malloc(tamanio_archivo);

        if (tamanio_archivo == 0){
               cantidad_bloques = 1;
        }

        if(fcb_a_copiar != fcb_no_truncar){//Esta verificacion es para que no se trunque el fcb que queremos agrandar, asi lo agregamos al final despues. 
            char* itoa_string = string_itoa(bloques_recorridos);
            config_set_value(config_truncar, "BLOQUE_INICIAL", itoa_string);
            config_save(config_truncar);

            fcb_a_copiar -> bloque_inicial = bloques_recorridos;
    
            for(int j = 0; bloques_recorridos < block_count && cantidad_bloques > 0; j++){
                
                leer_bloque(bloque_inicial + j, bloque_leido, block_size, 0);
                //bitarray_clean_bit(bit_map, bloque_inicial + j);

                memcpy(fs_bloques_auxiliar + (bloques_recorridos * block_size), bloque_leido, block_size);

                bloques_recorridos++;
                cantidad_bloques--;
            }

            if (fcb_a_copiar -> tamanio_archivo == 0){
               cantidad_bloques = 1;
            }else{
                cantidad_bloques = fcb_a_copiar -> tamanio_archivo / block_size;
                resto = tamanio_archivo % block_size;
            }

            if(resto != 0)
                cantidad_bloques++;

            for(int k = 0 ; k < cantidad_bloques; k++)
                bitarray_set_bit(bit_map, fcb_a_copiar -> bloque_inicial + k);
            
            sincronizar_bitmap();
            free(itoa_string);
        }
        free(path_truncar);
        free(bloque_leido);
        config_destroy(config_truncar);
        //free(path); <-- no funca
    }

    //Una vez copiado todo el archivo original de manera contigua en el archivo auxiliar, lo que tengo que hacer es copiar el auxiliar en el original
    memcpy(fs_bloques, fs_bloques_auxiliar, (block_count * block_size));
    sincronizar_archivo_bloques();

    if (munmap(fs_bloques_auxiliar, tamanio_archivo_auxiliar) == -1) {
        perror("Error en munmap");
        close(fd_auxiliar);
        terminar_ENTRADASALIDA();
        exit(EXIT_FAILURE);
    }
    close(fd_auxiliar);

    char* temporal = string_new();
    string_append(&temporal, path_base_dialfs);
    string_append(&temporal, "/auxiliar.dat");
    remove(temporal);
    free(temporal);
}

void sincronizar_archivo_bloques(){
    int tamanio_archivo_bloques = block_size * block_count;
    if (msync(fs_bloques, tamanio_archivo_bloques, MS_SYNC) == -1) {
        perror("Error en msync");
        munmap(fs_bloques, tamanio_archivo_bloques);
        close(fd_bloques);
        terminar_fs();
        exit(EXIT_FAILURE);
    }
}

bool comprobar_contiguos(int bloques_a_agregar, t_fcb* fcb){
    int bloque_inicial = fcb -> bloque_inicial;
    //int bloque_final = bloque_inicial + (fcb -> tamanio_archivo / block_size) - 1;
    for (int i = 0 ; i < bloques_a_agregar ; i++){ 
        if(bitarray_test_bit(bit_map, bloque_inicial + 1) == true){
            return false;
        }
        bloque_inicial++;
    }
    return true;
}

int recibir_io_gen_sleep(int kernel_fd)
{
    int * size = malloc(sizeof(int));
    recibir_operacion(kernel_fd);
    int * cantidad_unidades_trabajo = recibir_buffer(size, kernel_fd);

    free(size);
    
    return * cantidad_unidades_trabajo;
}

void terminar_fs(){
    for(int i = 0; i < list_size(lista_fcbs); i++){
        t_fcb* fcb = list_remove(lista_fcbs,i);
        free(fcb -> nombre);
        free(fcb);
    }
    munmap(fs_bloques,block_size * block_count);
    munmap(fs,block_count/8);
    terminar_ENTRADASALIDA();
}

void terminar_ENTRADASALIDA(){
    liberar_conexion(kernel_fd);
    liberar_conexion(memoria_fd);
    config_destroy(config);
    close(fd_bloques);
    close(fd_bit_map);
}

