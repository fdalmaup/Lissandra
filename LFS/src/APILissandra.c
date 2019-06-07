/*
 * APILissandra.c
 *
 *  Created on: 20 may. 2019
 *      Author: juanmaalt
 */

#include "APILissandra.h"



/*INICIO FUNCIONES API*/

int selectAPI(Comando comando){
	/*Checkea existencia de la Memtable*/
	if (!memtable) {
		log_error(logger_invisible, "No existe una memtable creada, no se puede realizar la operación.");
		return EXIT_FAILURE;
	}
	/*Checkea existencia de la tabla solicitada*/
	if(!(existeTabla(comando.argumentos.SELECT.nombreTabla))){
		log_error(logger_invisible, "No existe una tabla asociada a la key solicitada.");
		//avisar a la memoria?
		return EXIT_FAILURE;
	}
	/*Levanta la metadata de la tabla*/
	t_config* metadataFile = leerMetadata(comando.argumentos.SELECT.nombreTabla);
	getMetadata(comando.argumentos.SELECT.nombreTabla, metadataFile);

	/*Levanta la lista de registros relacionados a la tabla*/
	t_list* data = getData(comando.argumentos.SELECT.nombreTabla);

	/*Calculo el nro de partición en la que se encuentra la key*/
	int particionNbr = calcularParticionNbr(comando.argumentos.SELECT.key, metadata.partitions);

	/*Creo la lista de valores que condicen con dicha key*/
	t_list* listaDeValues = list_create();
	listaDeValues = buscarValue(data, comando.argumentos.SELECT.key, particionNbr);

	/*Recorro la tabla y obtengo el valor más reciente*/
	recorrerTabla(listaDeValues);//Función ad-hoc para testing
	getValueMasReciente(listaDeValues);

	/*Libero recursos en memoria*/
	list_destroy(listaDeValues);
	config_destroy(metadataFile);

	return EXIT_SUCCESS;
}


int insertAPI(Comando comando){
	/*Checkea existencia de la Memtable*/
	if (!memtable) {
		log_error(logger_invisible, "No existe una memtable creada, no se puede realizar la operación.");
		return EXIT_FAILURE;
	}
	/*Checkea existencia de la tabla solicitada*/
	if(!(existeTabla(comando.argumentos.SELECT.nombreTabla))){
		log_error(logger_invisible, "No existe una tabla asociada a la key solicitada.");
		//avisar a la memoria?
		return EXIT_FAILURE;
	}

	//checkExisteMemoria(); //Verificar si existe en memoria una lista de datos a dumpear. De no existir, alocar dicha memoria.

	/*Reservo espacio y aloco los datos a insertar*/
	Registro* reg = malloc(sizeof(Registro));
	reg->key = atoi(comando.argumentos.INSERT.key);
	reg->value = comando.argumentos.INSERT.value;
	reg->timestamp=checkTimestamp(comando.argumentos.INSERT.timestamp);

	/*Obtengo la lista de registros a partir de la tabla solicitada*/
	t_list* data = getData(comando.argumentos.INSERT.nombreTabla);

	/*Agrego el registro a dicha lista*/
	list_add(data, reg);

	printf("Registro->Timestamp= %llu\n", reg->timestamp);
	printf("Registro->Key= %d\n", reg->key);
	printf("Registro->Value= %s\n", reg->value);


	return EXIT_SUCCESS;
}


int createAPI(Comando comando){
	/*Checkeo la existencia de la tabla. De existir la misma loggeo un error*/
	if(existeTabla(comando.argumentos.CREATE.nombreTabla)){
		log_error(logger_invisible, "Existe una tabla asociada a la key solicitada.");
		//avisar a la memoria?
		return EXIT_FAILURE;
	}

	/*Obtengo la ruta del directorio donde van a estar los archivos de la tabla*/
	char* path = malloc(100 * sizeof(char));
	setPathTabla(path, comando.argumentos.CREATE.nombreTabla);

	/*Creo directorio para la tabla solicitada*/
	crearDirectorioTabla(path);

	/*Creo el archivo de metadata de la tabla*/
	crearArchivo(path, "Metadata");

	/*Escribo la metadata*/
	escribirArchivoMetadata(path, comando);

	/*Creo los archivos binarios vacíos*/
	crearArchivosBinarios(path, atoi(comando.argumentos.CREATE.numeroParticiones));

	/*Creo la Tabla en la Memtable*/
	crearTablaEnMemtable(comando.argumentos.CREATE.nombreTabla);

	/*Libero recursos*/
	free(path);

	return EXIT_SUCCESS;
}


void describeAPI(Comando comando){
}


void dropAPI(Comando comando){
}
/*FIN FUNCIONES API*/


/*INICIO FUNCIONES COMPLEMENTARIAS*/
bool existeTabla(char* nombreTabla){
	return dictionary_has_key(memtable, nombreTabla);
}


int getMetadata(char* nombreTabla, t_config* metadataFile){
	if(metadataFile == NULL){
		printf(RED"APILissandra.c: leerMetadata: error en el archivo 'Metadata' de la tabla %s"STD"\n", nombreTabla);
		return EXIT_FAILURE;
	}

	extraerMetadata(metadataFile);

	mostrarMetadata();//funcion adhoc para testing

	return EXIT_SUCCESS;
}


t_config* leerMetadata(char* nombreTabla){
	char* path = malloc(100 * sizeof(char));

	strcpy(path,config.punto_montaje);
	strcat(path, "Tables/");
	strcat(path, nombreTabla);
	strcat(path, "/");
	strcat(path, "Metadata");

	return config_create(path);
}


void extraerMetadata(t_config* metadataFile) {
	metadata.compaction_time = config_get_int_value(metadataFile, "COMPACTION_TIME");
	metadata.consistency = config_get_string_value(metadataFile, "CONSISTENCY");
	metadata.partitions= config_get_int_value(metadataFile, "PARTITIONS");
}


void mostrarMetadata(){
	log_info(logger_visible, "\nMetadata->compaction_time= %d\n", metadata.compaction_time);
	log_info(logger_visible, "Metadata->consistency= %s\n", metadata.consistency);
	log_info(logger_visible, "Metadata->partitions= %d\n", metadata.partitions);
}


t_list*	getData(char* nombreTabla){
	return dictionary_get(memtable, nombreTabla);
}


int calcularParticionNbr(char* key, int particiones){
	return atoi(key)%particiones;
}


t_list* buscarValue(t_list* data, char* key, int particionNbr){
	printf("Numero de partición: %d\n", particionNbr);

	bool compararConItem(void* item){
		if (atoi(key) != obtenerKey((Registro*) item)) {
			return false;
		}
		return true;
	}

	return list_filter(data, compararConItem);
}


void recorrerTabla(t_list* lista){
	Registro* reg= NULL;
	int i=0;

	while(!(list_get(lista, i)==NULL)){
		reg = list_get(lista, i);

		printf("Registro->Timestamp= %llu\n", reg->timestamp);
		printf("Registro->Key= %d\n", reg->key);
		printf("Registro->Value= %s\n", reg->value);

		i++;
	}
}


void getValueMasReciente(t_list* lista){

	bool compararFechas(void* item1, void* item2){
		if (obtenerTimestamp((Registro*) item1) < obtenerTimestamp((Registro*) item2)) {
			return false;
		}
		return true;
	}

	list_sort(lista, compararFechas);


	Registro* reg = malloc(sizeof(Registro));
	reg = list_get(lista, 0);

	printf("\nEl registro más reciente es:\n");
	printf("Registro->Timestamp= %llu\n",reg->timestamp );
	printf("Registro->Key= %d\n", reg->key);
	printf("Registro->Value= %s\n", reg->value);

	free(reg);

}


timestamp_t checkTimestamp(char* timestamp){
	if(timestamp==NULL){
		return getCurrentTime();
	}
	else{
		printf("time= %llu\n",atoll(timestamp));
		return atoll(timestamp);
	}
}


void crearTablaEnMemtable(char* nombreTabla){
	t_list* lista = list_create();
	char* tabla=nombreTabla;

	dictionary_put(memtable, tabla, lista);
}


void setPathTabla(char* path, char* nombreTabla){
	strcpy(path,config.punto_montaje);
	strcat(path, "Tables/");
	strcat(path, nombreTabla);
}

void crearDirectorioTabla(char* path){
	crearDirectorio(path);
	strcat(path,"/");
}


void crearArchivo(char* path, char* nombre){
	char* pathArchivo = malloc(110 * sizeof(char));
	strcpy(pathArchivo,path);
	strcat(pathArchivo, nombre);

	printf("archivo creado en: %s\n", pathArchivo);

	FILE* file=NULL;
	file = fopen(pathArchivo,"w");
	//free(file);
	fclose(file);
	free(pathArchivo);
}

void escribirArchivoMetadata(char* path, Comando comando){
	FILE* fmetadata;

	char* pathArchivo = malloc(110 * sizeof(char));

	strcpy(pathArchivo,path);
	strcat(pathArchivo, "Metadata");

	fmetadata = fopen(pathArchivo,"a");

	fprintf (fmetadata, "COMPACTION_TIME=%d\n",atoi(comando.argumentos.CREATE.compactacionTime));
	fprintf (fmetadata, "CONSISTENCY=%s\n",comando.argumentos.CREATE.tipoConsistencia);
	fprintf (fmetadata, "PARTITIONS=%d\n",atoi(comando.argumentos.CREATE.numeroParticiones));

	fclose(fmetadata);
	free(pathArchivo);
}

void crearArchivosBinarios(char* path, int particiones){
	char filename[8];
	for(int i=1;i<particiones+1;i++){
		sprintf(filename, "%d.bin", i);
		crearArchivo(path, filename);
	}
}

/*FIN FUNCIONES COMPLEMENTARIAS*/
