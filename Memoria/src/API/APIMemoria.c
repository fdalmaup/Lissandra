/*
 * APIMemoria.c
 *
 *  Created on: 15 may. 2019
 *      Author: fdalmaup
 */
#define JournalTestNumber 3

#include "APIMemoria.h"

void loggearMemoria(void){
	void mostrarRegistros(void * segmento){

		void muestroRegistro(void* registro){

			void mostrarRetorno(Operacion retorno) {
							log_info(logger_invisible,"     REGISTRO: ");
							log_info(logger_invisible,"		Timestamp: %llu\t|\tKey:%d\t|\tValue: %s",
												retorno.Argumentos.REGISTRO.timestamp,
												retorno.Argumentos.REGISTRO.key,
												retorno.Argumentos.REGISTRO.value);
										return;

						}
							log_info(logger_invisible,"	  Nro Pagina: %d",((registroTablaPag_t *) registro)->nroPagina);
							log_info(logger_invisible,"		Ultimo uso: %llu",((registroTablaPag_t *) registro)->ultimoUso);
							log_info(logger_invisible,"		Flag Modificado: %d",((registroTablaPag_t *) registro)->flagModificado);
							Operacion retorno=tomarContenidoPagina(*((registroTablaPag_t *) registro));
							mostrarRetorno(retorno);
							free(retorno.Argumentos.REGISTRO.value);

					}

					log_info(logger_invisible,"Segmento: %s",((segmento_t *) segmento)->pathTabla);
		list_iterate(((segmento_t *) segmento)->tablaPaginas->registrosPag, muestroRegistro);

	}
	log_info(logger_invisible,"========================Estado de la Memoria========================");
	if(list_is_empty(tablaSegmentos.listaSegmentos))

		log_info(logger_invisible,"MEMORIA VACIA");
	else{
		pthread_mutex_lock(&mutexTablaSegmentos);
		list_iterate(tablaSegmentos.listaSegmentos, mostrarRegistros);
		pthread_mutex_unlock(&mutexTablaSegmentos);
	}
}
Operacion ejecutarOperacion(char* input, bool esDeConsola) {
	Comando *parsed = malloc(sizeof(Comando));
	Operacion retorno;
	*parsed = parsear_comando(input);

	if (parsed->valido) {
		switch (parsed->keyword) {
		case SELECT:
			esperarFinJournal();
			seEmpiezaAEjecutarOperacion();
			retorno = selectAPI(input, *parsed);
			seTerminaDeEjecutarOperacion();

			loggearMemoria();
			break;
		case INSERT:
			esperarFinJournal();
			if ((strlen(parsed->argumentos.INSERT.value) + 1) > tamanioValue) {
				retorno.Argumentos.ERROR.mensajeError = string_from_format(
					"Error en el tamanio del value. Segmentation Fault");
				retorno.TipoDeMensaje = ERROR;
			} else {
				seEmpiezaAEjecutarOperacion();
				retorno = insertAPI(input, *parsed);
				seTerminaDeEjecutarOperacion();
			}
			loggearMemoria();
			break;
		case CREATE:
			retorno = createAPI(input, *parsed);
			break;
		case DESCRIBE:
			retorno = describeAPI(input, *parsed);
			break;
		case DROP:
			esperarFinJournal();
			seEmpiezaAEjecutarOperacion();
			retorno =dropAPI(input, *parsed);
			seTerminaDeEjecutarOperacion();
			loggearMemoria();
			break;
		case JOURNAL:
			pthread_mutex_lock(&mutexJournalSimultaneo);
			retorno = journalAPI();
			pthread_mutex_unlock(&mutexJournalSimultaneo);
			loggearMemoria();
			break;
		default:

			fprintf(stderr, RED"No se pude interpretar el enum: %d"STD"\n",
					parsed->keyword);
		}

		destruir_comando(*parsed);
		free(parsed);
		return retorno;
	} else {
		fprintf(stderr, RED"La request no es valida"STD"\n");

		destruir_comando(*parsed);
		free(parsed);
	}

	retorno.TipoDeMensaje = ERROR;
	retorno.Argumentos.ERROR.mensajeError = string_from_format(
			"Error en la recepcion del resultado.");

	return retorno;
}

/*
 * La operaci??n Select permite la obtenci??n del valor de una key dentro de una tabla. Para esto, se utiliza la siguiente nomenclatura:
 SELECT [NOMBRE_TABLA] [KEY]
 Ej:
 SELECT TABLA1 3

 Esta operaci??n incluye los siguientes pasos:
 1. Verifica si existe el segmento de la tabla solicitada y busca en las p??ginas del mismo si contiene key solicitada.
 Si la contiene, devuelve su valor y finaliza el proceso.

 2. Si no la contiene, env??a la solicitud a FileSystem para obtener el valor solicitado y almacenarlo.

 3. Una vez obtenido se debe solicitar una nueva p??gina libre para almacenar el mismo.

 En caso de no disponer de una p??gina libre, se debe ejecutar el algoritmo de reemplazo y,
 en caso de no poder efectuarlo por estar la memoria full, ejecutar el Journal de la memoria.

 * */

Operacion selectAPI(char* input, Comando comando) {
	Operacion resultadoSelect;
	resultadoSelect.TipoDeMensaje = ERROR;

	segmento_t *segmentoSeleccionado = NULL;

	uint16_t keyBuscada = atoi(comando.argumentos.SELECT.key); //TODO: se verifica que la key sea numerica?

	registroTablaPag_t *registroBuscado = NULL;

	if (verificarExistenciaSegmento(comando.argumentos.SELECT.nombreTabla,&segmentoSeleccionado)) {
		if (contieneKey(segmentoSeleccionado, keyBuscada, &registroBuscado)) {

			resultadoSelect = tomarContenidoPagina(*registroBuscado);
			//Actualizo uso
			registroBuscado->ultimoUso=getCurrentTime();

			return resultadoSelect;

		}else{
			//printf(YEL"APIMemoria.c: select: no encontro la key. Enviar a LFS la request"STD"\n");
			log_info(logger_invisible,"APIMemoria.c: select: no encontro la key. Enviando a LFS la request");

			resultadoSelect= comunicarseConLFS(input);

			if(resultadoSelect.TipoDeMensaje==REGISTRO){
				//INSERTAR VALOR EN BLOQUE DE MEMORIA Y METER CREAR REGISTRO EN TABLA DE PAGINAS DEL SEGMENTO

				if(insertarPaginaDeSegmento(resultadoSelect.Argumentos.REGISTRO.value, keyBuscada, resultadoSelect.Argumentos.REGISTRO.timestamp, segmentoSeleccionado, false)== EXIT_SUCCESS){
					return resultadoSelect;
				}
				resultadoSelect.TipoDeMensaje = ERROR_MEMORIAFULL;
				//wait
				return resultadoSelect;

			}else { //SE DEVUELVE EL ERROR QUE DA EL LFS
				return resultadoSelect;
			}


		}

	}else{ //NO EXISTE EL SEGMENTO
		log_info(logger_invisible,"APIMemoria.c: select: no se encontro el path. Enviando a LFS la request");

		resultadoSelect= comunicarseConLFS(input);;

		if(resultadoSelect.TipoDeMensaje==REGISTRO){
			if(crearSegmentoInsertandoRegistro(comando.argumentos.SELECT.nombreTabla, resultadoSelect.Argumentos.REGISTRO.value, resultadoSelect.Argumentos.REGISTRO.timestamp, keyBuscada, false)== EXIT_SUCCESS){

				return resultadoSelect;
			}

			resultadoSelect.TipoDeMensaje = ERROR_MEMORIAFULL;

			return resultadoSelect;
		}
		return resultadoSelect;
	}

}
/*
 Pasos:
 1. Verifica si existe el segmento de la tabla en la memoria principal.
 De existir, busca en sus p??ginas si contiene la key solicitada y de contenerla actualiza el valor insertando el Timestamp actual.
 En caso que no contenga la Key, se solicita una nueva p??gina para almacenar la misma.
 Se deber?? tener en cuenta que si no se disponen de p??ginas libres
 aplicar el algoritmo de reemplazo (LRU) y en caso de que la memoria se encuentre full iniciar el proceso Journal.

 2. En caso que no se encuentre el segmento en memoria principal, se crear?? y se agregar?? la nueva Key con el Timestamp actual,
 junto con el nombre de la tabla en el segmento. Para esto se debe generar el nuevo segmento y solicitar una nueva p??gina
 (aplicando para este ??ltimo la misma l??gica que el punto anterior).

 Cabe destacar que la memoria no verifica la existencia de las tablas al momento de insertar nuevos valores. De esta forma,
 no tiene la necesidad de guardar la metadata del sistema en alguna estructura administrativa. En el caso que al momento de realizar el Journaling
 una tabla no exista, deber?? informar por archivo de log esta situaci??n, pero el proceso deber?? actualizar correctamente las tablas que s?? existen.

 Cabe aclarar que esta operatoria en ning??n momento hace una solicitud directa al FileSystem, la misma deber?? manejarse siempre
 dentro de la memoria principal.
 * */

// INSERT [NOMBRE_TABLA] [KEY] ???[VALUE]???
//Ej:
//INSERT TABLA1 3 ???Mi nombre es Lissandra???

Operacion insertAPI(char* input, Comando comando) {

	Operacion resultadoInsert;
	resultadoInsert.TipoDeMensaje = ERROR;

	segmento_t *segmentoSeleccionado = NULL;

	uint16_t keyBuscada = atoi(comando.argumentos.INSERT.key); //TODO: se verifica que la key sea numerica?

	registroTablaPag_t *registroBuscado = NULL;
	//marco_t *marcoBuscado=NULL;

	//Verifica si existe el segmento de la tabla en la memoria principal.
	if (verificarExistenciaSegmento(comando.argumentos.INSERT.nombreTabla,
			&segmentoSeleccionado)) {
		//Busca en sus p??ginas si contiene la key solicitada y de contenerla actualiza el valor insertando el Timestamp actual.
		if (contieneKey(segmentoSeleccionado, keyBuscada, &registroBuscado)) {

			//En los request solo se utilizar??n las comillas (??????)
			//para enmascarar el Value que se env??e. No se proveer??n request con comillas en otros puntos.

			actualizarValueDeKey(comando.argumentos.INSERT.value, registroBuscado);

			log_info(logger_invisible,"APIMemoria.c: insertAPI: Se realizo el Insert, estaba en memoria la key");

			resultadoInsert.TipoDeMensaje = TEXTO_PLANO;
			resultadoInsert.Argumentos.TEXTO_PLANO.texto = string_from_format(
					"Insert realizado con exito");

			//mostrarContenidoPagina(*registroBuscado); //Para ver lo que se inserto

			return resultadoInsert;

		} else {//No contiene la KEY, se solicita una nueva p??gina para almacenar la misma.

			if(insertarPaginaDeSegmento(comando.argumentos.INSERT.value, keyBuscada,getCurrentTime(), segmentoSeleccionado, true)== EXIT_SUCCESS){
				resultadoInsert.TipoDeMensaje = TEXTO_PLANO;
				resultadoInsert.Argumentos.TEXTO_PLANO.texto = string_from_format(
									"Insert realizado con exito");

				log_info(logger_invisible,"APIMemoria.c: insertAPI: Se realizo el Insert, se pidio pagina");
				return resultadoInsert;
			}

			//log_info(logger_invisible,"Se realizo el INSERT, estaba en memoria\n");
			resultadoInsert.TipoDeMensaje = ERROR_MEMORIAFULL;


			return resultadoInsert;


		}
	} else {	//NO EXISTE SEGMENTO
		/*
		 * se crear?? y se agregar?? la nueva Key con el Timestamp actual,
		 junto con el nombre de la tabla en el segmento. Para esto se debe generar el nuevo segmento y solicitar una nueva p??gina
		 */
		if(crearSegmentoInsertandoRegistro(comando.argumentos.INSERT.nombreTabla, comando.argumentos.INSERT.value, getCurrentTime(), keyBuscada, true) == EXIT_SUCCESS){
			resultadoInsert.TipoDeMensaje = TEXTO_PLANO;
			resultadoInsert.Argumentos.TEXTO_PLANO.texto = string_from_format(
					"Insert realizado con exito");

			return resultadoInsert;
		}

		resultadoInsert.TipoDeMensaje = ERROR_MEMORIAFULL;

		return resultadoInsert;

	}

}

/*
 CREATE [TABLA] [TIPO_CONSISTENCIA] [NUMERO_PARTICIONES] [COMPACTION_TIME]
 Ej:
 CREATE TABLA1 SC 4 60000
 Esta operaci??n incluye los siguientes pasos:
 1.Se env??a al FileSystem la operaci??n para crear la tabla.
 2.Tanto si el FileSystem indica que la operaci??n se realiz?? de forma exitosa o en caso de falla
 por tabla ya existente, contin??a su ejecuci??n normalmente.
 */


Operacion createAPI(char* input, Comando comando) {
	Operacion resultadoCreate;

	//Enviar al FS la operacion
	//Lo que recibo del FS lo retorno

	resultadoCreate= comunicarseConLFS(input);

	return resultadoCreate;

}

/*
 La operaci??n Describe permite obtener la Metadata de una tabla en particular o de todas las tablas que el file system tenga.
 Para esto, se utiliza la siguiente nomenclatura:
 DESCRIBE [NOMBRE_TABLA]

 Esta operaci??n consulta al FileSystem por la metadata de las tablas. Sirve principalmente para poder responder las solicitudes del Kernel.
 */
Operacion describeAPI(char* input, Comando comando) {
	Operacion resultadoDescribe;

	//Enviar al FS la operacion
	//Lo que recibo del FS lo retorno

	resultadoDescribe= comunicarseConLFS(input);

	return resultadoDescribe;
}

/*
 * 		La operaci??n Drop permite la eliminaci??n de una tabla del file system. Para esto, se utiliza la siguiente nomenclatura:
 DROP [NOMBRE_TABLA]

 Esta operaci??n incluye los siguientes pasos:
 1. Verifica si existe un segmento de dicha tabla en la memoria principal y de haberlo libera dicho espacio.
 2. Informa al FileSystem dicha operaci??n para que este ??ltimo realice la operaci??n adecuada.
 *
 * */

Operacion dropAPI(char* input, Comando comando) {
	Operacion resultadoDrop;
	resultadoDrop.TipoDeMensaje = ERROR;

	segmento_t *segmentoSeleccionado = NULL;

	if (verificarExistenciaSegmento(comando.argumentos.DROP.nombreTabla,
			&segmentoSeleccionado)) {
		//EXISTE EL SEGMENTO
		//PROCEDE A LIBERAR EL ESPACIO

		//   SACO EL SEGMENTO DE LA TABLA DE SEGMENTOS
		removerSegmentoDeTabla(segmentoSeleccionado);

		//1. Tomo la tabla de paginas del segmento
		//2. Por cada registro de la tabla:
		//	 2.1. Busco el MCB correspondiente en la tabla administrativa de marcos
		//   y lo pusheo en la cola de marcosLibres

		//	 2.2 Elimino el registro de la tabla de paginas del segmento
		//3. Elimino la tabla de paginas
		//4. Elimino el segmento
		//5. Enviar request al FS

		liberarSegmento(segmentoSeleccionado);

		log_info(logger_visible,"Drop realizado");
		/*
		resultadoDrop.TipoDeMensaje = TEXTO_PLANO;
		resultadoDrop.Argumentos.TEXTO_PLANO.texto = string_from_format(
				"Drop realizado con exito");
*/
	} else {
		/*
		resultadoDrop.TipoDeMensaje = ERROR;
		resultadoDrop.Argumentos.ERROR.mensajeError = string_from_format(
				"La tabla buscada no existe en memoria, se envia la request al File System");
				*/
	}
	//Enviar al FS la operacion
	//Lo que recibo del FS lo retorno

	resultadoDrop= comunicarseConLFS(input);

	return resultadoDrop;

}

//COSAS A TENER EN CUENTA
//Una vez efectuados estos env??os se proceder?? a eliminar los segmentos actuales.

void getNombreTabla(char * nombreSegmento, char **nombreTabla){
	char** pathSeparado=string_split(nombreSegmento,"/");
	int contador=0;
	for(int i =0 ; pathSeparado [i] !=NULL; i++ ){
		contador=i;
	}

	*nombreTabla=string_from_format(pathSeparado[contador]);

	if(pathSeparado != NULL){
		for(int i = 0; pathSeparado[i] != NULL ; ++i)
			free(*(pathSeparado + i));
		free(pathSeparado);
	}

}

Operacion journalAPI(){
	Operacion resultadoJournal;
	Operacion registroAEnviar;
	char * input;
	//	1.1. Si tiene modificado, armo un insert con todos sus datos (viendo de que tabla es) y lo mando al FS

	//  1.2. Si no esta modificado avanzo

	bloquearMemoria();

	esperarFinRequestsViejas();

	pthread_mutex_lock(&mutexTablaSegmentos);

	void recorrerSegmento(void * segmento){

		void enviarRegistroModificado(void* registro){
			char *nombreTabla=NULL;

			if(((registroTablaPag_t *) registro)->flagModificado){

				registroAEnviar=tomarContenidoPagina(*((registroTablaPag_t *) registro));

				getNombreTabla(((segmento_t *) segmento)->pathTabla, &nombreTabla);

				//INSERT <NombreTabla> <KEY> ???<VALUE>??? <TIMESTAMP>
				input=string_from_format("INSERT %s %d \"%s\" %llu",nombreTabla,registroAEnviar.Argumentos.REGISTRO.key,registroAEnviar.Argumentos.REGISTRO.value, registroAEnviar.Argumentos.REGISTRO.timestamp);

				free(registroAEnviar.Argumentos.REGISTRO.value);

				log_info(logger_invisible,"APIMemoria.c: Journaling->Request enviada: %s", input);

				resultadoJournal= comunicarseConLFS(input);

				switch(resultadoJournal.TipoDeMensaje){
				 case TEXTO_PLANO:
					 log_info(logger_invisible,"APIMemoria.c: Resultado Journal: %s", resultadoJournal.Argumentos.TEXTO_PLANO.texto);
					 break;
				 case ERROR:
					 log_error(logger_error,"APIMemoria.c: Resultado Journal: %s", resultadoJournal.Argumentos.ERROR.mensajeError);
					 break;
				 default:
					 log_error(logger_error,"APIMemoria.c: Resultado Journal: No cumple con el tipo de mensaje esperado");
					 break;
				}
				if(nombreTabla!=NULL)
					free(nombreTabla);

				free(input);
				destruir_operacion(resultadoJournal);
				return;
			}
		}

		list_iterate(((segmento_t *) segmento)->tablaPaginas->registrosPag, enviarRegistroModificado);

		return;
	}

	list_iterate(tablaSegmentos.listaSegmentos, recorrerSegmento);



	void dropearTablas(void * segmento){
		removerSegmentoDeTabla(((segmento_t *) segmento));
		liberarSegmento(((segmento_t *) segmento));
		return;
	}

	list_iterate(tablaSegmentos.listaSegmentos, dropearTablas);

	pthread_mutex_unlock(&mutexTablaSegmentos);

	resultadoJournal.TipoDeMensaje = TEXTO_PLANO;
	resultadoJournal.Argumentos.TEXTO_PLANO.texto = string_from_format(
						"Journal finalizado");

	avisoMemoriaLiberada();

	finalizarEspera();
	desbloquearMemoria();
	reiniciarSemaforo();
	return resultadoJournal;
}

void bloquearMemoria(){
	sem_wait(&journal.sem);
}

void desbloquearMemoria(){
	pthread_mutex_lock(&mutexEnEspera);
	for(int i=0; i<=journal.enEspera; ++i)
		sem_post(&journal.sem);
	pthread_mutex_unlock(&mutexEnEspera);
}

void esperarFinJournal(){
	int semValue;
	pthread_mutex_lock(&mutexEnEspera);
	sem_getvalue(&journal.sem, &semValue); //quizas el getvalue tambien pueda generar interbloqueo
	if(semValue >= 1){
		pthread_mutex_unlock(&mutexEnEspera);
		return;
	}
	++journal.enEspera;
	pthread_mutex_unlock(&mutexEnEspera);
	sem_wait(&journal.sem);
}

void reiniciarSemaforo(){
	pthread_mutex_lock(&mutexEnEspera);
	journal.enEspera = 0;
	pthread_mutex_unlock(&mutexEnEspera);
}



void esperarFinRequestsViejas(){
	sem_wait(&journal.semRequest);
}

void seEmpiezaAEjecutarOperacion(){
	int valorSemReq;
	pthread_mutex_lock(&mutexEjecutando);
	++journal.ejecutando;
	sem_getvalue(&journal.semRequest,&valorSemReq);
	pthread_mutex_unlock(&mutexEjecutando);
	if(valorSemReq == 1)
		sem_wait(&journal.semRequest);
}

// post solo si el valor del semaforo es 0 && hay uno solo ejecutandose (si mismo)
void seTerminaDeEjecutarOperacion(){
	pthread_mutex_lock(&mutexEjecutando);
	--journal.ejecutando;
	if(journal.ejecutando == 0)
		sem_post(&journal.semRequest);
	pthread_mutex_unlock(&mutexEjecutando);
}

void finalizarEspera(){
	sem_post(&journal.semRequest);
}

void retenerRequestPorMemoriaFull(){
	int semValue;
	pthread_mutex_lock(&mutexFull);
	sem_getvalue(&journal.memoriaFull, &semValue);
	if(semValue >= 1){
		pthread_mutex_unlock(&mutexFull);
		return;
	}
	++journal.retenidosMemFull;
	pthread_mutex_unlock(&mutexFull);
	sem_wait(&journal.memoriaFull);
}

void avisoMemoriaLiberada(){
	pthread_mutex_lock(&mutexFull);
	for(int i=1; i<=journal.retenidosMemFull; ++i){
		sem_post(&journal.memoriaFull);
	}
	journal.retenidosMemFull = 0;
	pthread_mutex_unlock(&mutexFull);
}
