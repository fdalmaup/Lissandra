/*
 * ManejoDeMemoria.c
 *
 *  Created on: 23 jun. 2019
 *      Author: utnso
 */

#include "ManejoDeMemoria.h"


char* obtenerPath(segmento_t* segmento) {
	return segmento->pathTabla;
}

void mostrarContenidoMemoria() {
	timestamp_t timestamp;
	uint16_t key;
	char* value = malloc(sizeof(char) * tamanioValue);
	memcpy(&timestamp, memoriaPrincipal.memoria, sizeof(timestamp_t));
	memcpy(&key, memoriaPrincipal.memoria + sizeof(timestamp_t),
			sizeof(uint16_t));
	strcpy(value,
			memoriaPrincipal.memoria + sizeof(timestamp_t) + sizeof(uint16_t));

	printf("Timestamp: %llu\nKey:%d\nValue: %s\n", timestamp, key, value);

	free(value);

}

void asignarPathASegmento(segmento_t * segmentoANombrar, char* nombreTabla) {
	segmentoANombrar->pathTabla = malloc(
			sizeof(char) * (strlen(pathLFS) + strlen(nombreTabla)) + 1);
	strcpy(segmentoANombrar->pathTabla, pathLFS);
	strcat(segmentoANombrar->pathTabla, nombreTabla);
}

void crearRegistroEnTabla(tabla_de_paginas_t *tablaDondeSeEncuentra, int indiceMarco, bool esInsert) {
	registroTablaPag_t *registroACrear = malloc(sizeof(registroTablaPag_t));

	registroACrear->nroPagina=list_size(tablaDondeSeEncuentra->registrosPag);

	registroACrear->nroMarco = indiceMarco;

	registroACrear->ultimoUso=getCurrentTime();

	registroACrear->flagModificado=esInsert;

	list_add(tablaDondeSeEncuentra->registrosPag,(registroTablaPag_t*) registroACrear);

}

//Retorna el numero de marco donde se encuentra la pagina

int colocarPaginaEnMemoria(timestamp_t timestamp, uint16_t key, char* value) { //DEBE DEVOLVER ERROR_MEMORIA_FULL si la cola esta vacia
	if (queue_is_empty(memoriaPrincipal.marcosLibres)) {	//TODO: PUEDE DESAPARECER o dejar como salvaguarda
		return ERROR_MEMORIA_FULL;
	}
	usleep(vconfig.retardoMemoria * 1000); //TODO: es correcto?

	char* valueAux = malloc(sizeof(char)*tamanioValue);
		strcpy(valueAux,value);
		for(int i=strlen(value)+1; i< tamanioValue; ++i)
			valueAux[i]='\0';

	pthread_mutex_lock(&mutexMemoria);

	pthread_mutex_lock(&mutexColaMarcos);
	MCB_t * marcoObjetivo = (MCB_t *) queue_pop(memoriaPrincipal.marcosLibres); //No se elimina porque el MCB tambien esta en listaAdministrativaMarcos
	pthread_mutex_unlock(&mutexColaMarcos);

	void * direccionMarco = memoriaPrincipal.memoria + memoriaPrincipal.tamanioMarco * marcoObjetivo->nroMarco;

	memcpy(direccionMarco, &timestamp, sizeof(timestamp_t));

	memcpy(direccionMarco + sizeof(timestamp_t), &key, sizeof(uint16_t));

	memcpy(direccionMarco + sizeof(timestamp_t) + sizeof(uint16_t), valueAux,
			(sizeof(char) * tamanioValue));

	pthread_mutex_unlock(&mutexMemoria);

	free(valueAux);

	return marcoObjetivo->nroMarco;
}

int hayMarcoDisponible(void) {
	pthread_mutex_lock(&mutexColaMarcos);
	int marcosDisponibles = queue_is_empty(memoriaPrincipal.marcosLibres);
	pthread_mutex_unlock(&mutexColaMarcos);
	return marcosDisponibles != true;
}

// Se debe tener en cuenta que la p??gina a reemplazar no debe tener el Flag de Modificado activado

int realizarLRU(char* value, uint16_t key, timestamp_t ts, segmento_t * segmento, bool esInsert){
	registroTablaPag_t* registroVictima = NULL;
	segmento_t* segmentoDeVictima = NULL;
	bool primerMacheo = true;

	pthread_mutex_lock(&mutexTablaSegmentos);

	void buscarSegmentoDeNuevaVictima(void * segmento){

		void buscarRegistroVictima(void* registro){
			if((((registroTablaPag_t *) registro)->flagModificado==false) && primerMacheo){
				registroVictima=(registroTablaPag_t *) registro;
				segmentoDeVictima= (segmento_t *) segmento;
				primerMacheo=false;
				return;
			}

			if((((registroTablaPag_t *) registro)->flagModificado==false) && (registroVictima->ultimoUso > ((registroTablaPag_t *) registro)->ultimoUso)){
				registroVictima= (registroTablaPag_t *) registro;
				segmentoDeVictima= (segmento_t *) segmento;
				return;
			}
		}

		list_iterate(((segmento_t *) segmento)->tablaPaginas->registrosPag, buscarRegistroVictima);

	}

	list_iterate(tablaSegmentos.listaSegmentos, buscarSegmentoDeNuevaVictima);

	pthread_mutex_unlock(&mutexTablaSegmentos);

	if(registroVictima != NULL){
		log_info(logger_invisible,"ManejoDeMemoria.C: realizarLRU: liberando registro de segmento %s y creando uno nuevo en segmento %s",obtenerPath(segmentoDeVictima),obtenerPath(segmento));
		//log_info(logger_invisible,"El registro contiene");
		//Elimino el registro

		bool esRegistroVictima(void * registro){
			return ((registroTablaPag_t *) registro)-> nroMarco == registroVictima ->nroMarco;
		}
		void eliminarRegistroVictima(void *registro){
			free((registroTablaPag_t *)registro);
		}
		list_remove_and_destroy_by_condition(segmentoDeVictima->tablaPaginas->registrosPag,
				esRegistroVictima, liberarRegistroTablaPags);


		crearRegistroEnTabla(segmento->tablaPaginas,colocarPaginaEnMemoria(ts, key, value), esInsert);
		return EXIT_SUCCESS;
	}

	log_info(logger_invisible,"ManejoDeMemoria.C: realizarLRU: No se puede reemplazar con LRU, MEMORIA FULL");
	return ERROR_MEMORIA_FULL;
}

int insertarPaginaDeSegmento(char* value, uint16_t key, timestamp_t ts, segmento_t * segmento, bool esInsert) {


	if(hayMarcoDisponible()) {
		crearRegistroEnTabla(segmento->tablaPaginas,colocarPaginaEnMemoria(ts, key, value), esInsert);
		log_info(logger_invisible,"ManejoDeMemoria.C: insertarPaginaDeSegmento: Se ingreso el registro");
		return EXIT_SUCCESS;

	} else {//aplicar el algoritmo de reemplazo (LRU) y en caso de que la memoria se encuentre full iniciar el proceso Journal.
		log_info(logger_invisible,"Procediendo a realizar el algoritmo de reemplazo LRU");
		return realizarLRU(value, key, ts, segmento, esInsert);   //ERROR_MEMORIA_FULL;
	}
}

Operacion tomarContenidoPagina(registroTablaPag_t registro) {
	//Paso copia del registro ya que solo me interesa el nroMarco, no modifico nada en el registro
	usleep(vconfig.retardoMemoria * 1000); //TODO: es correcto?
	Operacion resultadoRetorno;

	resultadoRetorno.TipoDeMensaje=REGISTRO;

	pthread_mutex_lock(&mutexMemoria);

	void * direccionMarco = memoriaPrincipal.memoria
			+ memoriaPrincipal.tamanioMarco * registro.nroMarco;
	/*timestamp_t timestamp;
	 uint16_t key;
	 */
	resultadoRetorno.Argumentos.REGISTRO.value = malloc(
			sizeof(char) * tamanioValue);

	memcpy(&resultadoRetorno.Argumentos.REGISTRO.timestamp, direccionMarco,
			sizeof(timestamp_t));

	memcpy(&resultadoRetorno.Argumentos.REGISTRO.key,
			direccionMarco + sizeof(timestamp_t), sizeof(uint16_t));

	strcpy(resultadoRetorno.Argumentos.REGISTRO.value,
			direccionMarco + sizeof(timestamp_t) + sizeof(uint16_t));

	//printf("Timestamp: %llu\nKey:%d\nValue: %s\n",timestamp,key,value);

	pthread_mutex_unlock(&mutexMemoria);

	return resultadoRetorno;

}

void actualizarValueDeKey(char *value, registroTablaPag_t *registro){
	usleep(vconfig.retardoMemoria * 1000); //TODO: es correcto?

	pthread_mutex_lock(&mutexMemoria);

	void * direccionMarco = memoriaPrincipal.memoria + memoriaPrincipal.tamanioMarco * registro->nroMarco;

	//Seteo el flag de Modificado y actualizo uso
	registro->flagModificado=true;
	registro->ultimoUso=getCurrentTime();


	timestamp_t tsActualizado = getCurrentTime();

	memcpy(direccionMarco, &tsActualizado, sizeof(timestamp_t));

	strcpy(direccionMarco + sizeof(timestamp_t) + sizeof(uint16_t),value);

	pthread_mutex_unlock(&mutexMemoria);

}

int crearSegmentoInsertandoRegistro(char * nombreTabla, char* value, timestamp_t ts, uint16_t key, bool esInsert){

	if(hayMarcoDisponible()){
		//Crear un segmento
		segmento_t* segmentoNuevo = malloc(sizeof(segmento_t));
		//Asignar path determinado
		asignarPathASegmento(segmentoNuevo, nombreTabla);
		//Crear su tabla de paginas
		segmentoNuevo->tablaPaginas = malloc(sizeof(tabla_de_paginas_t));
		segmentoNuevo->tablaPaginas->registrosPag = list_create();

		insertarPaginaDeSegmento(value, key, ts, segmentoNuevo, esInsert);

		//Agregar segmento Nuevo a tabla de segmentos
		pthread_mutex_lock(&mutexTablaSegmentos);

		list_add(tablaSegmentos.listaSegmentos, (segmento_t*) segmentoNuevo);

		pthread_mutex_unlock(&mutexTablaSegmentos);

		return EXIT_SUCCESS;
	}


	return ERROR_MEMORIA_FULL;


}

