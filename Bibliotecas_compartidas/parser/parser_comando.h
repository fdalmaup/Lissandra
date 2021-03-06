/*
 * Copyright (C) 2018 Sistemas Operativos - UTN FRBA. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Adaptacion de 'parsi' del github de sisoputnfrba (https://github.com/sisoputnfrba/parsi) para los requisitos del tp 1c 2019
 */

#ifndef PARSER_H_
#define PARSER_H_
#define RANGO_MAX_UINT16_T 65535
#define RANGO_MIN_UINT16_T 0
#define RANGO_MAX_TIMESTAMP 18446744073709551615U
#define RANGO_MIN_TIMESTAMP 0

#include "../colores/colores.h"


	#include <stdlib.h>
	#include <stdio.h>
	#include <stdbool.h>
	#include <commons/string.h>
	#include <string.h>
	#include <ctype.h>

	typedef struct {
		bool valido;
		enum {
			SELECT,
			INSERT,
			CREATE,
			DESCRIBE,
			DROP,
			JOURNAL,
			ADDMEMORY,
			RUN,
			RUN_ALL,
			METRICS,
			METRICS_STOP
		} keyword;
		union {
			struct {
				char* nombreTabla;
				char* key;
			} SELECT;
			struct {
				char* nombreTabla;
				char* key;
				char* value;
				char* timestamp;
			} INSERT;
			struct {
				char* nombreTabla;
				char* tipoConsistencia;
				char* numeroParticiones;
				char* compactacionTime;
			} CREATE;
			struct {
				char* nombreTabla;
			} DESCRIBE;
			struct {
				char* nombreTabla;
			} DROP;
			struct {
			} JOURNAL;
			struct {
				char* numero;
				char* criterio;
			} ADDMEMORY;
			struct {
				char* path;
			} RUN;
			struct{
				char* dirPath;
			}RUN_ALL;
			struct {
			} METRICS;
			struct{
			}METRICS_STOP;
		} argumentos;
		char** _raw; //Para uso de la liberaci??n
	} __attribute__((packed)) Comando;

	/**
	* @NAME: parse
	* @DESC: interpreta una linea de un archivo ESI y
	*		 genera una estructura con el operador interpretado
	* @PARAMS:
	* 		line - Una linea de un archivo ESI
	*/
	Comando parsear_comando(char* line);

	/**
	* @NAME: destruir_operacion
	* @DESC: limpia la operacion generada por el parse
	* @PARAMS:
	* 		op - Una operacion obtenida del parse
	*/
	void destruir_comando(Comando op);

	/**
	* @NAME: mostrar
	* @DESC: muestra por la salida estandar un comando para testear el funcionamiento
	* @PARAMS:
	* 		comando - Una comando cualquiera
	*/
	void comando_mostrar(Comando comando);

	/**
	* @NAME: validar
	* @DESC: chequea la validez de un comando. Devuelve EXIT_SUCCESS si es valido, EXIT_FAILURE si no lo es.
	* 		 cuando se parsea un comando, este puede devolver, si lo hay, un error. Pero
	* 		 durante el proceso de parseo nosotros nunca nos enteramos, por eso antes de usar
	* 		 un comando parseado hay que preguntar por la validez del mismo. La funcion void mostrar()
	* 		 tambien pregunta la validez de esu comando para mostrarlo por pantalla. Esta funcion, solo
	* 		 se concentra en decir si es valido o no.
	* @PARAMS:
	* 		comando - Una comando cualquiera
	*/
	int comando_validar(Comando comando);

	/**
	* @NAME: remover_comillas
	* @DESC: remueve las comillas de una cadena pasada por parametro. Tiene efecto de lado.
	* 		 Para usarla pasarle la direccion de un puntero a char. Ejemplo char *cadena, le paso &cadena
	* 		 Si le pasas "hola" te la transforma a hola. Si le pasas hola no hace nada.
	* 		 Se puede usar con cualquier cadena, pero en el parser, hay que pasarle split[n]
	* @PARAMS:
	* 		comando - Una comando cualquiera
	*/
	void remover_comillas(char** cadena);

	/**
	* @NAME: remover_new_line
	* @DESC: devuelve una nueva cadena pero sin ningun \n
	*/
	char *remover_new_line(char* cadena);

	/**
	* @NAME: esAlfaNumerica
	* @DESC: esAlfaNumerica
	*/
	bool esAlfaNumerica(char* cadena, bool warning);

	/**
	* @NAME: esNumerica
	* @DESC: esNumerica
	*/
	bool esNumerica(char* cadena, bool warning);

	/**
	* @NAME: esConsistenciaValida
	* @DESC: esConsistenciaValida
	*/
	bool esConsistenciaValida(char* cadena, bool warning);

	/**
	* @NAME: esUint16_t
	* @DESC: esUint16_t
	*/
	bool esUint16_t(char* key, bool warning);

	/**
	* @NAME: esTimestamp
	* @DESC: esTimestamp
	*/
	bool esTimestamp(char* timestamp, bool warning);

	/**
	* @NAME: esValue
	* @DESC: esValue
	*/
	bool esValue(char* cadenan, bool warning);

	/**
	* @NAME: string_double_split
	* @DESC: separa una cadena en partes dado firstSeparator. Si encuentra secondSepartor, crea un nuevo token hasta la
	* 		 aparicion de otro secondSeparator y luego sigue usando firstSeparator. Por ejemplo, si firstSeparator es
	* 		 un espacio y secondSeparator es una comilla:
	* 		 input: INSERT ANIMALES 532 "PEPA PIG" 43151
	* 		 output: -INSERT
	* 		 		 -ANIMALES
	* 		 		 -532
	* 		 		 -"PEPA PIG"
	* 		 		 -43151
	* 		Nota: la funcion no esta muy pulida, y por algunas optimizaciones solo funciona bien en este parser.
	*/
	char **string_double_split(char *cadena, char *firstSeprator, char *secondSeparator);


#endif /* PARSER_H_ */
