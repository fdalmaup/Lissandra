/*
 * Gossiping.c
 *
 *  Created on: 17 jun. 2019
 *      Author: utnso
 */

#include "Gossiping.h"

int iniciar_gossiping() {

	pthread_mutex_init(&mutexGossiping, NULL);


//	char * ip_port_compresed =

//char * ip_port_compresed = string_from_format("%s:%s", fconfig.ip, fconfig.puerto);
//printf("IP propia : %s\n", ip_port_compresed);
	quitarCaracteresPpioFin(fconfig.ip_seeds);
	IPs = string_split(fconfig.ip_seeds, ",");

	quitarCaracteresPpioFin(fconfig.puerto_seeds);
	IPsPorts = string_split(fconfig.puerto_seeds, ",");
	listaMemoriasConocidas = list_create();

	//Reservo espacio para un knownMemory_t
	knownMemory_t * mem = malloc(
			sizeof(knownMemory_t)/*sizeof(int)+ sizeof(ip_port_compresed)+1*/);
	//Asigno a sus atributos los valores correspondientes
	mem->memory_number = atoi(fconfig.numero_memoria);
	mem->ip = string_from_format(fconfig.ip);
	mem->ip_port = string_from_format(fconfig.puerto);
	pthread_mutex_lock(&mutexGossiping);
	list_add(listaMemoriasConocidas, (knownMemory_t *) mem);
	pthread_mutex_unlock(&mutexGossiping);
	for (int i = 0; IPs[i] != NULL; ++i)	//Muestro por pantalla las IP seeds
		log_info(logger_gossiping,
				"GOSSIPING.C: iniciar_gossiping: IP SEED %d: %s:%s:", i, IPs[i],
				IPsPorts[i]);

	if (pthread_create(&idGossipSend, NULL, conectar_seeds, NULL)) {

		log_error(logger_error,
				"GOSSIPING.C: iniciar_gossiping: fallo la creacion hilo gossiping");
		return EXIT_FAILURE;
	}

	//if(pthread_create(&idConsola, NULL, recibir_comandos, NULL)){
	/*if (pthread_create(&idGossipReciv, NULL, recibir_seeds, NULL)) {
	 printf(
	 RED"Memoria.c: iniciar_gossiping: fallo la creacion hilo gossiping escucha"STD"\n");
	 return EXIT_FAILURE;
	 }
	 */

	return EXIT_SUCCESS;
}

void *conectar_seeds(void *null) { // hilo envia a las seeds
	pthread_detach(pthread_self());
	//conectarConSeed();
	// puertoSocket = ConsultoPorMemoriasConocidas(puertoSocket);
	//liberarIPs(IPs);
	//liberarIPs(IPsPorts);
	for (;;) {
		log_info(logger_invisible,
				"GOSSIPING.C: conectar_seeds: inicio gossiping");
		log_info(logger_gossiping,
				"GOSSIPING.C: conectar_seeds: inicio gossiping");
		conectarConSeed();
		usleep(vconfig.retardoGossiping * 1000);
		// Envia mensaje a las seeds que conoce
	}
	return NULL;
}

void quitarCaracteresPpioFin(char* cadena) {
	//char * temporal = malloc(sizeof(char) * (strlen(cadena) - 1)); //Me sobran 2 de comillas (-2) y +1 para el '\0'
	int i;

	for (i = 0; i < strlen(cadena) + 1; ++i) {
		cadena[i] = cadena[i + 1];
	}
	cadena[strlen(cadena) - 1] = '\0';

}

void liberarIPs(char** IPs) {
	if (IPs != NULL) {
		for (int i = 0; IPs[i] != NULL; ++i)
			free(*(IPs + i));
		free(IPs);
	}
}

int levantarSocketSeed(char * IP, char * puerto){
	return  connect_to_server(IP,puerto);

}

void removerDeListaDeConocidas(char * IP,char * puerto){
	bool buscarMemoria(void * buscoMemoria) {
		int comparoPort = strcmp(puerto,
				((knownMemory_t*) buscoMemoria)->ip_port);
		int comparoIP = strcmp(IP,
				((knownMemory_t*) buscoMemoria)->ip);
		//return ((IPs[conteo_seeds] == ((knownMemory_t*)buscoMemoria)->ip) && (IPsPorts[conteo_seeds] == ((knownMemory_t*)buscoMemoria)->ip_port));
		//printf("Buscar primeri %s  BUSCAR SEGUNDO %s\n",puerto,((knownMemory_t*)buscoMemoria)->ip_port);

		if ((comparoPort * comparoPort + comparoIP * comparoIP) == 0) // Como me puede dar negativo, saco los modulos. En el caso de que la IP y el puerto sean iguales devuelve 0 la suma
			return 1; //Tengo que ir a destruirMemoria
		else
			return 0;	// No hago nada
	}
	void destruirMemoria(void *destruir) {
		//printf(RED"DESTRUYO MEMORIA %d %s:%s"STD"\n",((knownMemory_t*) destruir)->memory_number,((knownMemory_t*) destruir)->ip,((knownMemory_t*) destruir)->ip_port);
		free(((knownMemory_t*) destruir)->ip);
		free(((knownMemory_t*) destruir)->ip_port);
		free(((knownMemory_t*) destruir));
		//printf("Destrui Memoria\n");
		return;
	}
	pthread_mutex_lock(&mutexGossiping);
	list_remove_and_destroy_by_condition(listaMemoriasConocidas,
		buscarMemoria, destruirMemoria);
}
void conectarConSeed() {
	// Se conecta con la seed para hacer el gossiping
	int conteo_seeds = 0; //Static

	int socketGossip;

	//printf("CONECTARCONSEED\n");

	for (; IPs[conteo_seeds] != NULL; conteo_seeds++) {


		if((socketGossip = levantarSocketSeed(IPs[conteo_seeds],IPsPorts[conteo_seeds])) == EXIT_FAILURE){
			//printf(YEL"IMPRIMO SOCKET %d\n IP PUERTO %s:%s"STD"\n",socketGossip,IPs[conteo_seeds],IPsPorts[conteo_seeds]);
			log_info(logger_gossiping, "GOSSIPING.C:conectarConSeed: La memoria seed no esta activa %s:%s", IPs[conteo_seeds], IPsPorts[conteo_seeds]);

			removerDeListaDeConocidas(IPs[conteo_seeds],IPsPorts[conteo_seeds]);

			chequeo_memorias_en_lista_activas();

			pthread_mutex_unlock(&mutexGossiping);
			//printf("TAMANIO LISTA CONOCIDAS %d\n",list_size(listaMemoriasConocidas));

		} else {

			//La memoria seed esta activa, voy a consultarle sus memorias
			log_info(logger_gossiping,
					"GOSSIPING.C:conectarConSeed: Memoria conocida. Enviar mensaje %s:%s ",
					IPs[conteo_seeds], IPsPorts[conteo_seeds]);

			ConsultoPorMemoriasConocidas(socketGossip); //
			log_info(logger_gossiping,"TAMANIO LISTA CONOCIDAS %d\n",list_size(listaMemoriasConocidas));
			//close(socket);
		}

	}
}

void liberarMemoriasConocidas(void* MemoryAdestruir) {
	if ((t_list *) MemoryAdestruir != NULL) {
		free(((knownMemory_t*) MemoryAdestruir)->ip);
		free(((knownMemory_t*) MemoryAdestruir)->ip_port);
		free(MemoryAdestruir);
	}


}

void ConsultoPorMemoriasConocidas(int socketSEEDS) {

	Operacion request;
	char * envio = NULL;
	knownMemory_t * recupero;
	//printf("Armo paquete\n");
	pthread_mutex_lock(&mutexGossiping);
	for (int i = 0; list_size(listaMemoriasConocidas) > i; i++) {
		//printf("Entro en lista\n");
		recupero = (knownMemory_t *) list_get(listaMemoriasConocidas, i);

		concatenar_memoria(&envio,
				string_from_format("%d", recupero->memory_number), recupero->ip,
				recupero->ip_port);
	}
	//concatenar_memoria(&envio,fconfig.numero_memoria  ,fconfig.ip , fconfig.puerto);

	//printf("Mensaje corrido: %s \n",envio);
	request.TipoDeMensaje = GOSSIPING_REQUEST;
	//printf("Paquete armado\n");
	log_info(logger_gossiping,
			"GOSSIPING.C:ConsultoPorMemoriasConocidas: Envio gossiping: %s",
			envio);
	request.Argumentos.GOSSIPING_REQUEST.resultado_comprimido = string_from_format("%s", envio);
	free(envio);
	send_msg(socketSEEDS, request);
	pthread_mutex_unlock(&mutexGossiping);
	free(request.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
	//pthread_mutex_unlock(&mutexGossiping);
	//printf("Envio %d\n",request.TipoDeMensaje);

	request = recv_msg(socketSEEDS);
	log_info(logger_gossiping,
			"GOSSIPING.C:ConsultoPorMemoriasConocidas: Respuesta gossiping: %s",
			request.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
	//printf("Respuesta\n");

	t_list *aux = list_create();
	/*
	 recibo lista.
	 elemento no machea => agrego en aux (es nuevo)
	 elemento machea => voy a la lista y la quito. agrego en aux (es viejo)
	 Al final los que quedan en la lista vieja son las bajas
	 */
	knownMemory_t * mem = malloc(sizeof(knownMemory_t));
		//Asigno a sus atributos los valores correspondientes
		mem->memory_number = atoi(fconfig.numero_memoria);
		mem->ip = string_from_format(fconfig.ip);
		mem->ip_port = string_from_format(fconfig.puerto);
		list_add(aux, mem);
	char **descompresion = descomprimir_memoria(
			request.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
	//printf("Mensaje corrido recibido: %s \n",request.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
	//pthread_mutex_lock(&mutexGossiping);
	pthread_mutex_lock(&mutexGossiping);
	for (int i = 0; descompresion[i] != NULL; i += 3) {

		knownMemory_t *memoria;
		if ((memoria = machearMemoria(atoi(descompresion[i]))) == NULL) {
			//NO MACHEA, agrego a la lista aux
			knownMemory_t *memoria = malloc(sizeof(knownMemory_t));
			memoria->memory_number = atoi(descompresion[i]);
			memoria->ip = string_from_format(descompresion[i + 1]);
			memoria->ip_port = string_from_format(descompresion[i + 2]);
			list_add(aux, memoria);
		} else {
			// Las que machean, las saco de la lista de conocidas y lo agrego a la lista aux
			knownMemory_t *memoria = malloc(sizeof(knownMemory_t));
			memoria->memory_number = atoi(descompresion[i]);
			memoria->ip = string_from_format(descompresion[i + 1]);
			memoria->ip_port = string_from_format(descompresion[i + 2]);

			int cmpIP = strcmp(mem->ip, memoria->ip);
			int cmpIPPORT = strcmp(mem->ip_port,memoria->ip_port);
				if ((cmpIP * cmpIP + cmpIPPORT * cmpIPPORT) != 0)
					list_add(aux, memoria);
				else {
					free(memoria->ip);
					free(memoria->ip_port);
					free(memoria);
				}
		}
	}
	destruir_split_memorias(descompresion);
	list_destroy_and_destroy_elements(listaMemoriasConocidas,liberarMemoriasConocidas);
	//list_destroy(listaMemoriasConocidas); //Libero las referencias de la lista, sin liberar cada uno de sus elementos. Es decir, libero solo los nodos
	listaMemoriasConocidas = list_duplicate(aux); //Duplico la lista auxiliar con todos los elementos del nuevo describe, manteniendo los del anterior describe (son sus respecrtivos atributos de criterios), y eliminando los viejos (ya que nunca se agregaron a la listaAuxiliar)
	//pthread_mutex_unlock(&mutexGossiping);
	list_destroy(aux);

	log_info(logger_invisible,
			"GOSSIPING.C:ConsultoPorMemoriasConocidas:Fin GOSSIP");
	log_info(logger_gossiping,
			"GOSSIPING.C:ConsultoPorMemoriasConocidas:Fin GOSSIP");
	pthread_mutex_unlock(&mutexGossiping);
	free(request.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
	pthread_mutex_unlock(&mutexGossiping);
	//free(request);
	close(socketSEEDS);

}



Operacion recibir_gossiping(Operacion resultado) {
	knownMemory_t * recupero;
	char * envio = NULL;
	log_info(logger_gossiping,
			"GOSSIPING.C:recibir_gossiping: ENTRO FUNCION RECIBIR GOSSIPING");
	//pthread_mutex_lock(&mutexGossiping);
	if (resultado.TipoDeMensaje == GOSSIPING_REQUEST) {	// si es gossping request, proceso las memorias que me envian
		t_list *aux = list_create();
		t_list *aux_filtro = list_create();
		//pthread_mutex_lock(&mutexGossiping);
		log_info(logger_gossiping,
				"GOSSIPING.C:recibir_gossiping: Mensaje recibido %s",resultado.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
		//log_info(logger_gossiping,
				//"GOSSIPING.C:recibir_gossiping: Recibo mensaje gossiping: %s",
				//resultado.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
		char **descompresion = descomprimir_memoria(
				resultado.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
		//pthread_mutex_lock(&mutexGossiping);
		list_add_all(aux, listaMemoriasConocidas);
		pthread_mutex_lock(&mutexGossiping);
		for (int i = 0; descompresion[i] != NULL; i += 3) {
			knownMemory_t *memoria;
			if ((memoria = machearMemoria(atoi(descompresion[i]))) == NULL) {
				//printf("NO MACHEA\n");

				knownMemory_t *memoria = malloc(sizeof(knownMemory_t));
				memoria->memory_number = atoi(descompresion[i]);
				memoria->ip = string_from_format(descompresion[i + 1]);
				memoria->ip_port = string_from_format(descompresion[i + 2]);
				//printf("PUERTO SOCKET %s:%s\n",descompresion[i + 1], descompresion[i + 2]);
				int socketNew = connect_to_server(descompresion[i + 1],
						descompresion[i + 2]);
				//printf("SOCKET NEW : %d\n",socketNew);
				//printf(RED"RECIBI GOSSIP Y GENERO SOCKET A LAS MEMORIA QUE ME PASAN %d" STD "\n",socketNew);
				if (socketNew == EXIT_FAILURE) {
					//printf("FALLO SOCKET\n");
					log_info(logger_gossiping,
							"GOSSIPING.C:recibir_gossiping: La memoria no esta activa %s:%s",
							descompresion[i + 1], descompresion[i + 2]);

					//printf(RED"MEMORIA QUE ME PASARON NO ESTA ACTIVA %s:%s" STD "\n",
					//							descompresion[i + 1], descompresion[i + 2]);
					//printf("No activa\n");

					// Debo quitar de la lista esta memoria ya que no esta
					for (int j = 0; list_size(listaMemoriasConocidas) > j; j++) {
						//printf("Entro a filtar para quitar de lista\n");
						recupero = (knownMemory_t *) list_remove(listaMemoriasConocidas, j);
						int cmpIP = strcmp(recupero->ip, descompresion[i + 1]);
						int cmpIPPORT = strcmp(recupero->ip_port,
								descompresion[i + 2]);
						if ((cmpIP * cmpIP + cmpIPPORT * cmpIPPORT) != 0) {
							list_add(aux_filtro, recupero);
						} else {
							free(((knownMemory_t*) recupero)->ip);
							free(((knownMemory_t*) recupero)->ip_port);
							free(recupero);
						}
					}

					list_destroy(aux); //Libero las referencias de la lista, sin liberar cada uno de sus elementos. Es decir, libero solo los nodos
					aux = list_duplicate(aux_filtro); //Duplico la lista auxiliar con todos los elementos del nuevo describe, manteniendo los del anterior describe (son sus respecrtivos atributos de criterios), y eliminando los viejos (ya que nunca se agregaron a la listaAuxiliar)
					list_destroy(aux_filtro);

				} else {
					list_add(aux, memoria);
					//printf("AGREGO EN LISTA cierro socket\n %s\n%s\n%s\n",descompresion[i],descompresion[i+1],descompresion[i+2]);
					close(socketNew);
					//printf("Cierro Socket\n");
				}
				//printf("AGREGO EN LISTA\n %s\n%s\n%s\n",descompresion[i],descompresion[i+1],descompresion[i+2]);
				//list_add(aux, memoria);
			} // NO USO EL ELSE, si machea, ya la agregue antes en el add all
		}
		envio = NULL;
		destruir_split_memorias(descompresion);
		list_destroy(listaMemoriasConocidas); //Libero las referencias de la lista, sin liberar cada uno de sus elementos. Es decir, libero solo los nodos
		listaMemoriasConocidas = list_duplicate(aux); //Duplico la lista auxiliar con todos los elementos del nuevo describe, manteniendo los del anterior describe (son sus respecrtivos atributos de criterios), y eliminando los viejos (ya que nunca se agregaron a la listaAuxiliar)
		//pthread_mutex_unlock(&mutexGossiping);
		list_destroy(aux);
		free(resultado.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
		pthread_mutex_unlock(&mutexGossiping);
		// Ya agregue las memorias que me llegaron
		// Logica para enviar mi lista
		log_info(logger_gossiping,
				"GOSSIPING.C:recibir_gossiping: Lista de memorias actualizada");
	}

	// Preparo mensaje para enviar mis memorias conocidas
	//pthread_mutex_lock(&mutexGossiping);
	pthread_mutex_lock(&mutexGossiping);
	for (int i = 0; list_size(listaMemoriasConocidas) > i; i++) {
		//printf("Entro en lista\n");
		recupero = (knownMemory_t *) list_get(listaMemoriasConocidas, i);

		concatenar_memoria(&envio,
				string_from_format("%d", recupero->memory_number), recupero->ip,
				recupero->ip_port);
		//printf("CONCATENO MENSAJE : %s\n",envio);
	}
	resultado.TipoDeMensaje = GOSSIPING_REQUEST;
	log_info(logger_gossiping,
			"GOSSIPING.C:recibir_gossiping: Envio mensaje gossiping %s", envio);
	resultado.Argumentos.GOSSIPING_REQUEST.resultado_comprimido = string_from_format("%s", envio);
	free(envio);
	pthread_mutex_unlock(&mutexGossiping);
	return resultado;
}

static knownMemory_t *machearMemoria(int numeroMemoria) {
	bool buscar(void *elemento) {
		return numeroMemoria == ((knownMemory_t*) elemento)->memory_number;
	}
	return (knownMemory_t*) list_find(listaMemoriasConocidas, buscar);
}


void chequeo_memorias_en_lista_activas (void) {
	int indexList = 0;
	int sizeList = list_size(listaMemoriasConocidas);
	//printf("TAMA??O DE LISTA; %d\n", sizeList);
				while ((sizeList > 0) && (indexList < sizeList)) {
					//printf(YEL"ENTRO AL WHILE\n"STD);
					knownMemory_t * memoriaLista;
					memoriaLista = (knownMemory_t *) list_get(
							listaMemoriasConocidas, indexList);
					int comparoPort = strcmp(fconfig.puerto,
							((knownMemory_t*) memoriaLista)->ip_port);
					int comparoIP = strcmp(fconfig.ip,
							((knownMemory_t*) memoriaLista)->ip);
					if ((comparoPort * comparoPort + comparoIP * comparoIP) == 0){
						indexList++;
				//	printf(GRN"Yo mismo, no hago nada"STD"\n");
					}
					else {
						// TIRO SOCKET, SI DA ERROR, lo tengo que limpiar de la lista
						//printf("VIENDO SI ESTA: %s ----- %s\n",((knownMemory_t*) memoriaLista)->ip,((knownMemory_t*) memoriaLista)->ip_port);
						int socketLista = connect_to_server(
								((knownMemory_t*) memoriaLista)->ip,
								((knownMemory_t*) memoriaLista)->ip_port);
						if (socketLista == EXIT_FAILURE) {
							list_remove(listaMemoriasConocidas, indexList);
							sizeList = list_size(listaMemoriasConocidas);
							//printf(YEL"SEED CAIDA Y OTRA MEMORIA TAMBIEN\n"STD);

						} else {
							// La memoria sigue activa, cierro el socket de chequeo
							close (socketLista);
						}

						indexList++;

					}

				}
}
