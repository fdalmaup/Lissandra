/*
 * Unidad_de_ejecucion.h
 *
 *  Created on: 8 may. 2019
 *      Author: utnso
 */

#ifndef UNIDAD_DE_EJECUCION_H_
#define UNIDAD_DE_EJECUCION_H_
#define TRUE 1
#define FALSE 0

	/*COMPORTAMIENTOS DE LA CPU*/
	/**
	* USAR_SOLO_MEMORIA_PRINCIPAL: Usa la memoria principal para atender todas las request sin importar si otras memorias
	* se sumen.
	*/
	#define USAR_SOLO_MEMORIA_PRINCIPAL TRUE

	/**
	* DELEGAR_A_MEMORIA_PRINCIPAL: Delega a la memoria principal todas las request que no hayan encontrado una memoria que las
	* ejecute. Por ejemplo si no hay memorias SC, entonces una request a una tabla SC seria ejecutada por la MP si esta
	* opcion esta activada.
	*/
	#define DELEGAR_A_MEMORIA_PRINCIPAL FALSE

#include <stdio.h>
#include <stdlib.h>
#include <commons/process.h>
#include <random/random_numbers.h>
#include "Planificador.h"
#include "Sistema_de_criterios.h"

//ESTRUCTURAS:
typedef struct pcb_t PCB;
typedef int socket_t;

typedef enum{
	DESALOJO,
	FINALIZO,
	INSTRUCCION_ERROR,
	CONTINUAR
}ResultadoEjecucionInterno;

//FUNCIONES: Publicas
void *exec(void *null);

#endif /* UNIDAD_DE_EJECUCION_H_ */
