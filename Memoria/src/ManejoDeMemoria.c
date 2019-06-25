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

	registroACrear->nroPagina=list_size(tablaDondeSeEncuentra->registrosPag)-1;

	registroACrear->nroMarco = indiceMarco;

	registroACrear->flagModificado=esInsert;

	list_add(tablaDondeSeEncuentra->registrosPag,(registroTablaPag_t*) registroACrear);

}

//Retorna el numero de marco donde se encuentra la pagina

int colocarPaginaEnMemoria(timestamp_t timestamp, uint16_t key, char* value) { //DEBE DEVOLVER ERROR_MEMORIA_FULL si la cola esta vacia
	if (queue_is_empty(memoriaPrincipal.marcosLibres)) {	//TODO: PUEDE DESAPARECER o dejar como salvaguarda
		return ERROR_MEMORIA_FULL;
	}

	//TODO: wSEMAFORO
	MCB_t * marcoObjetivo = (MCB_t *) queue_pop(memoriaPrincipal.marcosLibres); //No se elimina porque el MCB tambien esta en listaAdministrativaMarcos

	void * direccionMarco = memoriaPrincipal.memoria + memoriaPrincipal.tamanioMarco * marcoObjetivo->nroMarco;

	memcpy(direccionMarco, &timestamp, sizeof(timestamp_t));

	memcpy(direccionMarco + sizeof(timestamp_t), &key, sizeof(uint16_t));

	memcpy(direccionMarco + sizeof(timestamp_t) + sizeof(uint16_t), value,
			(sizeof(char) * tamanioValue));
	//sSEMAFORO
	return marcoObjetivo->nroMarco;
}

int hayPaginaDisponible(void) {
	return queue_is_empty(memoriaPrincipal.marcosLibres) != true;
}

// Se debe tener en cuenta que la página a reemplazar no debe tener el Flag de Modificado activado

void insertarPaginaDeSegmento(char* value, uint16_t key, timestamp_t ts, segmento_t * segmento, bool esInsert) { //TODO: PUEDE DEVOLVER FULL OJOOOOOO
	if (hayPaginaDisponible()) {
		crearRegistroEnTabla(segmento->tablaPaginas,colocarPaginaEnMemoria(ts, key, value), esInsert);
		//TODO: no olvidar
		printf("Se ingreso el registro\n");

	} else {//aplicar el algoritmo de reemplazo (LRU) y en caso de que la memoria se encuentre full iniciar el proceso Journal.
		printf("HACER LRU Y SINO AVISAR QUE SE DEBE REALIZAR JOURNAL YA QUE LA MEMORIA ESTA FULL\n");
	}
}

Operacion tomarContenidoPagina(registroTablaPag_t registro) {
	//Paso copia del registro ya que solo me interesa el nroMarco, no modifico nada en el registro

	Operacion resultadoRetorno;

	resultadoRetorno.TipoDeMensaje=REGISTRO;

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

	return resultadoRetorno;

}

void actualizarValueDeKey(char *value, registroTablaPag_t *registro){
	void * direccionMarco = memoriaPrincipal.memoria + memoriaPrincipal.tamanioMarco * registro->nroMarco;

	//Seteo el flag de Modificado
	registro->flagModificado=true;

	timestamp_t tsActualizado = getCurrentTime();

	memcpy(direccionMarco, &tsActualizado, sizeof(timestamp_t));
	strcpy(direccionMarco + sizeof(timestamp_t) + sizeof(uint16_t),value);

}

void crearSegmentoInsertandoRegistro(char * nombreTabla, char* value, timestamp_t ts, uint16_t key, bool esInsert){
	//TODO: PUEDE DEVOLVER FULL en la parte de insertar

	//Crear un segmento
	segmento_t* segmentoNuevo = malloc(sizeof(segmento_t));

	//Asignar path determinado
	asignarPathASegmento(segmentoNuevo, nombreTabla);

	//Crear su tabla de paginas
	segmentoNuevo->tablaPaginas = malloc(sizeof(tabla_de_paginas_t));
	segmentoNuevo->tablaPaginas->registrosPag = list_create();

	insertarPaginaDeSegmento(value, key, ts, segmentoNuevo, esInsert); //TODO: PUEDE DEVOLVER FULL OJOOOOOO

	//Agregar segmento Nuevo a tabla de segmentos
	list_add(tablaSegmentos.listaSegmentos, (segmento_t*) segmentoNuevo);
}

