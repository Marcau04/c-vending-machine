#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>

#define NUM_MAX_PRODUCTOS 10
#define CADENA_PROVEEDOR "Proveedor %i:\n\tProductos procesados: %i.\n\tProductos invalidos: %i.\n\tProductos validos: %i. De los cuales se han insertado:\n"
#define CADENA_PRODUCTO_PROVEEDOR "\t\t%i de tipo '%c'.\n"
#define CADENA_CONSUMIDOR "Cliente consumidor %i:\n\tProductos consumidos: %i. De los cuales:\n"
#define CADENA_PRODUCTO_CONSUMIDOR "\t\tProducto tipo '%c': %i\n"
#define CADENA_FACTURADOR "Total de productos consumidos: %i\n"
#define CADENA_FACTURADOR_PROVEEDORES "\t%i del proveedor %i\n"
#define CADENA_FACTURADOR_CLIENTE "Cliente consumidor que mas ha consumido: %i\n"

typedef struct {
    char producto;
    int idProveedor;
} elemento_buffer;

typedef struct nodo_lista_enlazada {
    int idConsumidor;
    int productosConsumidos;
    int productos[NUM_MAX_PRODUCTOS];
    int *productosConsumidosProveedores;
    struct nodo_lista_enlazada *sig;
} nodo_lista_enlazada;

typedef struct {
    int id;
    elemento_buffer *buffer;
    int T;
    char ficheroProveedor[255];
    FILE *fpFicheroSalida;
} parametrosHilosProveedores;

typedef struct {
    int id;
    elemento_buffer *buffer;
    int T;
    int P;
    nodo_lista_enlazada *cabezaListaEnlazada;
} parametrosHilosConsumidores;

typedef struct {
    int P;
    int C;
    nodo_lista_enlazada *cabezaListaEnlazada;
    FILE *fpFicheroSalida;
} parametrosHiloFacturador;


int esDigito(char c[]);
void *proveedor(void *args);
void *consumidor(void *args);
void *facturador(void *args);

sem_t insertarDato;
sem_t extraerDato;
sem_t SCP;
sem_t SCC;
sem_t listaEnlazadaMutex;
sem_t finConsumidor;

int proveedores, lecturaBuffer = 0, escrituraBuffer = 0;

int main(int argc, char *argv[]){
    //Creacion de variables y punteros.
    int i, T, P, C;
    FILE *fp, *fp2;
    pthread_t *idHiloProveedor;
    pthread_t *idHiloConsumidor;
    pthread_t idHiloFacturador;
    parametrosHilosProveedores *paramHilosProveedores;
    parametrosHilosConsumidores *paramHilosConsumidores;
    parametrosHiloFacturador paramHiloFacturador;
    elemento_buffer *buffer;
    nodo_lista_enlazada *cabezaListaEnlazada, *punteroAuxiliarListaEnlazada;
    
    //Comprobacion de parametros.
    //Se comprueba el numero de argumentos.
    if (argc != 6){
        write(2,"ERROR: Únicamente se admiten 5 parámetros para poder utilizar el programa.\n",strlen("ERROR: Únicamente se admiten 5 parámetros para poder utilizar el programa.\n"));
        exit(-1);
    }

    //Se comprueba que el valor del argumento T se encuentra en el intervalo 1-5000.
    if(esDigito(argv[3]) == 1){
        if ((T = atoi(argv[3])) == 0) {
            write(2,"ERROR: No se ha podido convertir la cadena de caracteres en un entero.\n",strlen("ERROR: No se ha podido convertir la cadena de caracteres en un entero.\n"));
            exit(-1);
        }
        if(T < 1 || T > 5000){
            write(2,"ERROR: Número inválido de tamaño del buffer, únicamente se admiten valores del 1 al 5000.\n",strlen("ERROR: Número inválido de tamaño del buffer, únicamente se admiten valores del 1 al 5000.\n"));
            exit(-1);
        }
    }else{
        write(2,"ERROR: La cadena de entrada tiene que ser numérica.\n",strlen("ERROR: La cadena de entrada tiene que ser numérica.\n"));
        exit(-1);
    }

    //Se comprueba que el valor del argumento P se encuentra en el intervalo 1-7.
    if(esDigito(argv[4]) == 1){
        if ((P = atoi(argv[4])) == 0) {
            write(2,"ERROR: No se ha podido convertir la cadena de caracteres en un entero.\n",strlen("ERROR: No se ha podido convertir la cadena de caracteres en un entero.\n"));
            exit(-1);
        }
        if(P < 1 || P > 7){
            write(2,"ERROR: Número inválido de proveedores, únicamente se admiten valores del 1 al 7.\n",strlen("ERROR: Número inválido de proveedores, únicamente se admiten valores del 1 al 7.\n"));
            exit(-1);
        }
    }else{
        write(2,"ERROR:La cadena de entrada tiene que ser numérica.\n",strlen("ERROR:La cadena de entrada tiene que ser numérica.\n"));
        exit(-1);
    }

    //Se comprueba que el valor del argumento C se encuentra en el intervalo 1-1000.
    if(esDigito(argv[5]) == 1){
        if ((C = atoi(argv[5])) == 0) {
            write(2,"ERROR: No se ha podido convertir la cadena de caracteres en un entero.\n",strlen("ERROR: No se ha podido convertir la cadena de caracteres en un entero.\n"));
            exit(-1);
        }
        if(C < 1 || C > 1000){
            write(2,"ERROR: Número inválido de clientes consumidores, únicamente se admiten valores del 1 al 1000.\n",strlen("ERROR: Número inválido de clientes consumidores, únicamente se admiten valores del 1 al 1000.\n"));
            exit(-1);
        }
    }else{
        write(2,"ERROR: La cadena de entrada tiene que ser numérica.\n",strlen("ERROR: La cadena de entrada tiene que ser numérica.\n"));
        exit(-1);
    }

    //Se comprueba que se puede crear el fichero sin problemas, en caso de que exista, se avisa al usuario de que será borrado.
    if((fp = fopen(argv[2],"r")) != NULL){
        write(2,"WARNING: El fichero destino no debe exister, se procederá a borrar y recrear dicho fichero.\n",strlen("WARNING: El fichero destino no debe exister, se procederá a borrar y recrear dicho fichero.\n"));
    }
    if((fp = fopen(argv[2],"w")) == NULL){
        write(2, "Error al crear el fichero destino.\n", strlen("Error al crear el fichero destino.\n"));
        exit(-1);
    }

    //Se reserva memoria para el array de IDs de los hilos proveedores.
    if ((idHiloProveedor = (pthread_t *)malloc(sizeof(pthread_t) * P)) == NULL) {
        write(2,"Error al reservar memoria para los parametros de los hilos.\n",strlen("Error al reservar memoria para los parametros de los hilos.\n"));
        exit(-1);
    }

    //Se reserva memoria para el array de IDs de los hilos consumidores.
    if ((idHiloConsumidor = (pthread_t *)malloc(sizeof(pthread_t) * C)) == NULL) {
        write(2,"Error al reservar memoria para los parametros de los hilos.\n",strlen("Error al reservar memoria para los parametros de los hilos.\n"));
        exit(-1);
    }

    //Se prepara el bufer circular.
    if ((buffer = (elemento_buffer *)malloc(sizeof(elemento_buffer) * T)) == NULL) {
        write(2,"Error al inicializar el bufer circular.\n",strlen("Error al inicializar el bufer circular.\n"));
        exit(-1);

    }

    //Se prepara la lista enlazada.
    if ((cabezaListaEnlazada = (nodo_lista_enlazada *)malloc(sizeof(nodo_lista_enlazada) * T)) == NULL) {
        write(2,"Error al inicializar la lista enlazada.\n",strlen("Error al inicializar la lista enlazada.\n"));
        exit(-1);

    }
    //Se inicializan los semaforos.
    sem_init(&insertarDato, 0, T);
    sem_init(&extraerDato, 0, 0);
    sem_init(&SCP, 0, 1);
    sem_init(&SCC, 0, 1);
    sem_init(&listaEnlazadaMutex, 0, 1);
    sem_init(&finConsumidor, 0, 0);

    //Se reserva memoria para los parametros.
    if ((paramHilosProveedores = (parametrosHilosProveedores *)malloc(sizeof(parametrosHilosProveedores) * P)) == NULL) {
        write(2,"Error al reservar memoria para los parametros de los hilos.\n",strlen("Error al reservar memoria para los parametros de los hilos.\n"));
        exit(-1);
    }
    if ((paramHilosConsumidores = (parametrosHilosConsumidores *)malloc(sizeof(parametrosHilosConsumidores) * C)) == NULL) {
        write(2,"Error al reservar memoria para los parametros de los hilos.\n",strlen("Error al reservar memoria para los parametros de los hilos.\n"));
        exit(-1);
    }
    
    //Se le da a la variable global proveedores el valor numerico del total de proveedores que hay.
    proveedores = P;
    
    //Se preparan los parametros para los hilos proveedores y se lanzan.
    for(i=0; i < P; i++){
        paramHilosProveedores[i].id = i;
        paramHilosProveedores[i].T = T;
        paramHilosProveedores[i].buffer = buffer;
        paramHilosProveedores[i].fpFicheroSalida = fp;
    
        sprintf(paramHilosProveedores[i].ficheroProveedor,"%s/proveedor%d.dat",argv[1],i);

        //Se comprueba que los ficheros de los proveedores se abren sin problemas.
        if((fp2 = fopen(paramHilosProveedores[i].ficheroProveedor,"r")) == NULL){
            write(2,"ERROR:El fichero proveedor no se puede leer correctamente.\n",strlen("ERROR:El fichero proveedor no se puede leer correctamente.\n"));
            exit(-1);
        }
        fclose(fp2);

        if (pthread_create(&idHiloProveedor[i], NULL, proveedor, (void *)&paramHilosProveedores[i]) != 0) {
            write(2,"Error en la creacion de un hilo.\n",strlen("Error en la creacion de un hilo.\n"));
            exit(-1);
        }
    }
    
    //Se preparan los parametros para los hilos consumidores y se lanzan.
    for(i = 0; i < C; i++) {
        paramHilosConsumidores[i].id = i;
        paramHilosConsumidores[i].buffer = buffer;
        paramHilosConsumidores[i].T = T;
        paramHilosConsumidores[i].P = P;
        paramHilosConsumidores[i].cabezaListaEnlazada = cabezaListaEnlazada;

        if (pthread_create(&idHiloConsumidor[i], NULL, consumidor, (void *)&paramHilosConsumidores[i]) != 0) {
            write(2,"Error en la creacion de un hilo.\n",strlen("Error en la creacion de un hilo.\n"));
            exit(-1);
        }
    }
    
    //Se preparan los parametros para el hilo facturador y se lanza.
    paramHiloFacturador.P = P;
    paramHiloFacturador.C = C;
    paramHiloFacturador.cabezaListaEnlazada = cabezaListaEnlazada;
    paramHiloFacturador.fpFicheroSalida = fp;
    if (pthread_create(&idHiloFacturador, NULL, facturador, (void *)&paramHiloFacturador) != 0) {
        write(2,"Error en la creacion de un hilo.\n",strlen("Error en la creacion de un hilo.\n"));
        exit(-1);
    }
    
    //Se espera a q los hilos terminen y se va liberando la memoria cuando ya no es necesaria.
    for(i = 0; i < P; i++) {
        pthread_join(idHiloProveedor[i], NULL);
    }
    fprintf(fp, "\n");//Salto de linea para generar un espacio en el fichero entre proveedores y clientes.
    free(paramHilosProveedores);
    free(idHiloProveedor);

    for(i = 0; i < C; i++) {
        pthread_join(idHiloConsumidor[i], NULL);
    }
    free(paramHilosConsumidores);
    free(idHiloConsumidor);
    free(buffer);
    
    pthread_join(idHiloFacturador, NULL);
    
    while(cabezaListaEnlazada != NULL) {
        punteroAuxiliarListaEnlazada = cabezaListaEnlazada;
        cabezaListaEnlazada = cabezaListaEnlazada->sig;
        free(punteroAuxiliarListaEnlazada->productosConsumidosProveedores);
        free(punteroAuxiliarListaEnlazada);
    }
    fclose(fp);

    //Se destruyen los semaforos.
    sem_destroy(&insertarDato);
    sem_destroy(&extraerDato);
    sem_destroy(&SCP);
    sem_destroy(&SCC);
    sem_destroy(&listaEnlazadaMutex);
    sem_destroy(&finConsumidor);

    return 0;
}

int esDigito(char c[]) {
    int i;
    for (i = 0; i<strlen(c); i++) {
        if (c[i] > '9' || c[i] < '0') {
            return 0;
        }
    }
    return 1;
}

void *proveedor(void *args) {
    //Puntero a los datos del tipo struct.
    parametrosHilosProveedores *p = (parametrosHilosProveedores *)args;
    //Declaracion de las variables y punteros a usar.
    FILE *fp;
    char caracter;
    int procesados = 0, invalidos = 0, validos = 0, productos[NUM_MAX_PRODUCTOS], i = 0;
    elemento_buffer elemento;

    //Se prepara el elemento a añadir de forma que guarde el id del proveedor.
    elemento.idProveedor = p->id;

    //Se comprueba que el fichero "proveedorX.dat" se abre sin problemas.
    if((fp = fopen(p->ficheroProveedor, "r")) == NULL) {
        write(2,"ERROR: El fichero proveedor no se puede leer correctamente.\n",strlen("ERROR: El fichero proveedor no se puede leer correctamente.\n"));
        exit(-1);
    }

    //Se realiza el conteo de productos y la insercion en el bufer circular.
    while((caracter = fgetc(fp)) != EOF) {
        if (caracter >= 'a' && caracter <= 'j') {
            sem_wait(&insertarDato);
            sem_wait(&SCP);
            p->buffer[escrituraBuffer] = elemento;
            escrituraBuffer = (escrituraBuffer + 1) % (p->T);
            sem_post(&SCP);
            sem_post(&extraerDato);
            

            validos++;
            productos[caracter - 'a']++;
            elemento.producto = caracter;
        } else {
            invalidos++;
        }
        procesados++;
    }
    fclose(fp);

    sem_wait(&SCP);
    if(proveedores == 1) {
        elemento.producto = 'z';
        sem_wait(&insertarDato);
        p->buffer[escrituraBuffer] = elemento;
        escrituraBuffer = (escrituraBuffer + 1) % (p->T);
        sem_post(&extraerDato);

    } else {
        proveedores--;
    }
    
    //Se escribe en el fichero el resultado del proveedor.
    fprintf(p->fpFicheroSalida, CADENA_PROVEEDOR,  p->id, procesados, invalidos, validos);
    for (i = 0; i < NUM_MAX_PRODUCTOS; i++) {
        fprintf(p->fpFicheroSalida, CADENA_PRODUCTO_PROVEEDOR, productos[i], (char)(i+'a'));
    }
    sem_post(&SCP);
    
    pthread_exit(NULL);
}

void *consumidor(void *args) {
    //Puntero a los datos del tipo struct.
    parametrosHilosConsumidores *p = (parametrosHilosConsumidores *)args;
    //Declaracion de las variables y punteros a usar.
    nodo_lista_enlazada *nuevoNodo;
    int productosConsumidos = 0, productos[NUM_MAX_PRODUCTOS], *prodConsumidosProveedores, i = 0, fin = 1;

    sem_wait(&SCC);
    //Se reserva memoria para el nodo de la lista enlazada de este consumidor.
    if((nuevoNodo = malloc(sizeof(nodo_lista_enlazada))) == NULL) {
        write(2,"Error al crear el nuevo nodo.\n",strlen("Error al crear el nuevo nodo.\n"));
        exit(-1);
    }
    nuevoNodo->sig = NULL;
    
    //Se coloca el puntero del nodo al final de la lista enlazada.
    while(p->cabezaListaEnlazada->sig != NULL) {
        p->cabezaListaEnlazada = p->cabezaListaEnlazada->sig;
    }
    p->cabezaListaEnlazada->sig = nuevoNodo;
    
    //Se reserva memoria para el array de productos consumidos de cada proveedor.
    if((p->cabezaListaEnlazada->productosConsumidosProveedores = malloc(sizeof(int) * p->P)) == NULL) {
        write(2,"Error al reservar memoria.\n",strlen("Error al reservar memoria.\n"));
        exit(-1);
    }

    //Se guarda el id del consumidor en el nodo.
    p->cabezaListaEnlazada->idConsumidor = p->id;
    sem_post(&SCC);

    //Se van extrayendo los datos del buffer y guardandolos en el nodo de la lista enlazada.
    while(fin == 1) {
        sem_wait(&extraerDato);
        sem_wait(&SCC);
        if (p->buffer[lecturaBuffer].producto == 'z') {
            fin = 0;
            sem_post(&extraerDato);
            sem_post(&SCC);
        } else {
            p->cabezaListaEnlazada->productos[p->buffer[lecturaBuffer].producto - 'a']++;
            p->cabezaListaEnlazada->productosConsumidosProveedores[p->buffer[lecturaBuffer].idProveedor]++;
            p->cabezaListaEnlazada->productosConsumidos++;
            lecturaBuffer = (lecturaBuffer + 1) % (p->T);
            sem_post(&insertarDato);
            sem_post(&SCC);
        }
    }
    
    //Se libera la memoria usada, se indica que un consumidor ha terminado y se sale del hilo.
    sem_post(&finConsumidor);
    pthread_exit(NULL);
}

void *facturador(void *args) {
    //Puntero a los datos de tipo struct.
    parametrosHiloFacturador *p = (parametrosHiloFacturador *)args;
    //Declaracion de punteros y variables.
    int i, j, totalProductosConsumidos = 0, idClienteMasConsumidor, numProductosClienteMasConsumidor = 0, *totalProductosConsumidosProveedores;
    
    //Se reserva memoria para el array de productos consumidos de cada proveedor.
    if((totalProductosConsumidosProveedores = malloc(sizeof(int) * p->P)) == NULL) {
        write(2,"Error al reservar memoria.\n",strlen("Error al reservar memoria.\n"));
        exit(-1);
    }
    
    for(i = 0; i < p->C; i++) {
        //Se espera a que termine algun consumidor.
        sem_wait(&finConsumidor);
        //A partir de la segunda iteracción se busca el nodo siguiente al facturado anteriormente.
        if (i > 0) {
            p->cabezaListaEnlazada = p->cabezaListaEnlazada->sig;
        }
        //Conteo de todos los productos consumidos.
        totalProductosConsumidos = totalProductosConsumidos + p->cabezaListaEnlazada->productosConsumidos;
        for (j = 0; j < p->P; j++) {
            totalProductosConsumidosProveedores[j] += p->cabezaListaEnlazada->productosConsumidosProveedores[j];
        }
        //Se busca el cliente que mas ha consumido.
        if (p->cabezaListaEnlazada->productosConsumidos > numProductosClienteMasConsumidor) {
            idClienteMasConsumidor = p->cabezaListaEnlazada->idConsumidor;
            numProductosClienteMasConsumidor = p->cabezaListaEnlazada->productosConsumidos;
        }
        //Se escribe en el fichero de salida los resultados de los clientes.
        fprintf(p->fpFicheroSalida, CADENA_CONSUMIDOR,  p->cabezaListaEnlazada->idConsumidor, p->cabezaListaEnlazada->productosConsumidos);
        for (j = 0; j < NUM_MAX_PRODUCTOS; j++) {
            fprintf(p->fpFicheroSalida, CADENA_PRODUCTO_CONSUMIDOR, (char)(j + 'a'), p->cabezaListaEnlazada->productos[j]);
        }
    }
    
    //Se escribe en el fichero el total de productos consumidos, los productos consumidos de cada proveedor y el id
    //del cliente que mas ha consumido.
    fprintf(p->fpFicheroSalida, "\n");
    fprintf(p->fpFicheroSalida, CADENA_FACTURADOR,  totalProductosConsumidos);
    for(i = 0; i < p->P; i++) {
        fprintf(p->fpFicheroSalida, CADENA_FACTURADOR_PROVEEDORES,  totalProductosConsumidosProveedores[i], i);
    }
    fprintf(p->fpFicheroSalida, CADENA_FACTURADOR_CLIENTE,  idClienteMasConsumidor);

    free(totalProductosConsumidosProveedores);
    pthread_exit(NULL);
}
