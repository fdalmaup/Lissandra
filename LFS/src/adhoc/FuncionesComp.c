/*
 * FuncionesComp.c
 *
 *  Created on: 24 jun. 2019
 *      Author: juanmaalt
 */

#include "FuncionesComp.h"

void cambiarNombreFilesTemp(char* pathTabla){
	char* pathFileViejo;
	char* pathFileNuevo;
	DIR *dir;
	struct dirent *entry;
	char* nombreArchivo;

	if((dir = opendir(pathTabla)) != NULL){
		while((entry = readdir (dir)) != NULL){
			nombreArchivo = string_from_format(entry->d_name);
			if(string_ends_with(nombreArchivo, ".tmp")){
				pathFileViejo = string_from_format("%s/%s", pathTabla, nombreArchivo);
				pathFileNuevo = string_duplicate(pathFileViejo);
				string_append(&pathFileNuevo,"c");
				rename(pathFileViejo, pathFileNuevo);
				free(pathFileViejo);
				free(pathFileNuevo);
			}
			free(nombreArchivo);
		}
		closedir (dir);
	}
}

char* obtenerListaDeBloques(int particion, char* nombreTabla){
	char* pathFile = string_from_format("%sTables/%s/%d.bin", config.punto_montaje, nombreTabla, particion);

	t_config* particionFile = config_create(pathFile);
	if(particionFile == NULL)
		return NULL;
	char* listaDeBloques = string_from_format(config_get_string_value(particionFile, "BLOCKS"));
	config_destroy(particionFile);
	free(pathFile);
	return listaDeBloques;
}

char* obtenerListaDeBloquesDump(char* pathDump){
	t_config* particionFile = config_create(pathDump);
	if(particionFile == NULL)
		return NULL;
	char* listaDeBloques = string_from_format(config_get_string_value(particionFile, "BLOCKS"));
	config_destroy(particionFile);
	return listaDeBloques;
}

char* firstBloqueDisponible(char* listaDeBloques){
	char** bloques = string_get_string_as_array(listaDeBloques);

	char* firstBloque=0;
	int i=0;

	while(bloques[i]!=NULL){
		int charsInFile = caracteresEnBloque(bloques[i]);
		//printf("Bloque %s con %d caracteres disponibles\n", bloques[i], (metadataFS.blockSize - charsInFile));
		if(charsInFile < metadataFS.blockSize){
			log_info(logger_invisible, "Compactador.c: firstBloqueDisponible() - Bloque %s con %d caracteres disponibles\n", bloques[i], (metadataFS.blockSize - charsInFile));
			firstBloque=bloques[i];
			return firstBloque;
		}
		else{
			log_info(logger_invisible, "Compactador.c: firstBloqueDisponible() - Bloque %s sin caracteres disponibles\n", bloques[i]);
			firstBloque="0";
		}
		i++;
	}
	string_iterate_lines(bloques, (void* )free);
	free(bloques);
	return firstBloque;
}

int caracteresEnBloque(char* bloque){
	if(atoi(bloque)>metadataFS.blocks){
		log_error(logger_visible, "Error a leer los caracteres del bloque '%s'", bloque);
		return metadataFS.blockSize;
	}
	char* pathBloque = string_from_format("%sBloques/%s.bin", config.punto_montaje, bloque);
	//printf("path: %s\n", pathBloque);

	FILE* fBloque = fopen(pathBloque, "r+");
	if(fBloque==NULL){
		log_error(logger_visible, "Error a leer los caracteres del bloque '%s'", bloque);
		free(pathBloque);
		return metadataFS.blockSize;
	}
	char ch;
	int count=0;

	while((ch=fgetc(fBloque))!=EOF){
		count++;
	}
	//printf("la cantidad de caracteres en %s.bin es %d\n", bloque, count);
	fclose(fBloque);
	free(pathBloque);
	return count;

}


void escribirEnBloque(char* bloque, char* linea){
	//printf("bloque que recibe escribirEnBloque: %s\n", bloque);
	char* pathBloque = string_from_format("%sBloques/%s.bin", config.punto_montaje, bloque);

	log_info(logger_invisible, "Compactador.c: escribirEnBloque() - Path del Bloque a escribir: %s", pathBloque);
	//printf("pathBloque - escribirEnBloque: %s\n", pathBloque);

	FILE* fBloque = fopen(pathBloque, "a");

	fprintf (fBloque, "%s",linea);

	log_info(logger_invisible, "Compactador.c: escribirEnBloque() - L??nea a escribir: %s", linea);

	fclose(fBloque);
	free(pathBloque);
}

/*
void escribirLinea(char* bloque, char* linea, char* nombreTabla, int particion){
	//printf("bloque recibido: %s\n", bloque);
	if(string_equals_ignore_case(bloque, "0")){
		char* bloqueLibre=NULL;
		if((bloqueLibre = getBloqueLibre())==NULL){
			log_error(logger_visible,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
			log_error(logger_invisible,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
			log_error(logger_error,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
			return;
		}

		bloque = string_from_format("%s",bloqueLibre);
		if(bloqueLibre!=NULL){
			free(bloqueLibre);
		}
	}
	log_info(logger_invisible, "Compactador.c: escribirLinea() - Bloque a escribir: %s", bloque);
	//printf("bloque elegido: %s\n", bloque);

	int charsDisponibles = metadataFS.blockSize-caracteresEnBloque(bloque);
	char* nuevoBloque;
	char* subLinea=string_new();
	subLinea=NULL;

	//printf("espacio en Bloque '%s': %d\n", bloque,charsDisponibles);

	if(charsDisponibles>0){
		if(charsDisponibles>=strlen(linea)){
			escribirEnBloque(bloque, linea);
			agregarBloqueEnParticion(bloque, nombreTabla, particion);
			//printf("A> hay espacio en bloque y linea menor al espacio: linea escrita: %s\n", linea);
			//printf("A> bloque para la linea: %s\n", bloque);
		}else{
			while(strlen(linea)!=0){
				if(strlen(linea)>charsDisponibles){
					subLinea = string_substring_until(linea, charsDisponibles);
				}else{
					subLinea = string_substring_until(linea, (strlen(linea)-1));
				}
				//printf("B> hay espacio en bloque y linea mayor al espacio: linea escrita: %s\n", subLinea);
				//printf("B> nuevo bloque para la sublinea: %s\n", bloque);
				escribirEnBloque(bloque, subLinea);
				if(strlen(linea)>charsDisponibles){
					linea = string_substring_from(linea, charsDisponibles);
					//printf("B> hay espacio en bloque y linea mayor al espacio: resto: %s\n", linea);
					if((nuevoBloque = getBloqueLibre())==NULL){
						log_error(logger_visible,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
						log_error(logger_invisible,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
						log_error(logger_error,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
						return;
					}
					bloque=nuevoBloque;
					free(nuevoBloque);
					agregarBloqueEnParticion(bloque, nombreTabla, particion);
					//printf("B> nuevo bloque para la sublinea: %s\n", bloque);
				}else{linea="";}
			//charsDisponibles = getMin((metadataFS.blockSize-caracteresEnBloque(bloque)), strlen(linea));
				//printf("B> actualizo char disponibles: %d\n", charsDisponibles);
			}
			//printf("B> sali??\n");
		}
	}else{
		if(strlen(linea)<metadataFS.blockSize){
			if((nuevoBloque = getBloqueLibre())==NULL){
				log_error(logger_visible,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
				log_error(logger_invisible,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
				log_error(logger_error,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
				return;
			}
			bloque=nuevoBloque;
			free(nuevoBloque);
			escribirEnBloque(bloque, linea);
			agregarBloqueEnParticion(bloque, nombreTabla, particion);
			//printf("C> No hay espacio en bloque y linea menor al espacio: linea escrita: %s\n", linea);
			//printf("C> nuevo bloque para la linea: %s\n", bloque);
		}else{
			while(strlen(linea)!=0){
				if((nuevoBloque = getBloqueLibre())==NULL){
					log_error(logger_visible,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
					log_error(logger_invisible,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
					log_error(logger_error,"FuncionesComp.c: escribirLinea() - No hay Bloques libres, no se puede guardar la informaci??n");
					return;
				}
				subLinea = string_substring_until(linea, metadataFS.blockSize);
				//printf("D> No hay espacio en bloque y linea menor al espacio: linea escrita: %s\n", subLinea);
				//printf("D> nuevo bloque para la linea: %s\n", bloque);
				bloque=nuevoBloque;
				free(nuevoBloque);
				escribirEnBloque(bloque, subLinea);
				agregarBloqueEnParticion(bloque, nombreTabla, particion);
				linea = string_substring_from(linea, metadataFS.blockSize);
				//printf("D> No queda espacio en bloque y pido nuevo bloque: resto: %s\n", linea);
			}
		}
	}
	free(subLinea);
}
*/


bool esRegistroMasReciente(timestamp_t timestamp, int key, char* listaDeBloques){
	Registro* reciente;

	reciente = fseekBloque(key, listaDeBloques);
	//printf("timestamp recibido:%llu\ntimestamp encontrado:%llu\n", timestamp, reciente->timestamp);

	return (timestamp > reciente->timestamp);
}

int getMin(int value1, int value2){
	if(value1<value2)
		return value1;
	return value2;
}

char **generarRegistroBloque(t_list *registros){
	//1: genero una cadena que contenga todos los registros de la lista
	char *stringsDeRegistros = string_new();
	char *registroString(Registro *reg){
		return string_from_format("%llu;%d;%s\n", reg->timestamp, reg->key, reg->value);
	}
	void generarString(void *registro){
		char *stringRegAux = registroString((Registro*)registro);
		string_append(&stringsDeRegistros, stringRegAux);
		free(stringRegAux);
	}
	list_iterate(registros, generarString);
	//2: empiezo a cortar la cadena en pedazos de longitud sizeBloque y las voy agregando a la matriz de retorno
	int pos = 1;
	char **retorno = malloc(sizeof(char*)*pos);
	retorno[pos-1] = NULL;
	void iterarString(char *c){
		if(retorno[pos-1] == NULL)
			retorno[pos-1] = string_new();
		string_append(&retorno[pos-1], c);
		if(strlen(retorno[pos-1]) == metadataFS.blockSize){
			++pos;
			retorno = realloc(retorno, sizeof(char*)*pos);
			retorno[pos-1] = NULL;
		}
	}
	simple_string_iterate(stringsDeRegistros, iterarString);
	if(retorno[pos-1] != NULL){
		++pos;
		retorno = realloc(retorno, sizeof(char*)*pos);
		retorno[pos-1] = NULL;
	}
	free(stringsDeRegistros);
	return retorno;
}


void borrarArchivosTmpc(char* nombreTabla){
	char* pathTabla = string_from_format("%sTables/%s", config.punto_montaje, nombreTabla);

	bool es_tmpc(EntradaDirectorio *entrada){
		return string_ends_with(entrada->d_name, ".tmpc");
	}
	void borrar_tmpc(EntradaDirectorio *entrada){
		char* pathArchivoTMPC = string_from_format("%s/%s", pathTabla, entrada->d_name);
		remove(pathArchivoTMPC);
		free(pathArchivoTMPC);
	}
	directory_iterate_if(pathTabla, es_tmpc, borrar_tmpc);
	free(pathTabla);
}

bool esUnRegistro(char *timestamp, char *key, char *value){
	if(timestamp == NULL || key == NULL || value == NULL)
		return false;
	if(!esTimestamp(timestamp, false))
		return false;
	if(!esUint16_t(key, false))
		return false;
	if(!esValue(value, false))
		return false;
	if(!string_ends_with(value, "\n"))
		return false;
	return true;
}


