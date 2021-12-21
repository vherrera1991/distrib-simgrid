#include <string.h>
#include <math.h>
#include <stdio.h>
#include "simgrid/msg.h"            
#include "xbt/synchro.h"
#include "rand.h"

#define NUM_SERVERS	100

#define MFLOPS_BASE     (1000*1000*1000)  // para el cálculo del tiempo de servicio de cada tarea

#define NUM_CLIENTS	1
#define NUM_DISPATCHERS	1
#define NUM_TASKS	200000		// número de tareas a generar


#define SERVICE_RATE    1.0     // Mu = 1  service time = 1 / 1; tasa de servicio de cada servidor
double ARRIVAL_RATE;		// tasa de llegada, como argumento del programa

const long MAX_TIMEOUT_SERVER=(86400*10); //Timeout= 10 días sin actividad

// variables para gestionar la cola de tareas en cada servidor
xbt_dynar_t client_requests[NUM_SERVERS] ;   // cola de peticiones en cada servidor, array dinamico de SimGrid
xbt_mutex_t mutex[NUM_SERVERS];
xbt_cond_t  cond[NUM_SERVERS];
int 	EmptyQueue[NUM_SERVERS];	// indicacion de fin de cola en cada servidor


// variables para estadísticas
int 	Nqueue[NUM_SERVERS];          	// elementos en la cola de cada servidor esperando a ser servidos
double 	Navgqueue[NUM_SERVERS];         // tamanio medio de la cola de cada servidor

int 	Nsystem[NUM_SERVERS];		// número de tareas en cada servidor (esperando y siendo atendidas)
double 	Navgsystem[NUM_SERVERS];	// número medio de tareas por servidor (esperando y siendo atendidas)


double tiempoMedioServicio[NUM_TASKS];    // tiempo medio de servicio para cada tarea

// estrucrtura de la petcición que envía el cliente 
struct ClientRequest {
	int    n_task;      // número de tarea
	double t_arrival;   /* momento en el que llega la tarea (tiempo de creacion)*/
	double t_service;   /* tiempo de servicio asignado en FLOPS*/
};


// ordena dos elementos de tipo struct ClientRequest
// utilizado para poder ordenar la colaisutilizando la fucion xbt__dynar_sort
static int sort_function(const void *e1, const void *e2)
{
	struct ClientRequest *c1 = *(void **) e1;
	struct ClientRequest *c2 = *(void **) e2;

	if (c1->t_service == c2->t_service)
		return 0;

	else    if (c1->t_service < c2->t_service)
		return -1;
	else
		return 1;
}


// Client function: genera las peticiones
int client(int argc, char *argv[])
{
  	double task_comp_size = 0;
  	double task_comm_size = 0;
  	char sprintf_buffer[64];
  	char mailbox[256];
  	msg_task_t task = NULL;
  	struct ClientRequest *req ;
  	double t_arrival;
	int my_c;
	double t;
	int k;

	my_c = atoi(argv[0]);  // identifiador del cliente
	MSG_mailbox_set_async("c-0");   //mailbox asincrono

	for (k=0; k <NUM_TASKS; k++) {
		req = (struct ClientRequest *) malloc(sizeof(struct ClientRequest));

      		/* espera la llegada de una peticion */
		/* ARRIVAL_RATE peticiones por segundo, lamda = ARRIVAL_RATE5 */
      		t_arrival = exponential((double)ARRIVAL_RATE);
      		MSG_process_sleep(t_arrival);

      		/* crea la tarea */
      		sprintf(sprintf_buffer, "Task_%d", k);
      		req->t_arrival = MSG_get_clock();                  // tiempo de llegada

		// tiempo de servicio asignada a la tarea
                // t medio de servicio = 1/SERVICE_RATE de seg
                // Como base se toma que en 1 seg se encutan MFLOPS_BASE flops
		t = exponential((double)SERVICE_RATE);

                req->t_service = MFLOPS_BASE * t;  // calculo del tiempo de servicio en funcion
						   // de la velocidad del host del servidor
		req->n_task = k;
		task_comp_size = req->t_service;
                task_comm_size = 0;

      		task = MSG_task_create(sprintf_buffer, task_comp_size, task_comm_size,NULL);

		// asigna datos a la tarea
      		MSG_task_set_data(task, (void *) req );

		// Código para el algotirmo de distribución
		
		// ahora se la envía a un único dispather
		sprintf(mailbox, "d-%d", 0);
	
		MSG_task_send(task, mailbox);   
	}

    	/* finalizar */
  	return 0;
}                               


// dispatcher function, recibe las peticiones de los clientes y las envía a los servidores
int dispatcher(int argc, char *argv[])
{
	int res;
        struct ClientRequest *req;
        msg_task_t task = NULL;
        msg_task_t new_task = NULL;
	int my_d;
	char mailbox[64];
	int k = 0;
	int s = 0;

	my_d = atoi(argv[0]);
	MSG_mailbox_set_async("d-0");   //mailbox asincrono


	int r = 0;

	while (1) {
                res = MSG_task_receive_with_timeout(&(task), MSG_host_get_name(MSG_host_self()) , MAX_TIMEOUT_SERVER);

                if (res != MSG_OK)
                        break;

                req = MSG_task_get_data(task);

		// copia la tarea en otra
	        new_task = MSG_task_create(MSG_task_get_name(task),
						MSG_task_get_flops_amount(task), 0, NULL);

                MSG_task_set_data(new_task, (void *) req );	

                MSG_task_destroy(task);
                task = NULL;


		////////////////////////////////////////////////////////////
		// ahora viene el algoritmo concreto del dispatcher	
		// para el algoritmo aleatorio se puede utilizar la función uniform_int definida en rand.c
		// para el algoritmo SQF se puede consultar directamente el array Nsystem que almacena
		// el número de elementos en cada uno de los servidores. Se trata de buscar el servidor con 
		// el menor numero de elementos en la cola.

                if (r == NUM_SERVERS)
                {
          		r = 0;
                }

                sprintf(mailbox, "s-%d", r);

                MSG_task_send(new_task, mailbox);

                r++;
		k++;
        }
	return 0;
}


/** server function  */
int server(int argc, char *argv[])
{
  	msg_task_t task = NULL;
  	msg_task_t t = NULL;
  	struct ClientRequest *req;
  	int res;
	int my_s;
	char buf[64];
	
	my_s = atoi(argv[0]);
	sprintf(buf, "s-%d", my_s);
	MSG_mailbox_set_async(buf);   //mailbox asincrono

  	while (1) {
    		res = MSG_task_receive_with_timeout(&(task), MSG_host_get_name(MSG_host_self()) , MAX_TIMEOUT_SERVER);

		if (res != MSG_OK)
			break;

		req = MSG_task_get_data(task);
		
		// inserta la petición en la cola
    		xbt_mutex_acquire(mutex[my_s]);
    		Nqueue[my_s]++;   // un elemento mas en la cola 
    		Nsystem[my_s]++;  // un elemento mas en el sistema 

		// se inserta la tarea en orden FCFS
		// Con otras políticas, habrá que ordenar después la cola utilizando
		// xbt_dynar_sort
		xbt_dynar_push(client_requests[my_s], (const char *)&req);


		xbt_cond_signal(cond[my_s]);  // despierta al proceso server
    		xbt_mutex_release(mutex[my_s]);

    		MSG_task_destroy(task);
    		task = NULL;
	}  

	// marca el fin
 	xbt_mutex_acquire(mutex[my_s]);
	EmptyQueue[my_s] = 1;
	xbt_cond_signal(cond[my_s]);
    	xbt_mutex_release(mutex[my_s]);

  	return 0;
}       


/** server function  */
int dispatcherServer(int argc, char *argv[])
{
	int res;
  	struct ClientRequest *req;
	msg_task_t task = NULL;
	msg_task_t ans_task = NULL;
	double Nqueue_avg = 0.0;
	double Nsystem_avg = 0.0;
	double c;
	int n_tasks = 0;
	int my_s;


	my_s = atoi(argv[0]);

	while (1) {
    		xbt_mutex_acquire(mutex[my_s]);

		while ((Nqueue[my_s] ==  0)   && (EmptyQueue[my_s] == 0)) {
			xbt_cond_wait(cond[my_s], mutex[my_s]);
		}

		if ((EmptyQueue[my_s] == 1) && (Nqueue[my_s] == 0)) {
			xbt_mutex_release(mutex[my_s]);
			break;
		}
		// extrae un elemento de la cola
                xbt_dynar_shift(client_requests[my_s], (char *) &req);

		Nqueue[my_s]--;  // un elemento menos en la cola

		n_tasks ++;

		// calculo de estadisticas
		Navgqueue[my_s] = (Navgqueue[my_s] * (n_tasks-1) + Nqueue[my_s]) / n_tasks;
		Navgsystem[my_s] = (Navgsystem[my_s] * (n_tasks-1) + Nsystem[my_s]) / n_tasks;


    		xbt_mutex_release(mutex[my_s]);

		// crea una tarea para su ejecución
		task = MSG_task_create("task", req->t_service, 0, NULL);

		MSG_task_execute(task);

    		xbt_mutex_acquire(mutex[my_s]);
		Nsystem[my_s]--;  // un elemento menos en el sistema
    		xbt_mutex_release(mutex[my_s]);

		c = MSG_get_clock();  // tiempo de terminacion de la tarea
		tiempoMedioServicio[req->n_task] = c - (req->t_arrival);

		free(req);
		MSG_task_destroy(task);
	}

}

void test_all(char *file)
{
	int argc;
        char str[50];
        int i;
        msg_process_t p;

  	MSG_create_environment(file);

	// el proceso client es el que genera las peticiones
  	MSG_function_register("client", client);

	// el proceso dispatcher es el que distribuye las peticiones que le llegan a los servidores
  	MSG_function_register("dispatcher", dispatcher);

	// cada servidor tiene un proceso server que recibe las peticiones: server
	// y un proceso dispatcher que las ejecuta
  	MSG_function_register("server", server);
  	MSG_function_register("dispatcherServer", dispatcherServer);

	for (i=0; i < NUM_SERVERS; i++) {
                sprintf(str,"s-%d", i);
                argc = 1;
                char **argvc=xbt_new(char*,2);

                argvc[0] = bprintf("%d",i);
                argvc[1] = NULL;

                p = MSG_process_create_with_arguments(str, server, NULL, MSG_get_host_by_name(str), argc, argvc);
                if (p == NULL) {
                        printf("Error en ......... %d\n", i);
                        exit(0);
                }
        }

        for (i=0; i < NUM_SERVERS; i++) {
                sprintf(str,"s-%d", i);
                argc = 1;
                char **argvc=xbt_new(char*,2);

                argvc[0] = bprintf("%d",i);
                argvc[1] = NULL;

                p = MSG_process_create_with_arguments(str, dispatcherServer, NULL, MSG_get_host_by_name(str), argc, argvc);
                if (p == NULL) {
                        printf("Error en ......... %d\n", i);
                        exit(0);
                }
        }

	 for (i=0; i < NUM_CLIENTS; i++) {
                sprintf(str,"c-%d", i);
                argc = 1;
                char **argvc=xbt_new(char*,2);

                argvc[0] = bprintf("%d",i);
                argvc[1] = NULL;

                p = MSG_process_create_with_arguments(str, client, NULL, MSG_get_host_by_name(str), argc, argvc);
                if (p == NULL) {
                        printf("Error en ......... %d\n", i);
                        exit(0);
                }
        }

	 for (i=0; i < NUM_DISPATCHERS; i++) {
                sprintf(str,"d-%d", i);
                argc = 1;
                char **argvc=xbt_new(char*,2);

                argvc[0] = bprintf("%d",i);
                argvc[1] = NULL;

                p = MSG_process_create_with_arguments(str, dispatcher, NULL, MSG_get_host_by_name(str), argc, argvc);
                if (p == NULL) {
                        printf("Error en ......... %d\n", i);
                        exit(0);
		}
	}

	return;
}


/** Main function */
int main(int argc, char *argv[])
{
  	msg_error_t res = MSG_OK;
	int i;

	double t_medio_servicio = 0.0;		// tiempo medio de servicio de cada tarea
        double q_medio = 0.0; 			// tamaño medio de la cola (esperando a ser servidos)
        double n_medio = 0.0;			// número medio de tareas en el sistema (esperando y ejecutando)

	 if (argc < 3) {
                printf("Usage: %s platform_file lamda \n", argv[0]);
                exit(1);
        }

        seed((int) time(NULL));
        ARRIVAL_RATE = atof(argv[2]) *  NUM_SERVERS;

  	MSG_init(&argc, argv);

	for (i = 0; i < NUM_SERVERS; i++) {
		Nqueue[i] =0;
		Nsystem[i] =0;
		EmptyQueue[i]=0;
  		mutex[i] = xbt_mutex_init();
  		cond[i] = xbt_cond_init();
		client_requests[i] = xbt_dynar_new(sizeof(struct ClientRequest *), NULL);
	}

	test_all(argv[1]);

  	res = MSG_main();
	
        for (i = 0; i < NUM_TASKS; i++){
                t_medio_servicio = t_medio_servicio + tiempoMedioServicio[i];
        }

        for (i = 0; i < NUM_SERVERS; i++){
                q_medio = q_medio + Navgqueue[i];
                n_medio = n_medio + Navgsystem[i];
        }

        t_medio_servicio = t_medio_servicio / (NUM_TASKS);
        q_medio = q_medio / (NUM_SERVERS);
        n_medio = n_medio / (NUM_SERVERS);

	printf("tiempoMedioServicio \t TamañoMediocola \t    TareasMediasEnElSistema  \t   tareas\n");
        printf("%g \t\t\t %g \t\t\t  %g  \t\t\t  %d \n", t_medio_servicio, q_medio,  n_medio, NUM_TASKS );

	//printf("Simulation time %g\n", MSG_get_clock());

	for (i = 0; i < NUM_SERVERS; i++) {
		xbt_dynar_free(&client_requests[i]);
	}

  	if (res == MSG_OK)
    		return 0;
  	else
    		return 1;
}
