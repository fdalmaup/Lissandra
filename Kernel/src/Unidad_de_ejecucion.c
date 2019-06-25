/*
 * Unidad_de_ejecucion.c
 *
 *  Created on: 8 may. 2019
 *      Author: facundosalerno
 */

#include "Unidad_de_ejecucion.h"

//FUNCIONES: Privadas. No van en el header.
static int exec_file_lql(PCB *pcb);
static int exec_string_comando(PCB *pcb);
static int procesar_retorno_operacion(Operacion op, PCB *pcb, char *instruccionActual);//Recibe el retorno de operacion (lo mas importante). El pcb es para mostrar datos extras como el nombre del archivo que fallo, al igual que la instruccion actual
static DynamicAddressingRequest direccionar_request(char *request);
static socket_t comunicarse_con_memoria();
static socket_t comunicarse_con_memoria_principal();
static void generar_estadisticas(DynamicAddressingRequest *link);


void *exec(void *null){
	pthread_detach(pthread_self());
	for(;;){
		//La cpu por default esta disponible si esta en este wait
		sem_wait(&scriptEnReady);
		sem_wait(&extraerDeReadyDeAUno);
		PCB *pcb = seleccionar_siguiente();
		sem_post(&extraerDeReadyDeAUno);
		switch(pcb->tipo){
		case STRING_COMANDO:
			exec_string_comando(pcb);
			break;
		case FILE_LQL:
			exec_file_lql(pcb);
			break;
		}
	}
	return NULL;
}





static DynamicAddressingRequest direccionar_request(char *request){
	DynamicAddressingRequest retorno;
	if(USAR_SOLO_MEMORIA_PRINCIPAL){
		retorno.socket = comunicarse_con_memoria_principal();
		return retorno;
	}
	Comando comando = parsear_comando(request);
	Memoria *memoria;
	switch(comando.keyword){//A esta altura ya nos aseguramos de que el comando habia sido valido
	case SELECT:
		memoria = determinar_memoria_para_tabla(comando.argumentos.SELECT.nombreTabla);
		retorno.criterioQueSeUso = consistencia_de_tabla(comando.argumentos.SELECT.nombreTabla);
		retorno.tipoOperacion = SELECT;
		break;
	case INSERT:
		memoria = determinar_memoria_para_tabla(comando.argumentos.INSERT.nombreTabla);
		retorno.criterioQueSeUso = consistencia_de_tabla(comando.argumentos.INSERT.nombreTabla);
		retorno.tipoOperacion = INSERT;
		break;
	case CREATE:
		memoria = determinar_memoria_para_tabla(comando.argumentos.CREATE.nombreTabla);
		break;
	case DESCRIBE:
		memoria = determinar_memoria_para_tabla(comando.argumentos.DESCRIBE.nombreTabla);
		break;
	case DROP:
		memoria = determinar_memoria_para_tabla(comando.argumentos.DROP.nombreTabla);
		break;
	default:
		retorno.socket = EXIT_FAILURE;
		return retorno;
	}
	destruir_comando(comando);

	if(memoria == NULL){
		if(DELEGAR_A_MEMORIA_PRINCIPAL){
			printf(YEL"Warning: la request '%s' se delego a la memoria principal\n"STD, request);
			log_info(logger_invisible, "Warning: la request '%s' se delego a la memoria principal", request);
			retorno.socket = comunicarse_con_memoria_principal();
			return retorno;
		}
		retorno.socket = EXIT_FAILURE;
		return retorno;
	}
	retorno.socket = comunicarse_con_memoria(memoria);
	retorno.memoria = memoria;
	return retorno;
}





static socket_t comunicarse_con_memoria(Memoria *memoria){
	int socketServer;
	if((socketServer = connect_to_server(memoria->ip, memoria->puerto)) == EXIT_FAILURE){
		log_error(logger_error, "Planificador.c: comunicarse_con_memoria: error al conectarse al servidor memoria %s:%s", memoria->ip, memoria->puerto);
		log_error(logger_invisible, "Planificador.c: comunicarse_con_memoria: error al conectarse al servidor memoria %s:%s", memoria->ip, memoria->puerto);
		return EXIT_FAILURE;
	}
	log_info(logger_invisible, "Conectado a la memoria numero: %d, %s:%s", memoria->numero, memoria->ip, memoria->puerto);
	log_info(logger_visible, "Conectado a la memoria numero: %d, %s:%s", memoria->numero, memoria->ip, memoria->puerto);
	return socketServer;
}





static socket_t comunicarse_con_memoria_principal(){
	int socketServer;
	if((socketServer = connect_to_server(fconfig.ip_memoria_principal, fconfig.puerto_memoria_principal)) == EXIT_FAILURE){
		log_error(logger_error, "Unidad_de_ejecucion.c: comunicarse_con_memoria_principal: error al conectarse al servidor memoria %s:%s", fconfig.ip_memoria_principal, fconfig.puerto_memoria_principal);
		log_error(logger_invisible, "Unidad_de_ejecucion.c: comunicarse_con_memoria_principal: error al conectarse al servidor memoria %s:%s", fconfig.ip_memoria_principal, fconfig.puerto_memoria_principal);
		return EXIT_FAILURE;
	}
	log_info(logger_invisible, "Conectado a la memoria principal %s:%s", fconfig.ip_memoria_principal, fconfig.puerto_memoria_principal);
	return socketServer;
}





static int exec_string_comando(PCB *pcb){
	DynamicAddressingRequest target = direccionar_request((char *)pcb->data);
	if(target.socket == EXIT_FAILURE){
		free(pcb->data);
		free(pcb);
		log_error(logger_error, "Unidad_de_ejecucion.c: exec_string_comando: no se pudo direccionar la request");
		log_error(logger_invisible, "Unidad_de_ejecucion.c: exec_string_comando: no se pudo direccionar la request");
		return INSTRUCCION_ERROR;
	}
	target.inicioOperacion = getCurrentTime();
	Operacion request;
	request.opCode = getNumber();
	request.TipoDeMensaje = COMANDO;
	request.Argumentos.COMANDO.comandoParseable = (char*)pcb->data;
	send_msg(target.socket, request);

	request = recv_msg(target.socket);
	if(procesar_retorno_operacion(request, pcb, (char*)pcb->data) != EXIT_FAILURE){
		target.operacionExitosa = true;
		target.finOperacion = getCurrentTime();
		generar_estadisticas(&target);
	}

	destruir_operacion(request);
	free(pcb->data);
	free(pcb->nombreArchivoLQL);
	free(pcb);
	close(target.socket);
	return FINALIZO;
}





static int exec_file_lql(PCB *pcb){
	Operacion request;
	char buffer[MAX_BUFFER_SIZE_FOR_LQL_LINE];
	char *line = NULL;
	FILE *lql = (FILE *)pcb->data; //Como el FILE nunca se cerro, cada vez que entre, va a continuar donde se habia quedado
	int quantumBuffer = vconfig.quantum(); //Para hacer la llamada una sola vez por cada exec. No se actualiza el quantum en tiempo real, pero se actualiza cuando entra un nuevo script por que ya tiene el valor actualizado

	for(int i=1; i<=quantumBuffer; ++i){
		line = fgets(buffer, MAX_BUFFER_SIZE_FOR_LQL_LINE, lql);
		if(line == NULL || feof(lql)){
			printf("\n");
			fclose(lql);
			free(pcb->nombreArchivoLQL);
			free(pcb);
			return FINALIZO;
		}
		DynamicAddressingRequest target = direccionar_request(line);
		if(target.socket == EXIT_FAILURE){
			printf("\n");
			fclose(lql);
			free(pcb->nombreArchivoLQL);
			free(pcb);
			log_error(logger_error, "Unidad_de_ejecucion.c: exec_file_lql: no se pudo direccionar la request");
			log_error(logger_invisible, "Unidad_de_ejecucion.c: exec_file_lql: no se pudo direccionar la request");
			return INSTRUCCION_ERROR;
		}
		target.inicioOperacion = getCurrentTime();
		id TEMPORTAL_OPCODE = getNumber(); //TODO: Fix para que se vea el opcode, sacar cuando el resto del sistema sea compatible con esto
		request.opCode = TEMPORTAL_OPCODE;
		request.TipoDeMensaje = COMANDO;
		request.Argumentos.COMANDO.comandoParseable = line;
		send_msg(target.socket, request);

		request = recv_msg(target.socket);
		request.opCode = TEMPORTAL_OPCODE;
		if(procesar_retorno_operacion(request, pcb, line) == INSTRUCCION_ERROR){
			fclose(lql);
			free(pcb->nombreArchivoLQL);
			free(pcb);
			close(target.socket);
			return INSTRUCCION_ERROR;
		}
		target.operacionExitosa = true;
		target.finOperacion = getCurrentTime();
		generar_estadisticas(&target);
		close(target.socket);
	}
	printf("\n");
	sem_wait(&meterEnReadyDeAUno);
	desalojar(pcb);
	sem_post(&meterEnReadyDeAUno);
	sem_post(&scriptEnReady); //Ya que se metio a la lista de vuelta
	simular_retardo();
	return DESALOJO;
}





static int procesar_retorno_operacion(Operacion op, PCB* pcb, char* instruccionActual){
	char *instruccionActualTemp = NULL;
	switch(op.TipoDeMensaje){
	case TEXTO_PLANO:
		log_info(logger_visible,"CPU: %d | ID Operacion: %d | %s", process_get_thread_id(), op.opCode, op.Argumentos.TEXTO_PLANO.texto);
		log_info(logger_invisible,"CPU: %d | ID Operacion: %d | %s", process_get_thread_id(), op.opCode, op.Argumentos.TEXTO_PLANO.texto);
		return CONTINUAR;
	case REGISTRO:
		log_info(logger_visible,"CPU: %d | ID Operacion: %d | Timestamp: %llu, Key: %d, Value: %s", process_get_thread_id(), op.opCode, op.Argumentos.REGISTRO.timestamp, op.Argumentos.REGISTRO.key, op.Argumentos.REGISTRO.value);
		log_info(logger_invisible,"CPU: %d | ID Operacion: %d | Timestamp: %llu, Key: %d, Value: %s", process_get_thread_id(), op.opCode, op.Argumentos.REGISTRO.timestamp, op.Argumentos.REGISTRO.key, op.Argumentos.REGISTRO.value);
		return CONTINUAR;
	case ERROR:
		instruccionActualTemp = remover_new_line(instruccionActual);
		log_error(logger_error,"CPU: %d | ID Operacion: %d | Fallo en la instruccion '%s', Path: '%s'. Abortando: %s", process_get_thread_id(), op.opCode, instruccionActualTemp, pcb->nombreArchivoLQL, op.Argumentos.ERROR.mensajeError);
		log_error(logger_invisible,"CPU: %d | ID Operacion: %d | Fallo en la instruccion '%s', Path: '%s'. Abortando: %s", process_get_thread_id(), op.opCode, instruccionActualTemp, pcb->nombreArchivoLQL, op.Argumentos.ERROR.mensajeError);
		free(instruccionActualTemp);
		return INSTRUCCION_ERROR;
	case DESCRIBE_REQUEST:
		if(procesar_describe(op.Argumentos.DESCRIBE_REQUEST.resultado_comprimido) == EXIT_FAILURE){
			log_error(logger_error,"CPU: %d | ID Operacion: %d | Abortando: Fallo Describe", process_get_thread_id(), op.opCode);
			log_error(logger_invisible,"CPU: %d | ID Operacion: %d | Abortando: Fallo Describe", process_get_thread_id(), op.opCode);
			return INSTRUCCION_ERROR;
		}
		mostrar_describe(op.Argumentos.DESCRIBE_REQUEST.resultado_comprimido);
		return CONTINUAR;
	default:
		instruccionActualTemp = remover_new_line(instruccionActual);
		log_error(logger_visible,"CPU: %d | ID Operacion: %d | Instruccion '%s' invalida o fuera de contexto", process_get_thread_id(), op.opCode, instruccionActualTemp);
		log_error(logger_invisible,"CPU: %d | ID Operacion: %d | Instruccion '%s' invalida o fuera de contexto", process_get_thread_id(), op.opCode, instruccionActualTemp);
		free(instruccionActualTemp);
		return INSTRUCCION_ERROR;
	}
	return INSTRUCCION_ERROR;
}





static void generar_estadisticas(DynamicAddressingRequest *link){
	if(USAR_SOLO_MEMORIA_PRINCIPAL)
		return;
	if(!link->operacionExitosa)
		return;
	switch(link->criterioQueSeUso){
	case SC:
		if(link->tipoOperacion == SELECT){
			++metricas.At.StrongConsistency.reads;
			metricas.At.StrongConsistency.acumuladorTiemposRead += link->finOperacion - link->inicioOperacion;
			metricas.At.StrongConsistency.readLatency = metricas.At.StrongConsistency.acumuladorTiemposRead / metricas.At.StrongConsistency.reads;
			return;
		}
		if(link->tipoOperacion == INSERT){
			++metricas.At.StrongConsistency.writes;
			metricas.At.StrongConsistency.acumuladorTiemposWrite += link->finOperacion - link->inicioOperacion;
			metricas.At.StrongConsistency.writeLatency = metricas.At.StrongConsistency.acumuladorTiemposWrite / metricas.At.StrongConsistency.writes;
			return;
		}
		break;
	case HSC:
		if(link->tipoOperacion == SELECT){
			++metricas.At.HashStrongConsistency.reads;
			metricas.At.HashStrongConsistency.acumuladorTiemposRead += link->finOperacion - link->inicioOperacion;
			metricas.At.HashStrongConsistency.readLatency = metricas.At.HashStrongConsistency.acumuladorTiemposRead / metricas.At.HashStrongConsistency.reads;
			return;
		}
		if(link->tipoOperacion == INSERT){
			++metricas.At.HashStrongConsistency.writes;
			metricas.At.HashStrongConsistency.acumuladorTiemposWrite += link->finOperacion - link->inicioOperacion;
			metricas.At.HashStrongConsistency.writeLatency = metricas.At.HashStrongConsistency.acumuladorTiemposWrite / metricas.At.HashStrongConsistency.writes;
			return;
		}
		break;
	case EC:
		if(link->tipoOperacion == SELECT){
			++metricas.At.EventualConsistency.reads;
			metricas.At.EventualConsistency.acumuladorTiemposRead += link->finOperacion - link->inicioOperacion;
			metricas.At.EventualConsistency.readLatency = metricas.At.EventualConsistency.acumuladorTiemposRead / metricas.At.EventualConsistency.reads;
			return;
		}
		if(link->tipoOperacion == INSERT){
			++metricas.At.EventualConsistency.writes;
			metricas.At.EventualConsistency.acumuladorTiemposWrite += link->finOperacion - link->inicioOperacion;
			metricas.At.EventualConsistency.writeLatency = metricas.At.EventualConsistency.acumuladorTiemposWrite / metricas.At.EventualConsistency.writes;
			return;
		}
		break;
	default:
		return;
	}
}
