/*
 * Planificador.c
 *
 *  Created on: 7 may. 2019
 *      Author: facundosalerno
 */

#include "Planificador.h"

int iniciar_planificador(){
	if(iniciar_unidades_de_ejecucion() == EXIT_FAILURE){
		printf(RED"Planificador.c: iniciar_planificador: no se pudieron iniciar las unidades de ejecucion"STD"\n");
		return EXIT_FAILURE;
	}
	colaDeReady = queue_create();
	sem_post(&disponibilidadPlanificador); //No queremos que la consola agregue algo a la cola de news si todavia no existe la cola de news

	if(ready() == EXIT_FAILURE){
		printf(RED"Planificador.c: iniciar_planificador: hubo un problema de ejecucion"STD"\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}






int new(PCB_DataType tipo, void *data){
	PCB *pcb = malloc(sizeof(PCB)); //TODO: no olvidarse que hay que liberarlo en algun momento
	pcb->quantumRemanente = config.quantum;
	pcb->data = data; //pcb->data almacena la direccion data
	switch(tipo){
	case COMANDO:
		pcb->tipo = COMANDO;printf("Comando encolado en new\n");
		break;
	case LQL:
		pcb->tipo = LQL;printf("LQL encolado en new\n");
		break;
	default:
		printf(RED"Planificador.c: new: no se reconoce el tipo de dato a ejecutar"STD"\n");
		return EXIT_FAILURE;
	}
	queue_push(colaDeReady, pcb); //TODO: IMPORTANTE esto no anda
	sem_post(&scriptEnReady); //Como esta funcion va a ser llamada por la consola, el semaforo tambien tiene que ser compartido por el proceso consola
	return EXIT_SUCCESS;
}






int ready(){
	while(1){
		sem_wait(&scriptEnReady);
		sem_wait(&unidadDeEjecucionDisponible);
		//TODO ready
		/*1. Espero a que haya algo para hacer
		 *2. Espero a que haya alguna cpu disponible
		 *3. Determino cual cpu esta disponible para darle una tarea. Puedo tener a todas las cpus disponibles bloqueadas en un semaforo y cuando les tiro signal de aca, una cpu cualquiera lo agarra
		 *4. Selecciono al siguiente a ejecutar y se lo paso a dicha cpu (Round Robin)
		 *5. Vuelvo a empezar
		 */
	}
	return EXIT_SUCCESS;
}






int iniciar_unidades_de_ejecucion(){
	sem_init(&unidadDeEjecucionDisponible, 0, 0);
	sem_init(&ordenDeEjecucion, 0, 0);
	idsExecInstances = list_create();
	for(int i=0; i<config.multiprocesamiento; ++i){
		pthread_t *id = malloc(sizeof(pthread_t)); //Lo hago asi por que los salames que hicieron la funcion list_add nada mas linkean el puntero, no le copian el valor. Por ende voy a necesitar un malloc de int por cada valor que quiera guardar, y no hacerles free de nada
		//TODO: todos estos mallocs de int se liberan supuestamente al finalizar el programa, pero no perderlo de vista xq podria haber algun error purulando
		int res = pthread_create(id, NULL, exec, NULL);
		if(res != 0){
			printf(RED"Kernel.c: iniciar_planificacion: fallo la creacion de un proceso"STD"\n");
			return EXIT_FAILURE;
		}
		list_add(idsExecInstances, id);
		sem_post(&unidadDeEjecucionDisponible);
	}
	return EXIT_SUCCESS;
}
/*
PCB seleccionar_siguiente(){
	return queue_pop(colaDeReady);
}*/
