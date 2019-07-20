/*
 * Bitmap.c
 *
 *  Created on: 9 jul. 2019
 *      Author: juanmaalt
 */

#include "Bitmap.h"

void leerBitmap(){
	log_info(logger_invisible, "Inicio levantarBitmap");
	int size = ((metadataFS.blocks/8)+1);
	char* path = string_from_format("%sMetadata/Bitmap.bin", config.punto_montaje);

	//printf("path: %s\n", path);

	int fileDescriptor;
	char* bitmap;

	/*Abro el bitmap.bin, provisto o creado por la consola del fileSystem*/
	fileDescriptor = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	/*Trunco el archivo con la longitud de los bloques, para evitar problemas*/
	ftruncate(fileDescriptor, size);
	if(fileDescriptor == -1){
		log_error(logger_visible, "No se pudo abrir el archivo");
		return;
	}

		//printf("antes del mmap\n");
	/*Mapeo a la variable bitmap el contenido del fileDescriptor*/
	bitmap = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor, 0);

	/*Inicializo el bitmap ni bien se abre, si está vacío*/
	if(strlen(bitmap)==0){
		memset(bitmap,0,size);
	}

		//printf("antes del bitarray_create\nsize= %d\n", size);
	/*Creo el bitarray para poder manejar lo leído del bitmap*/
	bitarray = bitarray_create_with_mode(bitmap, size, MSB_FIRST);

		//printf("antes del msync\n");
	/*Sincronizo el archivo con los datos del bitarray*/
	msync(bitarray, size, MS_SYNC);

	//log_info(logger_visible, "El tamanio del bitmap es de %lu bits", tamanio);
	/*Libero el file que está en memoria*/
	munmap(bitarray,size);
	/*Leo los bloques con información y actualizo el Bitarray*/
	actualizarBitarray();

	/*Función para imprimir*/

	for(int i=0;i<metadataFS.blocks;i++){
		bool valor = bitarray_test_bit(bitarray, i);
		//printf("valor bit= %d\n", valor);
	}


	sem_init(&leyendoBitarray, 0, 1);

	close(fileDescriptor);
	free(path);
}

char* getBloqueLibre(){
	sem_wait(&leyendoBitarray);

	int pos=0;
	char* posicion;

	while(bitarray_test_bit(bitarray, pos)!=0){
		if(pos<metadataFS.blocks){
			pos++;
		}else{
			log_error(logger_visible,"Bitmap.c: getBloqueLibre() - No hay Bloques libres, no se puede guardar la información");
			log_error(logger_invisible,"Bitmap.c: getBloqueLibre() - No hay Bloques libres, no se puede guardar la información");
			log_error(logger_error,"Bitmap.c: getBloqueLibre() - No hay Bloques libres, no se puede guardar la información");
			return NULL;
		}
	}

	bitarray_set_bit(bitarray, pos);

	escribirBitmap();
	//posicion=string_from_format("%d", pos+1);

	//printf("posicion:%s\n", posicion);
	sem_post(&leyendoBitarray);

	return posicion;
}

void liberarBloque(int pos){
	if(bitarray_test_bit(bitarray, (pos-1))){
		bitarray_clean_bit(bitarray, (pos-1));
		escribirBitmap();
	}
}

void escribirBitmap(){
	char* pathBitmap = string_from_format("%sMetadata/Bitmap.bin",config.punto_montaje);

	FILE* bitmap;

	char binario[metadataFS.blocks+1];
	int cantSimbolos=((metadataFS.blocks/8)+1);
	char* texto = calloc(cantSimbolos, sizeof(char));

	for(int i=0;i<metadataFS.blocks;i++){
		if(bitarray_test_bit(bitarray, i)!=0){
			binario[i]='1';
		}else{
			binario[i]='0';
		}
		//printf("El char a transformar es: %c\n", binario[i]);
	}
	binario[metadataFS.blocks]='\0';

	//printf("El texto a transformar es: %s\n", binario);

	binarioATexto(binario, texto, cantSimbolos);

	bitmap = fopen(pathBitmap,"w");
/*
	for(int i=0; i<(metadataFS.blocks/8); i++){
		fprintf (bitmap, "%c",texto[i]);
		//printf("El texto que se guardaría en el bitmap.bin es: %c\n", texto[i]);
	}
	*/
	fprintf (bitmap, "%s",texto);
	//printf("texto: %s\n", texto);

	fclose(bitmap);
	free(texto);
	free(pathBitmap);
}


void binarioATexto(char* binario, char* texto, int cantSimbolos){
    for(int i = 0; i<metadataFS.blocks; i+=8, binario += 8){
        char* byte = binario;
        byte[8] = '\0';
        *texto++ = binarioADecimal(byte, 8);
    }
    texto -= cantSimbolos;
}

unsigned long binarioADecimal(char* binario, int length){
	int i;
	unsigned long decimal = 0;
	unsigned long weight = 1;
	binario += length - 1;
	weight = 1;
	for(i = 0; i < length; ++i, --binario){
		if(*binario == '1')
			decimal += weight;
		weight *= 2;
	}
	return decimal;
}

void actualizarBitarray(){
	char* pathBloques = string_from_format("%sBloques", config.punto_montaje);
	DIR *dir;
	struct dirent *entry;
	char* nombreBloque;

	if((dir = opendir(pathBloques)) != NULL){
		while((entry = readdir (dir)) != NULL){
			nombreBloque = string_from_format(entry->d_name);
			if(string_contains(nombreBloque, ".bin")){
				if(!string_contains(nombreBloque, ".binx")){
					char* bloque = string_substring_until(nombreBloque, (strlen(nombreBloque)-4));
					if(caracteresEnBloque(bloque)>0){
						bitarray_set_bit(bitarray, (atoi(bloque)-1));
						log_info(logger_invisible, "Bloque con data: %s.bin", nombreBloque);
					}
				}else{
					char* pathBinx = string_from_format("%sBloques/%s", config.punto_montaje, nombreBloque);
					remove(pathBinx);
					free(pathBinx);
				}
			}
			free(nombreBloque);
		}
		closedir (dir);
	}
	free(pathBloques);
}
