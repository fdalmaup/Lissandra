#include "serializacion.h"

int send_msg(int socket, Operacion operacion) {
	size_t total=0;
	void* content=NULL;
	int longCadena = 0;
	Comando comando; //solo se usa para un chequeo en case COMANDO

	switch (operacion.TipoDeMensaje) {

	case TEXTO_PLANO:
		longCadena = strlen(operacion.Argumentos.TEXTO_PLANO.texto);
		total = sizeof(int) + sizeof(int) + sizeof(char) * longCadena + sizeof(id); //Enum + cantidad caracteres cadena + cadena +  opCode
		content = malloc(total);
		memcpy(content, &(operacion.TipoDeMensaje), sizeof(int));
		memcpy(content+sizeof(int), &longCadena, sizeof(int));
		memcpy(content+2*sizeof(int), operacion.Argumentos.TEXTO_PLANO.texto, sizeof(char)*longCadena);
		memcpy(content+2*sizeof(int)+sizeof(char)*longCadena, &(operacion.opCode), sizeof(id));
		break;

	case COMANDO://Nota: no puedo hacerlo en un paso tipo funcional
		comando = parsear_comando(operacion.Argumentos.COMANDO.comandoParseable);
		if (comando_validar(comando) == EXIT_FAILURE) {
			printf(RED"serializacion.c: send_command: el comando no es parseable"STD"\n");
			destruir_comando(comando);
			return EXIT_FAILURE;
		}
		destruir_comando(comando);
		longCadena = strlen(operacion.Argumentos.COMANDO.comandoParseable);
		total = sizeof(int) + sizeof(int) + sizeof(char) * longCadena + sizeof(id);
		content = malloc(total);
		memcpy(content, &(operacion.TipoDeMensaje), sizeof(int));
		memcpy(content+sizeof(int), &longCadena, sizeof(int));
		memcpy(content+2*sizeof(int),operacion.Argumentos.COMANDO.comandoParseable, sizeof(char)*longCadena);
		memcpy(content+2*sizeof(int)+sizeof(char)*longCadena, &(operacion.opCode), sizeof(id));
		break;

	case REGISTRO:
		longCadena = strlen(operacion.Argumentos.REGISTRO.value);
		size_t tamValue = sizeof(char) * longCadena;
		total = sizeof(int) + sizeof(timestamp_t) + sizeof(uint16_t) + sizeof(int) + tamValue + sizeof(id);
		content = malloc(total);
		memcpy(content, &(operacion.TipoDeMensaje), sizeof(int));
		memcpy(content+sizeof(int), &(operacion.Argumentos.REGISTRO.timestamp), sizeof(timestamp_t));
		memcpy(content+sizeof(int)+sizeof(timestamp_t), &(operacion.Argumentos.REGISTRO.key), sizeof(uint16_t));
		memcpy(content+sizeof(int)+sizeof(timestamp_t) + sizeof(uint16_t), &longCadena, sizeof(int));
		memcpy(content+sizeof(int)+sizeof(timestamp_t) + sizeof(uint16_t) + sizeof(int), operacion.Argumentos.REGISTRO.value, tamValue);
		memcpy(content+sizeof(int)+sizeof(timestamp_t) + sizeof(uint16_t) + sizeof(int) + tamValue, &(operacion.opCode), sizeof(id));
		break;

	case ERROR:
		longCadena = strlen(operacion.Argumentos.ERROR.mensajeError);
		total = sizeof(int) + sizeof(int) + sizeof(char) * longCadena + sizeof(id);
		content = malloc(total);
		memcpy(content, &(operacion.TipoDeMensaje), sizeof(int));
		memcpy(content+sizeof(int), &longCadena, sizeof(int));
		memcpy(content+2*sizeof(int), operacion.Argumentos.ERROR.mensajeError, sizeof(char)*longCadena);
		memcpy(content+2*sizeof(int)+sizeof(char)*longCadena, &(operacion.opCode), sizeof(id));
		break;

	case GOSSIPING_REQUEST:
		longCadena = strlen(operacion.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
		total = sizeof(int) + sizeof(int) +sizeof(char) * longCadena + sizeof(id);
		content = malloc(total);
		memcpy(content, &(operacion.TipoDeMensaje), sizeof(int));
		memcpy(content+sizeof(int), &longCadena, sizeof(int));
		memcpy(content+2*sizeof(int), operacion.Argumentos.GOSSIPING_REQUEST.resultado_comprimido, sizeof(char)*longCadena);
		memcpy(content+2*sizeof(int)+sizeof(char)*longCadena, &(operacion.opCode), sizeof(id));
		break;
	case GOSSIPING_REQUEST_KERNEL:
		total = sizeof(int);
		content=malloc(total);
		memcpy(content, &(operacion.TipoDeMensaje), sizeof(int));
		break;
	case DESCRIBE_REQUEST:
		longCadena = strlen(operacion.Argumentos.DESCRIBE_REQUEST.resultado_comprimido);
		total = sizeof(int) + sizeof(int) + sizeof(char) * longCadena + sizeof(id);
		content = malloc(total);
		memcpy(content, &(operacion.TipoDeMensaje), sizeof(int));
		memcpy(content+sizeof(int), &longCadena, sizeof(int));
		memcpy(content+2*sizeof(int), operacion.Argumentos.DESCRIBE_REQUEST.resultado_comprimido, sizeof(char)*longCadena);
		memcpy(content+2*sizeof(int)+sizeof(char)*longCadena, &(operacion.opCode), sizeof(id));
		break;
	default:
		return EXIT_FAILURE;
	}

	int result = send(socket, content, total, 0);
	if (content != NULL)
			free(content);
	if (result <= 0) {
		printf(RED"serializacion.c: send_command: no se pudo enviar el mensaje"STD"\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

Operacion recv_msg(int socket) {
	Operacion retorno;
	int longitud = 0;
	int result = recv(socket, &(retorno.TipoDeMensaje), sizeof(int), 0);
	if (result <= 0)
		RECV_FAIL("(Serializacion.c: recv_msg) Error en la recepcion del resultado. Es posible que se haya perdido la conexion");

	switch (retorno.TipoDeMensaje) {
	case TEXTO_PLANO:
		recv(socket, &longitud, sizeof(int), 0);
		retorno.Argumentos.TEXTO_PLANO.texto = calloc(longitud+1, sizeof(char));
		recv(socket, retorno.Argumentos.TEXTO_PLANO.texto, sizeof(char) * longitud, 0);
		retorno.Argumentos.TEXTO_PLANO.texto[longitud]='\0';
		break;
	case COMANDO:
		recv(socket, &longitud, sizeof(int), 0);
		retorno.Argumentos.COMANDO.comandoParseable = calloc(longitud+1,sizeof(char));
		recv(socket, retorno.Argumentos.COMANDO.comandoParseable,sizeof(char) * longitud, 0);
		retorno.Argumentos.COMANDO.comandoParseable[longitud]='\0';
		break;
	case REGISTRO:
		recv(socket, &(retorno.Argumentos.REGISTRO.timestamp), sizeof(timestamp_t), 0);
		recv(socket, &(retorno.Argumentos.REGISTRO.key), sizeof(uint16_t), 0);
		recv(socket, &longitud, sizeof(int), 0);
		retorno.Argumentos.REGISTRO.value = calloc(longitud+1, sizeof(char));
		recv(socket, retorno.Argumentos.REGISTRO.value, sizeof(char) * longitud,0);
		retorno.Argumentos.REGISTRO.value[longitud]='\0';
		break;
	case ERROR:
		recv(socket, &longitud, sizeof(int), 0);
		retorno.Argumentos.ERROR.mensajeError = calloc(longitud+1, sizeof(char));
		recv(socket, retorno.Argumentos.ERROR.mensajeError, sizeof(char) * longitud, 0);
		retorno.Argumentos.ERROR.mensajeError[longitud]='\0';
		break;
	case GOSSIPING_REQUEST:
		recv(socket, &longitud, sizeof(int), 0);
		retorno.Argumentos.GOSSIPING_REQUEST.resultado_comprimido = calloc(longitud+1, sizeof(char));
		recv(socket, retorno.Argumentos.GOSSIPING_REQUEST.resultado_comprimido, sizeof(char)*longitud, 0);
		retorno.Argumentos.GOSSIPING_REQUEST.resultado_comprimido[longitud] = '\0';
		break;
	case GOSSIPING_REQUEST_KERNEL:
		break;
	case DESCRIBE_REQUEST:
		recv(socket, &longitud, sizeof(int), 0);
		retorno.Argumentos.DESCRIBE_REQUEST.resultado_comprimido = calloc(longitud+1, sizeof(char));
		recv(socket, retorno.Argumentos.DESCRIBE_REQUEST.resultado_comprimido, sizeof(char)*longitud, 0);
		retorno.Argumentos.DESCRIBE_REQUEST.resultado_comprimido[longitud] = '\0';
		break;
	default:
		RECV_FAIL("(Serializacion.c: recv_msg) Error en la recepcion del resultado. Tipo de operacion desconocido");
	}
	recv(socket, &(retorno.opCode), sizeof(id), 0);
	return retorno;
}

void destruir_operacion(Operacion op) {
	switch (op.TipoDeMensaje) {
	case COMANDO:
		if (op.Argumentos.COMANDO.comandoParseable != NULL)
			free(op.Argumentos.COMANDO.comandoParseable);
		return;
	case TEXTO_PLANO:
		if (op.Argumentos.TEXTO_PLANO.texto != NULL)
			free(op.Argumentos.TEXTO_PLANO.texto);
		return;
	case REGISTRO:
		if (op.Argumentos.REGISTRO.value != NULL)
			free(op.Argumentos.REGISTRO.value);
		return;
	case ERROR:
		if (op.Argumentos.ERROR.mensajeError != NULL)
			free(op.Argumentos.ERROR.mensajeError);
		return;
	case GOSSIPING_REQUEST:
		if(op.Argumentos.GOSSIPING_REQUEST.resultado_comprimido != NULL)
			free(op.Argumentos.GOSSIPING_REQUEST.resultado_comprimido);
		return;
	case DESCRIBE_REQUEST:
		if(op.Argumentos.DESCRIBE_REQUEST.resultado_comprimido != NULL)
			free(op.Argumentos.DESCRIBE_REQUEST.resultado_comprimido);
		return;
	}

}

/*
 void *serializar_comando(Comando parsed, size_t *total){
 total = malloc(sizeof(size_t)); //Tamaño total de content. Se devuelve como referencia
 void *content; //Contenido que se va a retornar
 int keyword = parsed.keyword; //Palabra princial del contenido parseado, para definir en que case entrar
 //int validez = parsed.valido; La validez del comando no la mandamos, la asumimos desde el otro lado, ya que aca se chequea si es valido, y si no lo es no lo envia
 //int _raw = parsed._raw; //Atributo que se usa para la liberacion, es importante enviarlo para que se pueda hacer free del comando desde el lado del receptor
 int tipoDeEnvio = COMANDO;

 if(parsi_validar(parsed) == EXIT_FAILURE){
 printf("serializacion.c: serializar_comando: comando invalido\n");
 return EXIT_FAILURE;
 }

 switch(keyword){
 case SELECT:
 char *arg1 = parsed.argumentos.SELECT.nombreTabla;
 int longArg1 = strlen(parsed.argumentos.SELECT.nombreTabla);
 char *arg2 = parsed.argumentos.SELECT.key;
 int longArg2 = strlen(parsed.argumentos.SELECT.key);

 total = sizeof(int) + sizeof(int) + sizeof(char)*(longArg1) + sizeof(int) + sizeof(char)*(longArg2);
 content = malloc(total);
 //Keyword
 memcpy(content, &keyword, sizeof(int));
 //longArg1
 memcpy(content+sizeof(int), &longArg1, sizeof(int));
 //arg1
 memcpy(content+sizeof(int)+sizeof(int), arg1, sizeof(char)*longArg1);
 //longArg2
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1, &longArg2, sizeof(int));
 //arg2
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int), arg2, sizeof(char)*longArg2);
 break;
 case INSERT:
 arg1 = parsed.argumentos.INSERT.nombreTabla;
 longArg1 = strlen(parsed.argumentos.INSERT.nombreTabla);
 arg2 = parsed.argumentos.INSERT.key;
 longArg2 = strlen(parsed.argumentos.INSERT.key);
 arg3 = parsed.argumentos.INSERT.value;
 longArg3 = strlen(parsed.argumentos.INSERT.value);
 if(parsed.argumentos.INSERT.timestamp == NULL){
 parsed.argumentos.INSERT.timestamp = calloc(5, sizeof(char));
 strcpy(parsed.argumentos.INSERT.timestamp, "NULL");
 parsed.argumentos.INSERT.timestamp[sizeof(char)*5] = '\0';
 }
 arg4 = parsed.argumentos.INSERT.timestamp;
 longArg4 = strlen(parsed.argumentos.INSERT.timestamp);
 total = sizeof(int) + sizeof(int)+ sizeof(char)*(longArg1) + sizeof(int)+ sizeof(char)*(longArg2) + sizeof(int)+ sizeof(char)*(longArg3) + sizeof(int)+ sizeof(char)*(longArg4);
 content = malloc(total);
 //Keyword
 memcpy(content, &keyword, sizeof(int));
 //longArg1
 memcpy(content+sizeof(int), &longArg1, sizeof(int));
 //arg1
 memcpy(content+sizeof(int)+sizeof(int), arg1, sizeof(char)*longArg1);
 //longArg2
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1, &longArg2, sizeof(int));
 //arg2
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int), arg2, sizeof(char)*longArg2);
 //longArg3
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int)+sizeof(char)*longArg2, &longArg3, sizeof(int));
 //arg3
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int)+sizeof(char)*longArg2+sizeof(int), arg3, sizeof(char)*longArg3);
 //longArg4
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int)+sizeof(char)*longArg2+sizeof(int)+sizeof(char)*longArg3, &longArg4, sizeof(int));
 //arg4
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int)+sizeof(char)*longArg2+sizeof(int)+sizeof(char)*longArg3+sizeof(int), arg4, sizeof(char)*longArg4);
 break;
 case CREATE:
 arg1 = parsed.argumentos.CREATE.nombreTabla;
 longArg1 = strlen(parsed.argumentos.CREATE.nombreTabla);
 arg2 = parsed.argumentos.CREATE.tipoConsistencia;
 longArg2 = strlen(parsed.argumentos.CREATE.tipoConsistencia);
 arg3 = parsed.argumentos.CREATE.numeroParticiones;
 longArg3 = strlen(parsed.argumentos.CREATE.numeroParticiones);
 arg4 = parsed.argumentos.CREATE.compactacionTime;
 longArg4 = strlen(parsed.argumentos.CREATE.compactacionTime);
 total = sizeof(int) + sizeof(int)+ sizeof(char)*(longArg1) + sizeof(int)+ sizeof(char)*(longArg2) + sizeof(int)+ sizeof(char)*(longArg3) + sizeof(int)+ sizeof(char)*(longArg4);
 content = malloc(total);
 //Keyword
 memcpy(content, &keyword, sizeof(int));
 //longArg1
 memcpy(content+sizeof(int), &longArg1, sizeof(int));
 //arg1
 memcpy(content+sizeof(int)+sizeof(int), arg1, sizeof(char)*longArg1);
 //longArg2
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1, &longArg2, sizeof(int));
 //arg2
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int), arg2, sizeof(char)*longArg2);
 //longArg3
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int)+sizeof(char)*longArg2, &longArg3, sizeof(int));
 //arg3
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int)+sizeof(char)*longArg2+sizeof(int), arg3, sizeof(char)*longArg3);
 //longArg4
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int)+sizeof(char)*longArg2+sizeof(int)+sizeof(char)*longArg3, &longArg4, sizeof(int));
 //arg4
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int)+sizeof(char)*longArg2+sizeof(int)+sizeof(char)*longArg3+sizeof(int), arg4, sizeof(char)*longArg4);
 break;
 case DESCRIBE:
 arg1 = parsed.argumentos.DESCRIBE.nombreTabla;
 longArg1 = strlen(parsed.argumentos.DESCRIBE.nombreTabla);

 total = sizeof(int) + sizeof(int) +sizeof(char)*(longArg1);
 content = malloc(total);
 //Keyword
 memcpy(content, &keyword, sizeof(int));
 //longArg1
 memcpy(content+sizeof(int), &longArg1, sizeof(int));
 //arg1
 memcpy(content+sizeof(int)+sizeof(int), arg1, sizeof(char)*longArg1);
 break;
 case DROP:
 arg1 = parsed.argumentos.DROP.nombreTabla;
 longArg1 = strlen(parsed.argumentos.DROP.nombreTabla);

 total = sizeof(int) + sizeof(int) + sizeof(char)*(longArg1);
 content = malloc(total);
 //Keyword
 memcpy(content, &keyword, sizeof(int));
 //longArg1
 memcpy(content+sizeof(int), &longArg1, sizeof(int));
 //arg1
 memcpy(content+sizeof(int)+sizeof(int), arg1, sizeof(char)*longArg1);
 break;
 case JOURNAL:
 total = sizeof(int);
 content = malloc(total);
 memcpy(content, &keyword, sizeof(int));
 break;
 case ADDMEMORY:
 arg1 = parsed.argumentos.ADDMEMORY.numero;
 longArg1 = strlen(parsed.argumentos.ADDMEMORY.numero);
 arg2 = parsed.argumentos.ADDMEMORY.criterio;
 longArg2 = strlen(parsed.argumentos.ADDMEMORY.criterio);

 total = sizeof(int) + sizeof(int) + sizeof(char)*(longArg1) + sizeof(int) + sizeof(char)*(longArg2);
 content = malloc(total);
 //Keyword
 memcpy(content, &keyword, sizeof(int));
 //longArg1
 memcpy(content+sizeof(int), &longArg1, sizeof(int));
 //arg1
 memcpy(content+sizeof(int)+sizeof(int), arg1, sizeof(char)*longArg1);
 //longArg2
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1, &longArg2, sizeof(int));
 //arg2
 memcpy(content+sizeof(int)+sizeof(int)+sizeof(char)*longArg1+sizeof(int), arg2, sizeof(char)*longArg2);
 break;
 case RUN:
 arg1 = parsed.argumentos.RUN.path;
 longArg1 = strlen(parsed.argumentos.RUN.path);

 total = sizeof(int) + sizeof(int) + sizeof(char)*(longArg1);
 content = malloc(total);
 //Keyword
 memcpy(content, &keyword, sizeof(int));
 //longArg1
 memcpy(content+sizeof(int), &longArg1, sizeof(int));
 //arg1
 memcpy(content+sizeof(int)+sizeof(int), arg1, sizeof(char)*longArg1);
 break;
 case METRICS:
 total = sizeof(int);
 content = malloc(total);
 memcpy(content, &keyword, sizeof(int));
 break;
 default:
 return NULL;
 }


 return NULL;
 }
 */

/*
 Comando *recv_command(int socket){
 Comando *parsed = malloc(sizeof(Comando));
 int result = recv(socket, &parsed->keyword, sizeof(int), 0);
 if(result <= 0){
 return NULL;
 }

 char* arg1 = NULL;
 char* arg2 = NULL;
 char* arg3 = NULL;
 char* arg4 = NULL;
 int longArg1 = 0; int longArg2 = 0; int longArg3 = 0; int longArg4 = 0;

 switch(parsed->keyword){
 case SELECT:
 recv(socket, &longArg1, sizeof(int), 0);
 arg1 = calloc(longArg1, sizeof(char));
 recv(socket, arg1, sizeof(char)*longArg1, 0);

 recv(socket, &longArg2, sizeof(int), 0);
 arg2 = calloc(longArg2, sizeof(char));
 recv(socket, arg2, sizeof(char)*longArg2, 0);

 parsed->argumentos.SELECT.nombreTabla = arg1;
 parsed->argumentos.SELECT.key = arg2;
 break;
 case INSERT:
 recv(socket, &longArg1, sizeof(int), 0);
 arg1 = calloc(longArg1, sizeof(char));
 recv(socket, arg1, sizeof(char)*longArg1, 0);


 recv(socket, &longArg2, sizeof(int), 0);
 arg2 = calloc(longArg2, sizeof(char));
 recv(socket, arg2, sizeof(char)*longArg2, 0);

 recv(socket, &longArg3, sizeof(int), 0);
 arg3 = calloc(longArg3, sizeof(char));
 recv(socket, arg3, sizeof(char)*longArg3, 0);

 recv(socket, &longArg4, sizeof(int), 0);
 arg4 = calloc(longArg4, sizeof(char));
 recv(socket, arg4, sizeof(char)*longArg4, 0);
 if(strcmp(arg4, "NULL")==0){
 free(arg4);
 arg4=NULL;
 }

 parsed->argumentos.INSERT.nombreTabla = arg1;
 parsed->argumentos.INSERT.key = arg2;
 parsed->argumentos.INSERT.value = arg3;
 parsed->argumentos.INSERT.timestamp = arg4;
 break;
 case CREATE:
 recv(socket, &longArg1, sizeof(int), 0);
 arg1 = calloc(longArg1, sizeof(char));
 recv(socket, arg1, sizeof(char)*longArg1, 0);

 recv(socket, &longArg2, sizeof(int), 0);
 arg2 = calloc(longArg2, sizeof(char));
 recv(socket, arg2, sizeof(char)*longArg2, 0);

 recv(socket, &longArg3, sizeof(int), 0);
 arg3 = calloc(longArg3, sizeof(char));
 recv(socket, arg3, sizeof(char)*longArg3, 0);

 recv(socket, &longArg4, sizeof(int), 0);
 arg4 = calloc(longArg4, sizeof(char));
 recv(socket, arg4, sizeof(char)*longArg4, 0);

 parsed->argumentos.CREATE.nombreTabla = arg1;
 parsed->argumentos.CREATE.tipoConsistencia = arg2;
 parsed->argumentos.CREATE.numeroParticiones = arg3;
 parsed->argumentos.CREATE.compactacionTime = arg4;
 break;
 case DESCRIBE:
 recv(socket, &longArg1, sizeof(int), 0);
 arg1 = calloc(longArg1, sizeof(char));
 recv(socket, arg1, sizeof(char)*longArg1, 0);

 parsed->argumentos.DESCRIBE.nombreTabla = arg1;
 break;
 case DROP:
 recv(socket, &longArg1, sizeof(int), 0);
 arg1 = calloc(longArg1, sizeof(char));
 recv(socket, arg1, sizeof(char)*longArg1, 0);

 parsed->argumentos.DROP.nombreTabla = arg1;
 break;
 case JOURNAL:
 //Nada
 break;
 case ADDMEMORY:
 recv(socket, &longArg1, sizeof(int), 0);
 arg1 = calloc(longArg1, sizeof(char));
 recv(socket, arg1, sizeof(char)*longArg1, 0);

 recv(socket, &longArg2, sizeof(int), 0);
 arg2 = calloc(longArg2, sizeof(char));
 recv(socket, arg2, sizeof(char)*longArg2, 0);

 parsed->argumentos.ADDMEMORY.numero = arg1;
 parsed->argumentos.ADDMEMORY.criterio = arg2;
 break;
 case RUN:
 recv(socket, &longArg1, sizeof(int), 0);
 arg1 = calloc(longArg1, sizeof(char));
 recv(socket, arg1, sizeof(char)*longArg1, 0);

 parsed->argumentos.RUN.path = arg1;
 break;
 case METRICS:
 //Nada
 break;
 default:
 return 0;
 }
 return parsed;
 }*/
