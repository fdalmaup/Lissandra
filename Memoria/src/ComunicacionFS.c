/*
 * ComunicacionFS.c
 *
 *  Created on: 24 jun. 2019
 *      Author: fdalmaup
 */

#include "ComunicacionFS.h"

int handshakeLFS(int socketLFS) {
	Operacion handshake;

	handshake.TipoDeMensaje= HANDSHAKE;

	handshake.Argumentos.HANDSHAKE.informacion= string_from_format("handshake");

	send_msg(socketLFS, handshake);

	destruir_operacion(handshake);

	//Recibo el tamanio
	handshake = recv_msg(socketLFS);

	switch(handshake.TipoDeMensaje){
		case HANDSHAKE:
			tamanioValue=atoi(handshake.Argumentos.HANDSHAKE.informacion);
			destruir_operacion(handshake);
			break;
		default:
			return EXIT_FAILURE;
	}


	//Pido el punto de montaje
	handshake.TipoDeMensaje= HANDSHAKE;
	handshake.Argumentos.HANDSHAKE.informacion=string_from_format("handshake pathLFS");

	send_msg(socketLFS, handshake);

	destruir_operacion(handshake);

	//Recibo el punto de montaje
	handshake = recv_msg(socketLFS);

	//usleep(vconfig.retardoFS * 1000);

	switch(handshake.TipoDeMensaje){
			case HANDSHAKE:
				pathLFS=string_from_format(handshake.Argumentos.HANDSHAKE.informacion);
				destruir_operacion(handshake);
				break;
			default:
				return EXIT_FAILURE;
		}

	log_info(logger_visible, "El size del value es: %d", tamanioValue);
	log_info(logger_visible, "El punto de montaje es: %s",pathLFS);

	return EXIT_SUCCESS;
}

int conectarLFS() {
	//Obtiene el socket por el cual se va a conectar al LFS como cliente / * Conectarse al proceso File System
	int socket = connect_to_server(fconfig.ip_fileSystem, fconfig.puerto_fileSystem);
	if (socket == EXIT_FAILURE) {
		log_error(logger_error,"ComunicacionFS: conectarLFS: Error de comunicacion. El LFS no está levantado.");
		return EXIT_FAILURE;
	}
	return socket;
}

//Para API

Operacion comunicarseConLFS(char * input){
	Operacion retorno;

	int lfsSocket = conectarLFS();

	if(lfsSocket == EXIT_FAILURE){
		retorno.TipoDeMensaje=ERROR;
		retorno.Argumentos.ERROR.mensajeError=string_from_format("ComunicacionFS-> comunicarseConLFS: no se pudo comunicar con el LFS");
		return retorno;
	}

	enviarRequestFS(input, lfsSocket);

	retorno=recibirRequestFS(lfsSocket);

	close(lfsSocket);

	return retorno;
}

void enviarRequestFS(char* input, int lfsSocket) {

	Operacion request;

	request.TipoDeMensaje = COMANDO;

	request.Argumentos.COMANDO.comandoParseable = string_from_format(input);

	send_msg(lfsSocket, request);

	destruir_operacion(request);
}

Operacion recibirRequestFS(int lfsSocket) {
	Operacion resultado;
	resultado = recv_msg(lfsSocket);

	usleep(vconfig.retardoFS* 1000);

	return resultado;
}


