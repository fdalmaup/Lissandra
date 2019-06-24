/*
 * APIMemoria.c
 *
 *  Created on: 15 may. 2019
 *      Author: fdalmaup
 */
#define JournalTestNumber 3

#include "APIMemoria.h"

Operacion ejecutarOperacion(char* input, bool esDeConsola) {
	Comando *parsed = malloc(sizeof(Comando));
	Operacion retorno;
	*parsed = parsear_comando(input);
	usleep(vconfig.retardoMemoria() * 1000);
	if (parsed->valido) {
		switch (parsed->keyword) {
		case SELECT:
			retorno = selectAPI(input, *parsed);
			break;
		case INSERT:
			if ((strlen(parsed->argumentos.INSERT.value) + 1) > tamanioValue) {
				printf("TamValue teorico: %d\nTamValue real: %d\n",
						tamanioValue,
						(strlen(parsed->argumentos.INSERT.value) + 1));
				retorno.Argumentos.ERROR.mensajeError = string_from_format(
						"Error en el tamanio del value. Segmentation Fault");
				retorno.TipoDeMensaje = ERROR;
			} else {
				retorno = insertAPI(input, *parsed);
			}

			break;
		case CREATE:
			retorno = createAPI(input, *parsed);
			break;
		case DESCRIBE:
			retorno = describeAPI(input, *parsed);
			break;
		case DROP:
			retorno = dropAPI(input, *parsed);
			break;
		case JOURNAL:
			retorno = journalAPI();
			break;
		default:

			fprintf(stderr, RED"No se pude interpretar el enum: %d"STD"\n",
					parsed->keyword);
		}

		destruir_comando(*parsed);
		free(parsed);
		free(input);
		return retorno;
	} else {
		fprintf(stderr, RED"La request no es valida"STD"\n");

		destruir_comando(*parsed);
		free(parsed);
	}

	retorno.TipoDeMensaje = ERROR;
	retorno.Argumentos.ERROR.mensajeError = string_from_format(
			"Error en la recepcion del resultado.");

	free(input);

	return retorno;
}

/*
 * La operación Select permite la obtención del valor de una key dentro de una tabla. Para esto, se utiliza la siguiente nomenclatura:
 SELECT [NOMBRE_TABLA] [KEY]
 Ej:
 SELECT TABLA1 3

 Esta operación incluye los siguientes pasos:
 1. Verifica si existe el segmento de la tabla solicitada y busca en las páginas del mismo si contiene key solicitada.
 Si la contiene, devuelve su valor y finaliza el proceso.

 2. Si no la contiene, envía la solicitud a FileSystem para obtener el valor solicitado y almacenarlo.

 3. Una vez obtenido se debe solicitar una nueva página libre para almacenar el mismo.

 En caso de no disponer de una página libre, se debe ejecutar el algoritmo de reemplazo y,
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

			return resultadoSelect;

		}else{
			printf(YEL"APIMemoria.c: select: no encontro la key. Enviar a LFS la request"STD"\n"); //TODO: meter en log

			enviarRequestFS(input);

			resultadoSelect=recibirRequestFS();
			if(resultadoSelect.TipoDeMensaje==REGISTRO){
				//INSERTAR VALOR EN BLOQUE DE MEMORIA Y METER CREAR REGISTRO EN TABLA DE PAGINAS DEL SEGMENTO

				insertarPaginaDeSegmento(resultadoSelect.Argumentos.REGISTRO.value, keyBuscada, resultadoSelect.Argumentos.REGISTRO.timestamp, segmentoSeleccionado, false);

				//TODO: PUEDE DEVOLVER FULL OJOOOOOO, en este caso el resultado de select es ERROR con mensaje MEMORIA FULL
				return resultadoSelect;
			}else { //SE DEVUELVE EL ERROR QUE DA EL LFS
				return resultadoSelect;
			}


		}

	}else{ //NO EXISTE EL SEGMENTO
		printf(YEL"APIMemoria.c: select: no se encontro el path. Enviando a LFS la request"STD"\n");

		enviarRequestFS(input);

		resultadoSelect=recibirRequestFS();
		if(resultadoSelect.TipoDeMensaje==REGISTRO){
			crearSegmentoInsertandoRegistro(comando.argumentos.SELECT.nombreTabla, resultadoSelect.Argumentos.REGISTRO.value, resultadoSelect.Argumentos.REGISTRO.timestamp, keyBuscada, false);
			return resultadoSelect;
		}else {
			return resultadoSelect;
		}
	 }

	resultadoSelect.Argumentos.ERROR.mensajeError = string_from_format(
			"NO SE HA ENCONTRADO LA KEY");
	return resultadoSelect;

}
/*
 Pasos:
 1. Verifica si existe el segmento de la tabla en la memoria principal.
 De existir, busca en sus páginas si contiene la key solicitada y de contenerla actualiza el valor insertando el Timestamp actual.
 En caso que no contenga la Key, se solicita una nueva página para almacenar la misma.
 Se deberá tener en cuenta que si no se disponen de páginas libres
 aplicar el algoritmo de reemplazo (LRU) y en caso de que la memoria se encuentre full iniciar el proceso Journal.

 2. En caso que no se encuentre el segmento en memoria principal, se creará y se agregará la nueva Key con el Timestamp actual,
 junto con el nombre de la tabla en el segmento. Para esto se debe generar el nuevo segmento y solicitar una nueva página
 (aplicando para este último la misma lógica que el punto anterior).

 Cabe destacar que la memoria no verifica la existencia de las tablas al momento de insertar nuevos valores. De esta forma,
 no tiene la necesidad de guardar la metadata del sistema en alguna estructura administrativa. En el caso que al momento de realizar el Journaling
 una tabla no exista, deberá informar por archivo de log esta situación, pero el proceso deberá actualizar correctamente las tablas que sí existen.

 Cabe aclarar que esta operatoria en ningún momento hace una solicitud directa al FileSystem, la misma deberá manejarse siempre
 dentro de la memoria principal.
 * */

// INSERT [NOMBRE_TABLA] [KEY] “[VALUE]”
//Ej:
//INSERT TABLA1 3 “Mi nombre es Lissandra”

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
		//Busca en sus páginas si contiene la key solicitada y de contenerla actualiza el valor insertando el Timestamp actual.
		if (contieneKey(segmentoSeleccionado, keyBuscada, &registroBuscado)) {

			//En los request solo se utilizarán las comillas (“”)
			//para enmascarar el Value que se envíe. No se proveerán request con comillas en otros puntos.

			actualizarValueDeKey(comando.argumentos.INSERT.value, registroBuscado);

			printf("Se realizo el INSERT\n");

			resultadoInsert.TipoDeMensaje = TEXTO_PLANO;
			resultadoInsert.Argumentos.TEXTO_PLANO.texto = string_from_format(
					"INSERT REALIZADO CON EXITO");

			//mostrarContenidoPagina(*registroBuscado); //Para ver lo que se inserto

			return resultadoInsert;

		} else {//No contiene la KEY, se solicita una nueva página para almacenar la misma.

			insertarPaginaDeSegmento(comando.argumentos.INSERT.value, keyBuscada,getCurrentTime(), segmentoSeleccionado, true);

			//TODO: PUEDE DEVOLVER FULL OJOOOOOO, en este caso el resultado de INSERT es ERROR con mensaje MEMORIA FULL

			resultadoInsert.TipoDeMensaje = TEXTO_PLANO;
			resultadoInsert.Argumentos.TEXTO_PLANO.texto = string_from_format(
					"INSERT REALIZADO CON EXITO");

			return resultadoInsert;
		}
	} else {	//NO EXISTE SEGMENTO
		/*
		 * se creará y se agregará la nueva Key con el Timestamp actual,
		 junto con el nombre de la tabla en el segmento. Para esto se debe generar el nuevo segmento y solicitar una nueva página
		 */

		crearSegmentoInsertandoRegistro(comando.argumentos.INSERT.nombreTabla, comando.argumentos.INSERT.value, getCurrentTime(), keyBuscada, true);

		//TODO: PUEDE DEVOLVER FULL OJOOOOOO, en este caso el resultado de INSERT es ERROR con mensaje MEMORIA FULL

		resultadoInsert.TipoDeMensaje = TEXTO_PLANO;
		resultadoInsert.Argumentos.TEXTO_PLANO.texto = string_from_format(
				"INSERT REALIZADO CON EXITO");

		return resultadoInsert;
	}

}

/*
 CREATE [TABLA] [TIPO_CONSISTENCIA] [NUMERO_PARTICIONES] [COMPACTION_TIME]
 Ej:
 CREATE TABLA1 SC 4 60000
 Esta operación incluye los siguientes pasos:
 1.Se envía al FileSystem la operación para crear la tabla.
 2.Tanto si el FileSystem indica que la operación se realizó de forma exitosa o en caso de falla
 por tabla ya existente, continúa su ejecución normalmente.
 */


Operacion createAPI(char* input, Comando comando) {
	Operacion resultadoCreate;

	//Enviar al FS la operacion
	enviarRequestFS(input);

	//Lo que recibo del FS lo retorno

	resultadoCreate=recibirRequestFS();

	return resultadoCreate;

}

/*
 La operación Describe permite obtener la Metadata de una tabla en particular o de todas las tablas que el file system tenga.
 Para esto, se utiliza la siguiente nomenclatura:
 DESCRIBE [NOMBRE_TABLA]

 Esta operación consulta al FileSystem por la metadata de las tablas. Sirve principalmente para poder responder las solicitudes del Kernel.
 */
Operacion describeAPI(char* input, Comando comando) {
	Operacion resultadoDescribe;

	//Enviar al FS la operacion
	enviarRequestFS(input);

	//Lo que recibo del FS lo retorno

	resultadoDescribe=recibirRequestFS();

	return resultadoDescribe;
}

/*
 * 		La operación Drop permite la eliminación de una tabla del file system. Para esto, se utiliza la siguiente nomenclatura:
 DROP [NOMBRE_TABLA]

 Esta operación incluye los siguientes pasos:
 1. Verifica si existe un segmento de dicha tabla en la memoria principal y de haberlo libera dicho espacio.
 2. Informa al FileSystem dicha operación para que este último realice la operación adecuada.
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

		printf("Drop realizado\n");

		resultadoDrop.TipoDeMensaje = TEXTO_PLANO;
		resultadoDrop.Argumentos.TEXTO_PLANO.texto = string_from_format(
				"Drop realizado con exito");
	} else {
		resultadoDrop.TipoDeMensaje = ERROR;
		resultadoDrop.Argumentos.ERROR.mensajeError = string_from_format(
				"La tabla buscada no existe en memoria");
	}
	return resultadoDrop;

}

//TODO: BORRAR Y DEJAR JOURNAL BIEN

void mostrarRegistrosConFlagDeModificado(void){
	void mostrarRegistros(void * segmento){

		void muestroRegistro(void* registro){

			void mostrarRetorno(Operacion retorno) {
							printf("REGISTRO\n");
							printf("Timestamp: %llu\nKey:%d\nValue: %s\n",
									retorno.Argumentos.REGISTRO.timestamp,
									retorno.Argumentos.REGISTRO.key,
									retorno.Argumentos.REGISTRO.value);
							return;

			}

			if(((registroTablaPag_t *) registro)->flagModificado){
				mostrarRetorno(tomarContenidoPagina(*((registroTablaPag_t *) registro)));
			}
		}


		list_iterate(((segmento_t *) segmento)->tablaPaginas->registrosPag, muestroRegistro);

	}
	list_iterate(tablaSegmentos.listaSegmentos, mostrarRegistros);

}

//COSAS A TENER EN CUENTA
//Una vez efectuados estos envíos se procederá a eliminar los segmentos actuales.

Operacion journalAPI(){
	Operacion resultadoJournal;
	char * input;
	usleep(vconfig.retardoJOURNAL() * 1000);

	//char*   string_from_format(const char* format, ...);
	//1. Por cada MCB en la listaAdminMarcos ver si tiene el flag de modificado
	//(si en vez de tener en esa lista, lo tengo en la tabla de paginas de cada segmento, ya tengo la tabla PENSAR )
	//	1.1. Si tiene modificado, armo un insert con todos sus datos (viendo de que tabla es) y lo mando al FS

	//  1.2. Si no esta modificado avanzo
	for(int i=0; i< JournalTestNumber; i++){


		//Enviar al FS la operacion

		resultadoJournal.TipoDeMensaje = COMANDO;
		//resultadoJournal.Argumentos.COMANDO.comandoParseable= string_from_format(input);
		input=string_from_format("INSERT tabla%d %d \"JOHN WILLY\" %llu",i,i*100, getCurrentTime());

		enviarRequestFS(input);



		resultadoJournal=recibirRequestFS();


	}
	return resultadoJournal;

	//mostrarRegistrosConFlagDeModificado();
}

