#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <commons/collections/queue.h>
#include "main.h"



int main(int argc, char* argv[]) {

    logger = log_create("kernel.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    logger_obligatorio = log_create("kernel_obligatorio.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    config = iniciar_config();

    copiar_config();

    inicializar_variables();

    planificar();

    crear_conexiones();

    consola();

    conexiones();

    entradas_salidas(logger,diccionario_entradasalidas,"Kernel",kernel_fd); 

    terminar_kernel();
    return 0;
}

t_config* iniciar_config(){
    t_config* nuevo_config = config_create("kernel.config");

    if (nuevo_config == NULL) {
        log_error(logger, "Config no encontrado");
        config_destroy(nuevo_config);
        exit(EXIT_FAILURE);
    }
    return nuevo_config;
}

void copiar_config(){
    ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    ip_cpu = config_get_string_value(config,"IP_CPU");
    algoritmo_planificacion = config_get_string_value(config,"ALGORITMO_PLANIFICACION");
    recursos = config_get_array_value(config,"RECURSOS");
    instancias_recursos = config_get_array_value(config,"INSTANCIAS_RECURSOS");
    puerto_escucha = config_get_string_value(config,"PUERTO_ESCUCHA");
    puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    puerto_cpu_dispatch = config_get_string_value(config,"PUERTO_CPU_DISPATCH");
    puerto_cpu_interrupcion = config_get_string_value(config,"PUERTO_CPU_INTERRUPT");
    Quantum = config_get_int_value(config,"QUANTUM");
    grado_multiprogramacion = config_get_int_value(config,"GRADO_MULTIPROGRAMACION");
    path_scripts = config_get_string_value(config,"PATH_SCRIPTS");
}

void inicializar_variables(){

    planificacion_activa = true;

    lista_new = list_create();
    lista_ready = list_create();
    lista_exit = list_create();

    lista_aux_vrr = list_create();
    lista_generica = list_create();
    lista_stdin = list_create();
    lista_stdout = list_create();
    lista_io_filesystem = list_create();

    lista_script = list_create();

    diccionario_entradasalidas = dictionary_create();
    dictionary_put(diccionario_entradasalidas,"nombreGenerica","");
    dictionary_put(diccionario_entradasalidas,"nombreStdin","");
    dictionary_put(diccionario_entradasalidas,"nombreStdout","");
    dictionary_put(diccionario_entradasalidas,"nombreFilesystem","");
    
    contador_pid = 0;
    pid_a_finalizar = -1;

    pthread_mutex_init(&mutex_lista_ready, NULL);
    pthread_mutex_init(&mutex_lista_new, NULL);
    pthread_mutex_init(&mutex_lista_exit, NULL);
    pthread_mutex_init(&mutex_lista_aux_vrr, NULL);
    pthread_mutex_init(&mutex_contador_pid, NULL);
    pthread_mutex_init(&mutex_cola_generica, NULL);
    pthread_mutex_init(&mutex_cola_stdin, NULL);
    pthread_mutex_init(&mutex_cola_stdout, NULL);
    pthread_mutex_init(&mutex_cola_filesystem, NULL);
    pthread_mutex_init(&mutex_send_generica, NULL);
    pthread_mutex_init(&mutex_send_stdin, NULL);
    pthread_mutex_init(&mutex_send_stdout, NULL);
    pthread_mutex_init(&mutex_send_filesystem, NULL);
    pthread_mutex_init(&mutex_lista_recursos, NULL);
    pthread_mutex_init(&mutex_cola_fs, NULL);

    pthread_mutex_init(&mutex_sleep,NULL);

    sem_init(&sem_multiprogramacion, 0, grado_multiprogramacion);
    sem_init(&sem_nuevo_proceso, 0, 0);
    sem_init(&sem_pasar_procesador, 0, 1);
    sem_init(&sem_recibirCpu, 0, 1);
    sem_init(&sem_exit_pcb, 0, 0);
    sem_init(&sem_ready, 0, 0);
    sem_init(&sem_sleep, 0, 0);

    sem_init(&sem_fin_proceso, 0, 0);

    sem_init(&detener_planificador_corto_plazo, 0, 0);
    sem_init(&detener_planificador_new, 0, 0);
    sem_init(&detener_manejo_motivo, 0, 0);
    sem_init(&detener_ready_generica, 0, 0);
    sem_init(&detener_ready_stdin, 0, 0);
    sem_init(&detener_ready_stdout, 0, 0);
    sem_init(&detener_ready_recursos, 0, 0);
    sem_init(&detener_ready_filesystem, 0, 0);

    mutex_recursos = malloc(sizeof(pthread_mutex_t)* string_array_size(instancias_recursos));

    instancias_recursos_int = (int*)malloc(sizeof(int) * string_array_size(instancias_recursos));

    lista_recursos = list_create();

    for(int i = 0; i < string_array_size(instancias_recursos); i++){
        pthread_mutex_init(&mutex_recursos[i], NULL);
        instancias_recursos_int[i] = atoi(instancias_recursos[i]);
        struct t_nodo_recurso* paquete = malloc(sizeof(struct t_nodo_recurso));
        paquete -> recurso = recursos[i];
        paquete -> bloqueados_x_recursos = list_create();
        paquete -> pids_utilizando = (int*)malloc(sizeof(int)*string_array_size(instancias_recursos));
        list_add(lista_recursos, paquete);
    }

    grado_multiprogramacion_disminuido = 0;
    contador_generica = 0;
    contador_stdin = 0;
    contador_stdout = 0;
    contador_recursos = 0;
    contador_filesystem = 0;
}

void crear_conexiones(){
    kernel_fd = iniciar_socket(puerto_escucha,"Kernel");
    memoria_fd = crear_conexion(ip_memoria, puerto_memoria);//crea conexion con memoria. memoria_fd es el socket de memoria 
    cpu_dispatch_fd = crear_conexion(ip_cpu, puerto_cpu_dispatch);//crea conexion con kernel. conexionKernel es el socket de kernel
    cpu_interrupt_fd = crear_conexion(ip_cpu, puerto_cpu_interrupcion);//crea conexion con kernel. conexionKernel es el socket de kernel
}

//FUNCIONES DE CONSOLA

void consola(){

    pthread_t hconsola;

    pthread_create(&hconsola, NULL, (void*)procesar_consola, NULL);
    pthread_detach(hconsola);
}

static void procesar_consola() {
    bool script_activo = false;
    struct t_consola operacion;
    char* cod_op_string = NULL;

    while(1){
        
        if(script_activo){
            cod_op_string = cod_op_script(&script_activo);
            operacion = obtener_codop_consola(cod_op_string);
            log_info(logger_obligatorio," EJECUTANDO SCRIPT: %s",cod_op_string);
        }
        else{
            cod_op_string = readline("> ");
            operacion = obtener_codop_consola(cod_op_string);
        }

        switch(operacion.codigo_operacion){
            case EJECUTAR_SCRIPT: 
                char* path = string_duplicate(path_scripts); 
                string_append(&path,operacion.parametro);
                guardar_script(path,&script_activo);
                free(cod_op_string);
                free(operacion.parametro);
                free(path);
                break;

            case INICIAR_PROCESO:
            
                iniciar_proceso(operacion.parametro);
                free(operacion.parametro);
                free(cod_op_string);
                break;

            case FINALIZAR_PROCESO:
                
                finalizar_proceso(atoi(operacion.parametro));
                free(operacion.parametro);
                free(cod_op_string);
                break;
            
            case DETENER_PLANIFICACION:
                
                detener_planificacion();
                free(cod_op_string);
                break;

            case INICIAR_PLANIFICACION:
                
                iniciar_planificacion();
                free(cod_op_string);
                break;

            case MULTIPROGRAMACION:
                
                modificar_grado_multiprogramacion(atoi(operacion.parametro));
                free(operacion.parametro);
                free(cod_op_string);
                break;

            case PROCESO_ESTADO:

                mostrar_proceso_estado();
                free(cod_op_string);
                break;

            case -1:
                free(cod_op_string);
                terminar_kernel();
                exit(EXIT_FAILURE);
            case -2:
                log_warning(logger,"Bool_sleep: %d, no_presente: %d",bool_sleep,no_presente);
                break;
            default:
                    log_warning(logger, "Comando desconocido -> '%s'",cod_op_string);
                    log_warning(logger,"introduzca un comando valido...");
                    free(cod_op_string);
                    break;
        }
    }
}

void finalizar_proceso(int pid_a_eliminar){
    detener_planificacion();
    

    if(!buscar_pid(pid_a_eliminar)){
        if(exec_global != NULL && exec_global -> contexto -> pid != pid_a_eliminar)
            log_warning(logger,"Proceso: <%d> ya no esta en el sistema",pid_a_eliminar);
        else{
            finalizar_un_proceso = true;
            int int_interrupcion = FIN_QUANTUM;
            send(cpu_interrupt_fd,&int_interrupcion,sizeof(int),0);
            sem_wait(&sem_fin_proceso);
        }
    }
    iniciar_planificacion();
}

bool buscar_pid(int pid){
    t_pcb* pcb;
    for(int i = 0; i < list_size(lista_new);i++){ //NEW
        pcb = list_get(lista_new,i);
        if(pcb -> contexto -> pid == pid){
            sem_wait(&sem_nuevo_proceso);
            pthread_mutex_lock(&mutex_lista_new);
            pcb = list_remove(lista_new,i);
            pthread_mutex_unlock(&mutex_lista_new);

            pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;
            
            pthread_mutex_lock(&mutex_lista_exit);
            list_add(lista_exit,pcb);
            pthread_mutex_unlock(&mutex_lista_exit);
            sem_post(&sem_exit_pcb);
            
            return true;
        }
    }
    for(int i = 0; i < list_size(lista_ready);i++){ //READY
        pcb = list_get(lista_ready,i);
        if(pcb -> contexto -> pid == pid){
            sem_wait(&sem_ready);
            pthread_mutex_lock(&mutex_lista_ready);
            pcb = list_remove(lista_ready,i);
            pthread_mutex_unlock(&mutex_lista_ready);

            pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;

            pthread_mutex_lock(&mutex_lista_exit);
            list_add(lista_exit,pcb);
            pthread_mutex_unlock(&mutex_lista_exit);
            sem_post(&sem_exit_pcb);
            return true;
        }
    }
    for(int i = 0; i < list_size(lista_aux_vrr);i++){ //READY
        pcb = list_get(lista_aux_vrr,i);
        if(pcb -> contexto -> pid == pid){
            sem_wait(&sem_ready);
            pthread_mutex_lock(&mutex_lista_aux_vrr);
            pcb = list_remove(lista_aux_vrr,i);
            pthread_mutex_unlock(&mutex_lista_aux_vrr);

            pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;

            pthread_mutex_lock(&mutex_lista_exit);
            list_add(lista_exit,pcb);
            pthread_mutex_unlock(&mutex_lista_exit);
            sem_post(&sem_exit_pcb);
            return true;
        }
    }
    for(int i = 0; i < list_size(lista_generica);i++){//BLOQUEADOS
        pcb = list_get(lista_generica,i);
        if(pcb -> contexto -> pid == pid){
            pid_a_finalizar = pid;

            pthread_mutex_lock(&mutex_cola_generica);
            pcb = list_remove(lista_generica,i);
            pthread_mutex_unlock(&mutex_cola_generica);

            pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;

            pthread_mutex_lock(&mutex_lista_exit);
            list_add(lista_exit,pcb);
            pthread_mutex_unlock(&mutex_lista_exit);
            sem_post(&sem_exit_pcb);
            return true;
        }
    }
    for(int i = 0; i < list_size(lista_stdin);i++){ //BLOQUEADOS
        pcb = list_get(lista_stdin,i);
        if(pcb -> contexto -> pid == pid){
            pid_a_finalizar = pid;

            pthread_mutex_lock(&mutex_cola_stdin);
            pcb = list_remove(lista_stdin,i);
            pthread_mutex_unlock(&mutex_cola_stdin);

            pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;

            pthread_mutex_lock(&mutex_lista_exit);
            list_add(lista_exit,pcb);
            pthread_mutex_unlock(&mutex_lista_exit);
            sem_post(&sem_exit_pcb);
            return true;
        }
    }
    for(int i = 0; i < list_size(lista_stdout);i++){ //BLOQUEADOS
        pcb = list_get(lista_stdout,i);
        if(pcb -> contexto -> pid == pid){
            pid_a_finalizar = pid;

            pthread_mutex_lock(&mutex_cola_stdout);
            pcb = list_remove(lista_stdout,i);
            pthread_mutex_unlock(&mutex_cola_stdout);

            pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;

            pthread_mutex_lock(&mutex_lista_exit);
            list_add(lista_exit,pcb);
            pthread_mutex_unlock(&mutex_lista_exit);
            sem_post(&sem_exit_pcb);
            return true;
        }
    }
    for(int i = 0; i < list_size(lista_io_filesystem);i++){ //BLOQUEADOS
        pcb = list_get(lista_io_filesystem,i);
        if(pcb -> contexto -> pid == pid){
            pid_a_finalizar = pid;
            pthread_mutex_lock(&mutex_cola_fs);
            pcb = list_remove(lista_io_filesystem,i);
            pthread_mutex_unlock(&mutex_cola_fs);

            pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;

            pthread_mutex_lock(&mutex_lista_exit);
            list_add(lista_exit,pcb);
            pthread_mutex_unlock(&mutex_lista_exit);
            sem_post(&sem_exit_pcb);
            return true;
        }
    }
    struct t_nodo_recurso* paquete;
    for(int i = 0; i < list_size(lista_recursos);i++){
        paquete = list_get(lista_recursos,i);

        for(int j = 0; j < list_size(paquete -> bloqueados_x_recursos);j++){
            pcb = list_get(paquete -> bloqueados_x_recursos, j);
            if(pcb -> contexto -> pid == pid){
                pid_a_finalizar = pid;
                
                pthread_mutex_lock(&mutex_lista_recursos);
                pcb = list_remove(paquete -> bloqueados_x_recursos,j);
                pthread_mutex_unlock(&mutex_lista_recursos);

                pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;

                pthread_mutex_lock(&mutex_lista_exit);
                list_add(lista_exit,pcb);
                pthread_mutex_unlock(&mutex_lista_exit);
                sem_post(&sem_exit_pcb);
                return true;
            }
        }
    }
    return false;
}

void iniciar_proceso(char* path){
    t_pcb *pcb = crear_pcb(path);

    if(pcb != NULL){
        pthread_mutex_lock(&mutex_lista_new);
        list_add(lista_new, pcb);
        pthread_mutex_unlock(&mutex_lista_new);
        sem_post(&sem_nuevo_proceso); //HABILITA FUNCION READY CONSOLA
    }
}

void guardar_script(char* path, bool* script_activo){
    FILE* archivo = fopen(path,"r");
    char caracter;
    char* linea = malloc(70);
    int index = 0;
    if(archivo){
        *script_activo = true;
        while((caracter = fgetc(archivo)) != EOF)
            if(caracter != '\n' ){
                linea[index] = caracter;
                index++;
            }
            else{
                linea[index] = '\0';
                if(strlen(linea) > 2){
                    
                    index = 0;
                    linea = realloc(linea, strlen(linea) + 1);
                    list_add(lista_script,linea);
                    linea = malloc(70);
                }else free(linea);
            }
        linea[index] = '\0';   
        if(strlen(linea) > 2){
            
            index = 0;
            linea = realloc(linea, strlen(linea) + 1);
            list_add(lista_script,linea);
        }
    }
    else log_error(logger,"Script no encontrado");
}

void detener_planificacion(){
    if(planificacion_activa){
        planificacion_activa = false;
    }
    else
        log_warning(logger, "La planificacion ya esta detenida");
}

void iniciar_planificacion(){
    if(!planificacion_activa){
        planificacion_activa = true;

        sem_post(&detener_planificador_corto_plazo);
        sem_post(&detener_planificador_new);
        sem_post(&detener_manejo_motivo);

        for(int i = 0; i < contador_generica; i++){
            sem_post(&detener_ready_generica);
        }

        contador_generica = 0;

        for(int i = 0; i < contador_stdin; i++){
            sem_post(&detener_ready_stdin);
        }
            
        contador_stdin = 0;

        for(int i = 0; i < contador_stdout; i++){
            sem_post(&detener_ready_stdout);
        }

        contador_stdout = 0;

        for(int i = 0; i < contador_recursos; i++){
            sem_post(&detener_ready_recursos);
        }

        contador_recursos = 0;

    }
    else
        log_warning(logger, "La planificacion ya esta esta activa");
}

void mostrar_proceso_estado(){ 

    if(exec_global != NULL)
        log_info(logger_obligatorio,"%d esta en EJECUTANDO", exec_global -> contexto -> pid);

    log_info(logger_obligatorio, "Entrando a cola NEW");

    list_iterate(lista_new, print_new);
    
    log_info(logger_obligatorio, "Entrando a cola READY");

    list_iterate(lista_ready, print_ready);

    log_info(logger_obligatorio, "Entrando a cola BLOQUEADOS GENERICA");

    list_iterate(lista_generica, print_bloq_gen);

    log_info(logger_obligatorio, "Entrando a cola BLOQUEADOS STDOUT");

    list_iterate(lista_stdout, print_bloq_stdout);

    log_info(logger_obligatorio, "Entrando a cola BLOQUEADOS STDIN");

    list_iterate(lista_stdin, print_bloq_stdin);

    log_info(logger_obligatorio, "Entrando a cola BLOQUEADOS WAIT");

    for(int i = 0; i < string_array_size(instancias_recursos); i++){
        struct t_nodo_recurso* paquete = list_get(lista_recursos,i);
        list_iterate(paquete -> bloqueados_x_recursos, print_bloq_recursos);
    }
}

struct t_consola obtener_codop_consola(char* codop){

    char** array_codop = string_split(codop, " ");
    struct t_consola nueva_operacion; 

    for(int i = 0; i < string_array_size(cod_op_array); i++){

        if(strcmp(array_codop[0], cod_op_array[i]) == 0){
            
            nueva_operacion.codigo_operacion = i;
            if(array_codop[1] != NULL)
                nueva_operacion.parametro = string_duplicate(array_codop[1]);
            string_array_destroy(array_codop);
            return nueva_operacion;
        }
    }
    if(strcmp(array_codop[0],"-1") == 0){
            nueva_operacion.codigo_operacion = -1;
            nueva_operacion.parametro = "NULL";
            string_array_destroy(array_codop);
            return nueva_operacion;
    }else if(strcmp(array_codop[0],"-2") == 0){
            nueva_operacion.codigo_operacion = -2;
            nueva_operacion.parametro = "NULL";
            string_array_destroy(array_codop);
            return nueva_operacion;
    }
}

char* cod_op_script(bool* script_activo){
    char* linea = list_remove(lista_script,0);
    if(list_size(lista_script) == 0){
        *script_activo = false;
    }
    return linea;
}

void modificar_grado_multiprogramacion(int grado_nuevo){

    if((grado_nuevo - grado_multiprogramacion) > 0){//significa que el grado de multiprogramacion nuevo es mayor
        int grado_a_aumentar = grado_nuevo - grado_multiprogramacion;
        grado_multiprogramacion = grado_nuevo;
        grado_multiprogramacion_disminuido = 0;

        for(int i = 0; i < grado_a_aumentar; i++)
            sem_post(&sem_multiprogramacion);
    }else {
        grado_multiprogramacion_disminuido = abs(grado_nuevo - grado_multiprogramacion);
        grado_multiprogramacion = grado_nuevo;
    }
}

// FUNCIONES PCB

void registros(t_contexto* contexto){
    
}

struct t_pcb* crear_pcb(char* path){

    t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));

    send_instruccion(path,memoria_fd,CREAR_PCB);
    char* consulta = recv_notificacion(memoria_fd);
    consulta[2] = '\0';
    if(strcmp(consulta,"OK") == 0){

        nuevo_pcb -> quantum = Quantum;
        nuevo_pcb ->contexto = malloc(sizeof(t_contexto));
        nuevo_pcb ->contexto -> pid = contador_pid;

        pthread_mutex_lock(&mutex_contador_pid);
        contador_pid++;
        pthread_mutex_unlock(&mutex_contador_pid);

        nuevo_pcb ->contexto -> estado = NEW;
        nuevo_pcb ->contexto -> motivo_exit = NA;
        nuevo_pcb ->contexto -> motivo_bloqueo = NO_BLOQUEADO;

        nuevo_pcb -> contexto -> Registros_CPU = malloc(sizeof(t_registros_cpu));

        nuevo_pcb -> contexto -> Registros_CPU -> pc = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> ax = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> bx = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> cx = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> dx = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> eax = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> ebx = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> ecx = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> edx = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> si = 0;
        nuevo_pcb -> contexto -> Registros_CPU -> di = 0;

        send_pcb(nuevo_pcb, memoria_fd, CREAR_PCB);
        liberar_pcb(nuevo_pcb);
        nuevo_pcb = recv_pcb(memoria_fd);
    

        log_info(logger_obligatorio, "Se crea el proceso <%d> en NEW", nuevo_pcb -> contexto -> pid);
        free(consulta);
        return nuevo_pcb;
    }
    else {
        free(nuevo_pcb);
        free(consulta);
        log_error(logger,"Archivo no encontrado o invalido");
        return NULL;
    }
}

//FUNCIONES PLANIFICACION

void planificar(){

    planificar_largo_plazo();

    pthread_t hcortoPlazo;
    pthread_create(&hcortoPlazo, NULL, (void*)planificar_corto_plazo, NULL);
    pthread_detach(hcortoPlazo);  
} 

void planificar_largo_plazo(){
    pthread_t hready;
    pthread_t hexit;

    pthread_create(&hready, NULL, (void*)FuncionReadyConsola, NULL);
    pthread_create(&hexit,NULL, (void*)FuncionExit, NULL);
    pthread_detach(hready);
    pthread_detach(hexit); 

}

void FuncionReadyConsola(){

    while(1){
        sem_wait(&sem_nuevo_proceso);
        sem_wait(&sem_multiprogramacion);

        if(!planificacion_activa){
            sem_wait(&detener_planificador_new);
        }

        pthread_mutex_lock(&mutex_lista_ready);
        t_pcb* pcb = list_remove(lista_new, 0);
        list_add(lista_ready, pcb);
        logear_ready(true);
        pthread_mutex_unlock(&mutex_lista_ready);

        

        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[1]);
        pcb ->contexto -> estado = READY;

        sem_post(&sem_ready);

    }
}

void FuncionExit(){
    while(1){
        sem_wait(&sem_exit_pcb);
        
        pthread_mutex_lock(&mutex_lista_exit);
        t_pcb* pcb = list_remove(lista_exit,0);
        pthread_mutex_unlock(&mutex_lista_exit);

        liberar_recursos(pcb);

        send_limpiar_memoria(pcb->contexto -> pid,memoria_fd);

        if(grado_multiprogramacion_disminuido == 0){//si es igual a 0 significa que no se disminuyo el grado de multiprogramacion. 
            if(pcb -> contexto -> estado != NEW)
                sem_post(&sem_multiprogramacion);
        }else//si grado_multiprogramacion_disminuido es != de 0, es porque se tiene que disminuir el grado de multiprogramacion, entonces no hay q hacer los signals.
            grado_multiprogramacion_disminuido--;
        
        log_info(logger_obligatorio, "Finaliza el proceso <%d> - Motivo: <%s>",pcb->contexto->pid, array_motivo_exit[pcb -> contexto -> motivo_exit]);
        
        liberar_pcb(pcb);
    }
}

void liberar_recursos(t_pcb* pcb){
    t_list_iterator* iterador_recursos_grande = list_iterator_create(lista_recursos);
    struct t_nodo_recurso* recurso;
    

    for(int index = 0; index < string_array_size(instancias_recursos); index++){ // iterar hasta llegar al index == size del array
        recurso = list_iterator_next(iterador_recursos_grande);

        for(int i = 0; i < atoi(instancias_recursos[index]) - instancias_recursos_int[index] ; i++) // Itera segun cuantas instancias de recursos fueron solicitadas
            if(recurso -> pids_utilizando[i] == pcb -> contexto -> pid){
                if(instancias_recursos_int[index] < 0 && list_size(recurso -> bloqueados_x_recursos) > 0)
                    desbloquear_segun_recurso(recursos[index],index);
                    
                pthread_mutex_lock(&mutex_recursos[index]);
                instancias_recursos_int[index]++;
                pthread_mutex_unlock(&mutex_recursos[index]);
            }
    }
    list_iterator_destroy(iterador_recursos_grande);
}

void planificar_corto_plazo(){
    
    int algoritmo = elegir_algoritmo(algoritmo_planificacion), int_interrupcion;
    
    while(1){
        sem_wait(&sem_pasar_procesador);
        sem_wait(&sem_ready);

        if(!planificacion_activa){
            sem_wait(&detener_planificador_corto_plazo);
        }

        
        switch (algoritmo){
            case FIFO:
                pthread_mutex_lock(&mutex_lista_ready);
                exec_global = list_remove(lista_ready, 0); 
                pthread_mutex_unlock(&mutex_lista_ready);

                log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", exec_global -> contexto -> pid, array_estados[exec_global -> contexto -> estado],array_estados[3]);
                exec_global ->contexto -> estado = EXEC;
                
                send_pcb(exec_global, cpu_dispatch_fd, EXEC_PCB);
                
                break;
                
            case RR:

                pthread_mutex_lock(&mutex_lista_ready);
                exec_global = list_remove(lista_ready, 0);
                pthread_mutex_unlock(&mutex_lista_ready);

                if(exec_global -> contexto -> estado != EXEC){
                    log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", exec_global -> contexto -> pid, array_estados[exec_global -> contexto -> estado],array_estados[3]);
                    exec_global ->contexto -> estado = EXEC;
                }

                send_pcb(exec_global, cpu_dispatch_fd, EXEC_PCB);

                pthread_mutex_lock(&mutex_sleep);
                bool_sleep = false;
                no_presente = false;
                pthread_mutex_unlock(&mutex_sleep);

                pthread_t h_usleep1;
                pthread_create(&h_usleep1,NULL, (void*)funcion_usleep, &(Quantum));
                pthread_detach(h_usleep1);

                sem_wait(&sem_sleep);

                if(exec_global != NULL && bool_sleep){
                    int_interrupcion = FIN_QUANTUM;
                    send(cpu_interrupt_fd,&int_interrupcion,sizeof(int),0);
                }else pthread_cancel(h_usleep1);

                break;
        
            case VRR:
                
                exec_global = verificar_lista_aux();

                if(exec_global -> contexto -> estado != EXEC){
                    log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", exec_global -> contexto -> pid, array_estados[exec_global -> contexto -> estado],array_estados[3]);
                    exec_global ->contexto -> estado = EXEC;
                }

                send_pcb(exec_global, cpu_dispatch_fd, EXEC_PCB);

                pthread_mutex_lock(&mutex_sleep);
                bool_sleep = false;
                no_presente = false;
                pthread_mutex_unlock(&mutex_sleep);

                pthread_t h_usleep2;
                pthread_create(&h_usleep2,NULL, (void*)funcion_usleep, &(exec_global -> quantum));
                pthread_detach(h_usleep2);

                sem_wait(&sem_sleep);

                if(exec_global != NULL && bool_sleep){
                    int_interrupcion = FIN_QUANTUM;
                    send(cpu_interrupt_fd,&int_interrupcion,sizeof(int),0);
                }else pthread_cancel(h_usleep2);
        
                break;
            
            default:
                log_error(logger,"Error en el config (ALGORITMO)");
                terminar_kernel();
                exit(EXIT_FAILURE);
                break;

        }
    }
}

void funcion_usleep(void* args){

    int cantidad = *(int*)args;
    usleep(cantidad * 1000);

    //no_presente se fija si el proceso esta fuera del procesador o bloqueado por algun motivo
    // Entonces, el usleep queda obsoleto y no hay q darle bola
    pthread_testcancel();

    pthread_mutex_lock(&mutex_sleep);
    if(no_presente == false){
        bool_sleep = true;
        sem_post(&sem_sleep);
    }
    pthread_mutex_unlock(&mutex_sleep);
}

int elegir_algoritmo(char* algoritmo_config){

    for(register int i = 0; i < 3; i++){

        if(strcmp(algoritmo_config, algoritmo_array[i]) == 0){
            return i;
        }
    }
    return -1;
}

t_pcb* verificar_lista_aux(){
    t_pcb* pcb;
    if(!list_is_empty(lista_aux_vrr)){

        pthread_mutex_lock(&mutex_lista_aux_vrr);
        pcb = list_remove(lista_aux_vrr, 0);
        pthread_mutex_unlock(&mutex_lista_aux_vrr);
    }
    else{

        pthread_mutex_lock(&mutex_lista_ready);
        pcb = list_remove(lista_ready, 0);
        pthread_mutex_unlock(&mutex_lista_ready);
        pcb -> quantum = Quantum;
    }

    return pcb;
}

//FUNCIONES DE MANEJO DE CONEXIONES

void procesar_conexiones(){

    int cod_op;

    while(1){
        
        sem_wait(&sem_recibirCpu);
        cod_op = recibir_operacion(cpu_dispatch_fd);
        t_pcb* pcb = recv_pcb_sin_op(cpu_dispatch_fd);

        if(finalizar_un_proceso){
            pcb -> contexto -> motivo_exit = INTERRUPTED_BY_USER;
            cod_op = EXIT_PCB;
            finalizar_un_proceso = false;
            sem_post(&sem_fin_proceso);
        }

        if(exec_global != NULL && exec_global -> contexto -> pid == pcb -> contexto -> pid){
            liberar_pcb(exec_global);
            exec_global = NULL;
        }

        if(!planificacion_activa)
            sem_wait(&detener_manejo_motivo);
        
        switch(cod_op){
            case MANEJAR_IO_GEN_SLEEP: 
                pthread_t hilo_sleep;
                pthread_create(&hilo_sleep, NULL, (void*)manejar_io_gen_sleep, pcb);
                pthread_detach(hilo_sleep);
                
                actualizar_sleep();
                break; 

            case MANEJAR_IO_STDIN_READ:
                pthread_t hilo_stdin;
                pthread_create(&hilo_stdin, NULL, (void*)manejar_io_stdin, pcb);
                pthread_detach(hilo_stdin);

                actualizar_sleep();
                break;

            case MANEJAR_IO_STDOUT_WRITE:
                pthread_t hilo_stdout;
                pthread_create(&hilo_stdout, NULL, (void*)manejar_io_stdout, pcb);
                pthread_detach(hilo_stdout);

                actualizar_sleep();
                break;

            case MANEJAR_WAIT: 
                pthread_t hilo_wait;
                pthread_create(&hilo_wait, NULL, (void*)manejar_wait, pcb);
                pthread_detach(hilo_wait);

                break;

            case MANEJAR_SIGNAL:
                pthread_t hilo_signal;
                pthread_create(&hilo_signal, NULL, (void*)manejar_signal, pcb);
                pthread_detach(hilo_signal);

                break;

            case MANEJAR_FS:
                pthread_t hilo_fs;
                pthread_create(&hilo_fs, NULL, (void*)manejar_fs , pcb);
                pthread_detach(hilo_fs);

                actualizar_sleep();
                break;

            case EXIT_PCB:
                pthread_mutex_lock(&mutex_lista_exit);
                list_add(lista_exit,pcb);
                pthread_mutex_unlock(&mutex_lista_exit);

                actualizar_sleep();
                
                sem_post(&sem_recibirCpu);
                sem_post(&sem_pasar_procesador);
                sem_post(&sem_exit_pcb);

                break;

            case FIN_QUANTUM:
                pthread_t hiloQuantum;
                pthread_create(&hiloQuantum, NULL, (void*)manejar_fin_quantum, pcb);
                pthread_detach(hiloQuantum);

                break;
            
            default:
                log_error(logger, "CPU DESCONECTADO");
                terminar_kernel();
                exit(EXIT_FAILURE);
        }
    }
}

void actualizar_sleep(){
    pthread_mutex_lock(&mutex_sleep);
    if(bool_sleep == false){
        no_presente = true;
        sem_post(&sem_sleep);
    }
    pthread_mutex_unlock(&mutex_sleep);
}

void manejar_io_stdin(void* void_args){
    t_pcb* args = (t_pcb*) void_args;
    t_pcb* pcb = args;
    int cant_accesos, socket;

    t_list* lista_df = list_create();
    t_list* lista_tam = list_create();

    recibir_operacion(cpu_dispatch_fd);
    char* nombre_io = recv_paquete_df(&cant_accesos, lista_df, lista_tam, cpu_dispatch_fd);

    sem_post(&sem_recibirCpu);
    sem_post(&sem_pasar_procesador);
    
    if(verificar_io(nombre_io, &socket)){
        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[2]);
        log_info(logger,"PID: <%d> - Bloqueado por: <TECLADO>",pcb ->contexto ->pid);
        pcb -> contexto -> estado = BLOCKED;

        pthread_mutex_lock(&mutex_cola_stdin);
        list_add(lista_stdin, pcb);
        pthread_mutex_unlock(&mutex_cola_stdin);

        pthread_mutex_lock(&mutex_send_stdin);
        if(!planificacion_activa){//aca nose si habria q usar un semaforo?
            contador_stdin++;
            sem_wait(&detener_ready_stdin);
        }
        if(pid_a_finalizar == pcb ->contexto -> pid){
            list_destroy(lista_df);
            list_destroy(lista_tam);
            return;
        }

        send_int(pcb -> contexto -> pid, socket, 0);
        send_paquete_df(lista_df, lista_tam, cant_accesos, "STDIN", STDIN, socket);
        free(recv_notificacion(socket));
        pthread_mutex_unlock(&mutex_send_stdin);

        if(pid_a_finalizar == pcb ->contexto -> pid){
            list_destroy(lista_df);
            list_destroy(lista_tam);
            return;
        }

        //aca nose si iria otra validacion como la de arriba. Porque el primero que estaba haciendo la io y desp se freno la planificacion, va a seguir de largo. 
        pthread_mutex_lock(&mutex_cola_stdin);
        pcb = list_remove(lista_stdin, 0);
        pthread_mutex_unlock(&mutex_cola_stdin);

        guardar_segun_algoritmo(pcb);
        
    }else{
        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[4]);
        pcb -> contexto -> estado = EXIT_PROCESO;
        pthread_mutex_lock(&mutex_lista_exit);
        list_add(lista_exit,pcb);
        pthread_mutex_unlock(&mutex_lista_exit);
        sem_post(&sem_exit_pcb);
    }
    free(nombre_io);
    list_destroy(lista_df);
    list_destroy(lista_tam);
    pthread_exit(NULL);
}

void manejar_io_stdout(void* void_args){
    t_pcb* args = (t_pcb*) void_args;
    t_pcb* pcb = args;
    int  socket, cant_accesos;

    t_list* lista_df = list_create();
    t_list* lista_tam = list_create();

    recibir_operacion(cpu_dispatch_fd);
    char* nombre_io = recv_paquete_df(&cant_accesos, lista_df, lista_tam, cpu_dispatch_fd);

    sem_post(&sem_recibirCpu);
    sem_post(&sem_pasar_procesador);
    
    if(verificar_io(nombre_io, &socket)){
        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[2]);
        log_info(logger,"PID: <%d> - Bloqueado por: <MONITOR>",pcb ->contexto ->pid);
        pcb ->contexto -> estado = BLOCKED;

        pthread_mutex_lock(&mutex_cola_stdout);
        list_add(lista_stdout, pcb);
        pthread_mutex_unlock(&mutex_cola_stdout);

        pthread_mutex_lock(&mutex_send_stdout);
        if(!planificacion_activa){
            contador_stdout++;
            sem_wait(&detener_ready_stdout);
        }
        if(pid_a_finalizar == pcb ->contexto -> pid){
            list_destroy(lista_df);
            list_destroy(lista_tam);
            return;
        }

        send_int(pcb -> contexto -> pid, socket, 0);
        send_paquete_df(lista_df, lista_tam, cant_accesos, "STDIN", STDOUT, socket);
        free(recv_notificacion(socket));
        pthread_mutex_unlock(&mutex_send_stdout);

        if(pid_a_finalizar == pcb ->contexto -> pid){
            list_destroy(lista_df);
            list_destroy(lista_tam);
            return;
        }
        //aca nose si iria otra validacion como la de arriba. Porque el primero que estaba haciendo la io y desp se freno la planificacion, va a seguir de largo. 

        pthread_mutex_lock(&mutex_cola_stdout);
        pcb = list_remove(lista_stdout, 0);
        pthread_mutex_unlock(&mutex_cola_stdout);

        guardar_segun_algoritmo(pcb);
        
    }else{
        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[4]);
        pcb -> contexto -> estado = EXIT_PROCESO;

        pthread_mutex_lock(&mutex_lista_exit);
        list_add(lista_exit,pcb);
        pthread_mutex_unlock(&mutex_lista_exit);
        sem_post(&sem_exit_pcb);
    }
    free(nombre_io);
    list_destroy(lista_tam);
    list_destroy(lista_df);
    pthread_exit(NULL);
}

void manejar_io_gen_sleep(void* void_args){

    int cantidad_unidades_trabajo;
    t_pcb* pcb = (t_pcb*)void_args;
    int socket;
    
    recibir_operacion(cpu_dispatch_fd);
    char* nombre_io = recv_ins_io_gen_sleep(&cantidad_unidades_trabajo, cpu_dispatch_fd);

    sem_post(&sem_pasar_procesador);
    sem_post(&sem_recibirCpu);

    if(verificar_io(nombre_io, &socket)){
        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[2]);
        log_info(logger,"PID: <%d> - Bloqueado por: <%s>",pcb ->contexto ->pid,nombre_io);
        pcb ->contexto -> estado = BLOCKED;

        

        pthread_mutex_lock(&mutex_cola_generica);
        list_add(lista_generica, pcb);
        pthread_mutex_unlock(&mutex_cola_generica);

        pthread_mutex_lock(&mutex_send_generica);
        if(!planificacion_activa){
            contador_generica++;
            sem_wait(&detener_ready_generica);
        }
        if(pid_a_finalizar == pcb ->contexto -> pid)
            return;
        
        send_io_gen_sleep(socket, cantidad_unidades_trabajo, MANEJAR_IO_GEN_SLEEP, pcb -> contexto -> pid);
        free(recv_notificacion(socket));
        pthread_mutex_unlock(&mutex_send_generica);
    
        if(pid_a_finalizar == pcb ->contexto -> pid)
            return;
        
        //aca nose si iria otra validacion como la de arriba. Porque el primero que estaba haciendo la io y desp se freno la planificacion, va a seguir de largo. 

        pthread_mutex_lock(&mutex_cola_generica);
        pcb = list_remove(lista_generica, 0);
        pthread_mutex_unlock(&mutex_cola_generica);

        guardar_segun_algoritmo(pcb);

    }else{
        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[4]);
        pcb -> contexto -> estado = EXIT_PROCESO;

        pthread_mutex_lock(&mutex_lista_exit);
        list_add(lista_exit,pcb);
        pthread_mutex_unlock(&mutex_lista_exit);
        sem_post(&sem_exit_pcb);
    }
    free(nombre_io);
    pthread_exit(NULL);
}

void manejar_wait(void* void_args){
    struct t_nodo_recurso* paquete;
    t_pcb* pcb = (t_pcb*) void_args;
    char* recurso = recv_notificacion(cpu_dispatch_fd);

    int cant_recursos = string_array_size(recursos);

    for(int i = 0; i < cant_recursos ; i++){
        if(strcmp(recursos[i], recurso) == 0){

            if(instancias_recursos_int[i] > 0){//Caso en que consumir el recurso no bloquea al proceso.
                
                paquete = list_get(lista_recursos, i);
                paquete -> pids_utilizando[atoi(instancias_recursos[i]) - instancias_recursos_int[i]] = pcb -> contexto -> pid;
                
                //le asigno el recurso al proceso y le resto uno
                pthread_mutex_lock(&mutex_recursos[i]);
                instancias_recursos_int[i]--;
                pthread_mutex_unlock(&mutex_recursos[i]);

                send_notificacion("NO BLOQUEANTE",cpu_dispatch_fd,0);
                sem_post(&sem_recibirCpu);
                liberar_pcb(pcb);
                free(recurso);
                return;

            }else{//Caso en que consumir en que no hay recurso y se bloquea el proceso

                paquete = list_get(lista_recursos, i);
                paquete -> pids_utilizando[atoi(instancias_recursos[i]) - instancias_recursos_int[i]] = pcb -> contexto -> pid;
                
                pthread_mutex_lock(&mutex_recursos[i]);
                instancias_recursos_int[i]--;
                pthread_mutex_unlock(&mutex_recursos[i]);

                bloquear_segun_recurso(pcb, recurso, i);
                log_info(logger,"PID: <%d> - Bloqueado por: <%s>",pcb ->contexto ->pid,recurso);

                send_notificacion("BLOQUEANTE",cpu_dispatch_fd,0);

                sem_post(&sem_recibirCpu);
                sem_post(&sem_pasar_procesador);
                free(recurso);
                return;
            }

            break;
        }
    }

    log_error(logger, "Recurso no existe \"%s\"", recurso);

    free(recurso);

    pthread_mutex_lock(&mutex_lista_exit);
    list_add(lista_exit, pcb);
    pthread_mutex_unlock(&mutex_lista_exit);
    sem_post(&sem_exit_pcb);
    sem_post(&sem_recibirCpu);
    sem_post(&sem_pasar_procesador);
    pthread_exit(NULL);
}

void bloquear_segun_recurso(t_pcb* pcb, char* recurso, int cardinal){

    struct t_nodo_recurso* paquete = list_get(lista_recursos, cardinal);

    log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[2]);
    pcb -> contexto -> estado = BLOCKED;
    pcb -> contexto -> motivo_bloqueo = RECURSO;

    pthread_mutex_lock(&mutex_lista_recursos);
    list_add(paquete -> bloqueados_x_recursos, pcb);
    pthread_mutex_unlock(&mutex_lista_recursos);
}

void manejar_signal(void* void_args){

    struct t_nodo_recurso* paquete;
    t_pcb* pcb = (t_pcb*) void_args; 
    char* recurso = recv_notificacion(cpu_dispatch_fd);

    int cant_recursos = string_array_size(recursos);

    for(int i = 0; i < cant_recursos ; i++){
        if(strcmp(recursos[i], recurso) == 0){

            if(instancias_recursos_int[i] < 0){

                paquete = list_get(lista_recursos, i);
                for(int j = 0; j < string_array_size(instancias_recursos); j++)
                    if(pcb -> contexto -> pid == paquete -> pids_utilizando[j]){
                        paquete -> pids_utilizando[j] = -1;
                        break;
                    }
                
                pthread_mutex_lock(&mutex_recursos[i]);
                instancias_recursos_int[i]++;
                pthread_mutex_unlock(&mutex_recursos[i]);
        
                //creo q esta esta bien, pero no la probe la verdad
                if(!planificacion_activa){
                    contador_recursos++;
                    sem_wait(&detener_ready_recursos);
                }
                
                desbloquear_segun_recurso(recurso, i);

                sem_post(&sem_recibirCpu);
                free(recurso);
                return;
                
            }else{
                paquete = list_get(lista_recursos, i);
                for(int j = 0; j < string_array_size(instancias_recursos); j++)
                    if(pcb -> contexto -> pid == paquete -> pids_utilizando[j]){
                        paquete -> pids_utilizando[j] = -1;
                        break;
                    }

                pthread_mutex_lock(&mutex_recursos[i]);
                instancias_recursos_int[i]++;
                pthread_mutex_unlock(&mutex_recursos[i]);

                sem_post(&sem_recibirCpu);
                free(recurso);
                return;
            }

            break;
        }
    }

    log_error(logger, "Recurso no existe \"%s\"", recurso);

    free(recurso);

    pthread_mutex_lock(&mutex_lista_exit);
    list_add(lista_exit, pcb);
    pthread_mutex_unlock(&mutex_lista_exit);
    sem_post(&sem_exit_pcb);

    sem_post(&sem_recibirCpu);

    liberar_pcb(pcb);
    pthread_exit(NULL);
}

void desbloquear_segun_recurso(char* recurso, int cardinal){
    struct t_nodo_recurso* paquete = list_get(lista_recursos, cardinal);

    pthread_mutex_lock(&mutex_lista_recursos);
    t_pcb* pcb = list_remove(paquete -> bloqueados_x_recursos, 0);
    pthread_mutex_unlock(&mutex_lista_recursos);

    log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[1]);
    pcb -> contexto -> estado = READY;
    pcb -> contexto -> motivo_bloqueo = NO_BLOQUEADO;

    pthread_mutex_lock(&mutex_lista_ready);
    list_add(lista_ready, pcb);
    logear_ready(true);
    pthread_mutex_unlock(&mutex_lista_ready);
    sem_post(&sem_ready);
}

void manejar_fin_quantum(void* void_args){
    t_pcb* pcb = (t_pcb*) void_args;

    log_info(logger,"PID: <%d> - Desalojado por fin de Quantum", pcb -> contexto -> pid);
    // aca no se si habria que frenarlo tmb cuando detengo la planificacion???????
    sem_post(&sem_recibirCpu);

    pthread_mutex_lock(&mutex_lista_ready);
    list_add(lista_ready, pcb);
    pthread_mutex_unlock(&mutex_lista_ready);

    logear_ready(true);

    sem_post(&sem_ready);
    sem_post(&sem_pasar_procesador);

    pthread_exit(NULL);
}

void guardar_segun_algoritmo(t_pcb* pcb){

    log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[1]);
    pcb -> contexto -> estado = READY;

    if(strcmp(algoritmo_planificacion, "VRR") == 0 && pcb -> quantum > 0){
        
        pthread_mutex_lock(&mutex_lista_aux_vrr);
        list_add(lista_aux_vrr, pcb);
        logear_ready(false);
        pthread_mutex_unlock(&mutex_lista_aux_vrr);
        sem_post(&sem_ready);

    }else{

        pthread_mutex_lock(&mutex_lista_ready);
        list_add(lista_ready, pcb);
        logear_ready(true);
        pthread_mutex_unlock(&mutex_lista_ready);
        sem_post(&sem_ready);
    }
}

void manejar_fs(void* void_args){
    t_pcb* args = (t_pcb*) void_args;
    t_pcb* pcb = args;

    int socket, operacion_fs = recibir_operacion(cpu_dispatch_fd);// es el codop para ver en que case del fs entra
    char* nombre_io = recv_instruccion(cpu_dispatch_fd);
    
    if(verificar_io(nombre_io, &socket)){
        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[2]);
        log_info(logger,"PID: <%d> - Bloqueado por: <FileSystem>",pcb ->contexto ->pid);
        pcb -> contexto -> estado = BLOCKED;

        pthread_mutex_lock(&mutex_cola_filesystem);
        list_add(lista_io_filesystem, pcb);
        pthread_mutex_unlock(&mutex_cola_filesystem);

        pthread_mutex_lock(&mutex_send_filesystem);
        if(!planificacion_activa){//aca nose si habria q usar un semaforo?
            contador_filesystem++;
            sem_wait(&detener_ready_filesystem);
        }
        if(pid_a_finalizar == pcb ->contexto -> pid){
            return;
        }

        t_list* lista_df;
        t_list* lista_tam;
        int puntero_archivo, cantidad_accesos;

        char* nombre_archivo;
        switch (operacion_fs){
            case IO_FS_CREATE:
                nombre_archivo = recv_notificacion(cpu_dispatch_fd);
                send_int(pcb -> contexto -> pid, socket, IO_FS_CREATE);
                send_notificacion(nombre_archivo, socket, 0);
                free(recv_notificacion(socket));
                free(nombre_archivo);
                
                break;
            case IO_FS_DELETE:
                nombre_archivo = recv_notificacion(cpu_dispatch_fd);
                send_int(pcb -> contexto -> pid, socket, IO_FS_DELETE);
                send_notificacion(nombre_archivo, socket, 0);
                free(recv_notificacion(socket));
                free(nombre_archivo);

                break;
            case IO_FS_TRUNCATE:
                nombre_archivo = recv_notificacion(cpu_dispatch_fd);
                recibir_operacion(cpu_dispatch_fd);
                int tamanio_archivo = recv_int(cpu_dispatch_fd);
                
                send_int(pcb -> contexto -> pid, socket, IO_FS_TRUNCATE);
                send_int(tamanio_archivo, socket, 0);
                send_notificacion(nombre_archivo, socket, 0);
                
                free(recv_notificacion(socket));
                free(nombre_archivo);

                break;
            case IO_FS_READ:
                lista_df = list_create();
                lista_tam = list_create();

                recibir_operacion(cpu_dispatch_fd);
                nombre_archivo = recv_paquete_df(&cantidad_accesos, lista_df, lista_tam, cpu_dispatch_fd);
                recibir_operacion(cpu_dispatch_fd);
                puntero_archivo = recv_int(cpu_dispatch_fd);

                send_int(pcb -> contexto -> pid, socket, IO_FS_READ);
                send_notificacion(nombre_archivo, socket, 0);
                send_paquete_df(lista_df, lista_tam, cantidad_accesos, "FILESYSTEM", 0, socket);
                send_int(puntero_archivo, socket, 0);

                free(nombre_archivo);
                free(recv_notificacion(socket));
                list_destroy(lista_df);
                list_destroy(lista_tam);
                break;
            case IO_FS_WRITE:
                lista_df = list_create();
                lista_tam = list_create();

                recibir_operacion(cpu_dispatch_fd);
                nombre_archivo = recv_paquete_df(&cantidad_accesos, lista_df, lista_tam, cpu_dispatch_fd);
                recibir_operacion(cpu_dispatch_fd);
                puntero_archivo = recv_int(cpu_dispatch_fd);

                send_int(pcb -> contexto -> pid, socket, IO_FS_WRITE);
                send_notificacion(nombre_archivo, socket, 0);
                send_paquete_df(lista_df, lista_tam, cantidad_accesos, "FILESYSTEM", 0, socket);
                send_int(puntero_archivo, socket, 0);

                free(nombre_archivo);
                free(recv_notificacion(socket));
                list_destroy(lista_df);
                list_destroy(lista_tam);
                break;
            }
        pthread_mutex_unlock(&mutex_send_filesystem);
        
        sem_post(&sem_recibirCpu);
        sem_post(&sem_pasar_procesador);

        //aca nose si iria otra validacion como la de arriba. Porque el primero que estaba haciendo la io y desp se freno la planificacion, va a seguir de largo. 
        pthread_mutex_lock(&mutex_cola_filesystem);
        pcb = list_remove(lista_io_filesystem, 0);
        pthread_mutex_unlock(&mutex_cola_filesystem);

        guardar_segun_algoritmo(pcb);

    }else{
        log_info(logger_obligatorio,"PID: <%d> - Estado Anterior: <%s> - Estado Actual: <%s>", pcb -> contexto -> pid, array_estados[pcb -> contexto -> estado],array_estados[4]);
        pcb -> contexto -> estado = INVALID_INTERFACE;
        pthread_mutex_lock(&mutex_lista_exit);
        list_add(lista_exit,pcb);
        pthread_mutex_unlock(&mutex_lista_exit);
        sem_post(&sem_exit_pcb);
    }
    free(nombre_io);
    pthread_exit(NULL);
}

bool verificar_io(char* nombreIo, int* socket){
    if(strcmp(nombreIo, "ESPERA") == 0 || strcmp(nombreIo, "GENERICA") == 0 || strcmp(nombreIo, "SLP1") == 0 || strcmp(nombreIo, "SLP2") == 0){
        char* nombre = dictionary_get(diccionario_entradasalidas, "nombreGenerica");
        *socket = dictionary_get(diccionario_entradasalidas, "socketGenerica");
        int estado = dictionary_get(diccionario_entradasalidas, "estadoGenerica");

        if(strcmp(nombreIo, nombre) == 0 && estado == 1){
            char buffer = 0;
            int result = recv(*socket, buffer, sizeof(buffer), MSG_DONTWAIT);
            if(result == 0){
                dictionary_put(diccionario_entradasalidas, "estadoGenerica", 0);
                return false;  
            }
            else{
                return true;
            }
        }
    }else if(strcmp(nombreIo, "TECLADO") == 0){
        char* nombre = dictionary_get(diccionario_entradasalidas, "nombreStdin");
        *socket = dictionary_get(diccionario_entradasalidas, "socketStdin");
        int estado = dictionary_get(diccionario_entradasalidas, "estadoStdin");

        if(strcmp(nombreIo, nombre) == 0 && estado == 1){
            char buffer = 0;
            int result = recv(*socket, buffer, sizeof(buffer), MSG_DONTWAIT);
            if(result == 0){
                dictionary_put(diccionario_entradasalidas, "estadoStdin", 0);
                return false;  
            }
            else{
                return true;
            }

        }
    }else if(strcmp(nombreIo, "MONITOR") == 0){
        char* nombre = dictionary_get(diccionario_entradasalidas, "nombreStdout");
        *socket = dictionary_get(diccionario_entradasalidas, "socketStdout");
        int estado = dictionary_get(diccionario_entradasalidas, "estadoStdout");

        if(strcmp(nombreIo, nombre) == 0 && estado == 1){
            char buffer = 0;
            int result = recv(*socket, buffer, sizeof(buffer), MSG_DONTWAIT);
            if(result == 0){
                dictionary_put(diccionario_entradasalidas, "estadoStdout", 0);
                return false;  
            }
            else{
                return true;
            }
        }
    }else{
        char* nombre = dictionary_get(diccionario_entradasalidas, "nombreFilesystem");
        *socket = dictionary_get(diccionario_entradasalidas, "socketFilesystem");
        int estado = dictionary_get(diccionario_entradasalidas, "estadoFilesystem");

        if(strcmp(nombreIo, nombre) == 0 && estado == 1){
            char buffer = 0;
            int result = recv(*socket, buffer, sizeof(buffer), MSG_DONTWAIT);
            if(result == 0){
                dictionary_put(diccionario_entradasalidas, "estadoFilesystem", 0);
                return false;  
            }
            else{
                return true;
            }

        }
    }
}

void conexiones(){

    pthread_t hconexiones;

    pthread_create(&hconexiones, NULL, (void*)procesar_conexiones, NULL);
    pthread_detach(hconexiones);
}

//FUNCIONES VARIAS

void liberar_diagrama(){
    t_pcb *pcb = NULL;

    for(int i = 0; i < list_size(lista_ready); i++){
        pcb = list_remove(lista_ready, 0);
        liberar_pcb(pcb);
    }
    for(int i = 0; i < list_size(lista_aux_vrr); i++){
        pcb = list_remove(lista_aux_vrr, 0); 
        liberar_pcb(pcb);
    }
    for(int i = 0; i < list_size(lista_new); i++){
        pcb = list_remove(lista_new, 0);
        liberar_pcb(pcb);
    }
    for(int i = 0; i < list_size(lista_generica); i++){
        pcb = list_remove(lista_generica, 0);
        liberar_pcb(pcb);
    }
    for(int i = 0; i < list_size(lista_stdin); i++){
        pcb = list_remove(lista_stdin, 0);
        liberar_pcb(pcb);
    }
    for(int i = 0; i < list_size(lista_stdout); i++){
        pcb = list_remove(lista_stdout, 0);
        liberar_pcb(pcb);
    }
    for(int i = 0; i < list_size(lista_io_filesystem); i++){
        pcb = list_remove(lista_io_filesystem, 0);
        liberar_pcb(pcb);
    }
    struct t_nodo_recurso* paquete;
    for(int i = 0; i < list_size(lista_recursos); i++){
        paquete = list_remove(lista_recursos, 0);
        free(paquete ->pids_utilizando);
        free(paquete ->recurso);
        for(int j = 0; j < list_size(paquete ->bloqueados_x_recursos) ; j++){
            pcb = list_remove(paquete -> bloqueados_x_recursos, 0);
            liberar_pcb(pcb);
        }
        
    }
}

void logear_ready(bool param){
    t_list_iterator* iterador;
    t_pcb* paquete;
    
    if(param){
        iterador = list_iterator_create(lista_ready);
        int pids[list_size(lista_ready)], contador = 0;

        while(list_iterator_has_next(iterador)){
            paquete = list_iterator_next(iterador);
            pids[contador] = paquete -> contexto -> pid;
            contador++;
        }

        for(int i = 0; i < list_size(lista_ready) ; i++)
            log_info(logger_obligatorio,"Cola Ready / Ready Prioridad: %d",pids[i]);
    }else{

        iterador = list_iterator_create(lista_aux_vrr);

        int pids[list_size(lista_aux_vrr)], contador = 0;

        while(list_iterator_has_next(iterador)){
            paquete = list_iterator_next(iterador);
            pids[contador] = paquete -> contexto -> pid;
            contador++;
        }

        for(int i = 0; i < list_size(lista_aux_vrr) ; i++)
            log_info(logger_obligatorio,"Cola Ready / Ready Auxiliar Prioridad: %d",pids[i]);
    }

    

    list_iterator_destroy(iterador);
}

void print_new(void* data){
    t_pcb* pcb = (t_pcb*)data;
    log_info(logger_obligatorio, "%d esta en NEW", pcb -> contexto -> pid);
}

void print_ready(void* data){
    t_pcb* pcb = (t_pcb*)data;
    log_info(logger_obligatorio, "%d esta en READY", pcb -> contexto -> pid);
}

void print_bloq_gen(void* data){
    t_pcb* pcb = (t_pcb*)data;
    log_info(logger_obligatorio, "%d esta en BLOQUEADOS GENERICA", pcb -> contexto -> pid);
}

void print_bloq_stdin(void* data){
    t_pcb* pcb = (t_pcb*)data;
    log_info(logger_obligatorio, "%d esta en BLOQUEADOS STDIN", pcb -> contexto -> pid);
}

void print_bloq_stdout(void* data){
    t_pcb* pcb = (t_pcb*)data;
    log_info(logger_obligatorio, "%d esta en BLOQUEADOS STDOUT", pcb -> contexto -> pid);
}

void print_bloq_recursos(void* data){
    t_pcb* pcb = (t_pcb*)data;
    log_info(logger_obligatorio, "%d esta en BLOQUEADOS WAIT", pcb -> contexto -> pid);
}

void liberar_diccionario(){
    char* consulta = NULL;
    consulta = dictionary_get(diccionario_entradasalidas,"nombreGenerica");
    if(strcmp(consulta,"") != 0)
        free(consulta);

    
    consulta = dictionary_get(diccionario_entradasalidas,"nombreStdin");
    if(strcmp(consulta,"") != 0)
        free(consulta);

    consulta = dictionary_get(diccionario_entradasalidas,"nombreStdout");
    if(strcmp(consulta,"") != 0)
        free(consulta);

    consulta = dictionary_get(diccionario_entradasalidas,"nombreFilesystem");
    if(strcmp(consulta,"") != 0)
        free(consulta);
}

void terminar_kernel(){

    liberar_diagrama();
    liberar_diccionario();
    liberar_conexion(memoria_fd);
    liberar_conexion(cpu_dispatch_fd);
    liberar_conexion(cpu_interrupt_fd);

    list_destroy(lista_new);
    list_destroy(lista_ready);
    list_destroy(lista_exit);
    list_destroy(lista_aux_vrr);
    list_destroy(lista_generica);
    list_destroy(lista_stdin);
    list_destroy(lista_stdout);
    list_destroy(lista_io_filesystem);
    
    log_destroy(logger);
    log_destroy(logger_obligatorio);

    sem_destroy(&sem_multiprogramacion);
    sem_destroy(&sem_nuevo_proceso);
    sem_destroy(&sem_pasar_procesador);
    sem_destroy(&sem_recibirCpu);
    sem_destroy(&sem_exit_pcb);
    sem_destroy(&sem_ready);
    sem_destroy(&sem_sleep);

    pthread_mutex_destroy(&mutex_lista_ready);
    pthread_mutex_destroy(&mutex_lista_new);
    pthread_mutex_destroy(&mutex_lista_exit);
    pthread_mutex_destroy(&mutex_lista_aux_vrr);
    pthread_mutex_destroy(&mutex_contador_pid);
    pthread_mutex_destroy(&mutex_cola_generica);
    pthread_mutex_destroy(&mutex_send_generica);
    pthread_mutex_destroy(&mutex_sleep);

    sem_destroy(&detener_planificador_corto_plazo);
    sem_destroy(&detener_planificador_new);
    sem_destroy(&detener_manejo_motivo);
    sem_destroy(&detener_ready_generica);
    sem_destroy(&detener_ready_stdin);
    sem_destroy(&detener_ready_stdout);
    sem_destroy(&detener_ready_recursos);

    dictionary_destroy(diccionario_entradasalidas);
    config_destroy(config);
    free(instancias_recursos_int);
}