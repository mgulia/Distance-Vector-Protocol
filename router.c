#include "router.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include "ne.h"
#include <pthread.h>
#include <time.h>

#define LISTENQ 10
#define MAXLINE 100
#define MAX_ROUTERS 10
#define _OPEN_SYS_ITOA_EXT

extern int errno;
int childpid;
int listenudp;
int id;
int no_nbr;
char routerlogfile[MAXLINE];
time_t convergenceTime;
time_t lastTime;
pthread_mutex_t lock;
FILE* file;
int flag=0;
time_t startinitresponse;

struct sockaddr_in serveraddr;
struct hostent *hp;
socklen_t size=sizeof(serveraddr);

struct data{
	int nbrID;
	time_t timeAt;
	int costToNbr;
	int alive;
};

struct data arr[MAX_ROUTERS];


int open_listenudp(int port)  
{ 
  int listenudp, optval=1; 
  struct sockaddr_in serveraddr; 
   
  /* Create a socket descriptor */ 
  if ((listenudp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    return -1; 
  
  /* Eliminates "Address already in use" error from bind. */ 
  if (setsockopt(listenudp, SOL_SOCKET, SO_REUSEADDR,  
		 (const void *)&optval , sizeof(int)) < 0) 
    return -1; 
 
  /* Listenudp will be an endpoint for all requests to port 
     on any IP address for this host */ 
  bzero((char *) &serveraddr, sizeof(serveraddr)); 
  serveraddr.sin_family = AF_INET;  
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);  
  serveraddr.sin_port = htons((unsigned short)port);  
  if (bind(listenudp, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) 
    return -1; 
 
  return listenudp; 
}

void * udpFunction(){

	struct pkt_RT_UPDATE updatePkt;
	int update;
	int index;
	struct sockaddr_in serveraddr;

	socklen_t size=sizeof(serveraddr);
    	lastTime=time(NULL);
	while(1){

		recvfrom(listenudp,&updatePkt, sizeof(updatePkt),0, &serveraddr, &size);
		
		//lock
		pthread_mutex_lock(&lock);
		ntoh_pkt_RT_UPDATE(&updatePkt);
        	int i;
		for(i=0;i<no_nbr;i++){
		    if(arr[i].nbrID==updatePkt.sender_id){
		        index=i;
		        break;
		    }
		}

		update = UpdateRoutes(&updatePkt, arr[index].costToNbr, id);

		arr[index].timeAt = time(NULL);
		arr[index].alive = 1;
		if (update == 1){
			flag=0;
		    convergenceTime = time(NULL);
		    PrintRoutes(file,id);
	   	}
		pthread_mutex_unlock(&lock);
	}
	return;
}
void * timerFunction(){
    time_t seconds=NULL;
    int sent;
    int htonl_nbr;
    struct pkt_RT_UPDATE updatePkt;

    while(1){
	pthread_mutex_lock(&lock);
	if (time(NULL) - lastTime > UPDATE_INTERVAL){
		ConvertTabletoPkt(&updatePkt, id);
		hton_pkt_RT_UPDATE(&updatePkt);
		int x;
		for(x=0; x<no_nbr; x++){
			updatePkt.dest_id = htonl(arr[x].nbrID);
			sent= sendto(listenudp, &updatePkt, sizeof(updatePkt), 0, &serveraddr,sizeof(serveraddr));	//fix
		}	
		lastTime = time(NULL);
	}

        seconds=time(NULL);
        int i;
        for(i=0;i<no_nbr;i++){
            if(time(NULL)-arr[i].timeAt>FAILURE_DETECTION && arr[i].alive == 1){
		arr[i].alive = 0;
                UninstallRoutesOnNbrDeath(arr[i].nbrID);
		convergenceTime = time(NULL);
		flag = 0;
		PrintRoutes(file,id);

            }
        }
        if(time(NULL)-convergenceTime>CONVERGE_TIMEOUT && flag == 0){
            time_t elapsed=time(NULL)-startinitresponse;
	    char str[MAXLINE];
	    sprintf(str,"%ld:converged\n", elapsed);
            fprintf(file, str);
	    fflush(file);
	flag=1;
        }
	pthread_mutex_unlock(&lock);
	
    }

}


int main(int argc, char **argv) {

	int udport= atoi(argv[4]); /* the server listens on a port passed 
			   on the command line */
    sprintf(routerlogfile,"router%s.log",argv[1]);
	file=fopen(routerlogfile,"w");
	id=(atoi(argv[1]));
	 listenudp=open_listenudp(udport);

	struct pkt_INIT_REQUEST init_request;
	init_request.router_id = htonl(id);  


	struct pkt_INIT_RESPONSE init_response;
	char *hostname=argv[2];
	int port=atoi(argv[3]);

	if ((hp = gethostbyname(hostname)) == NULL) 
	return -2;
	bzero((char *) &serveraddr, sizeof(serveraddr)); 
	serveraddr.sin_family = AF_INET; 
	bcopy((char *)hp->h_addr,  
	(char *)&serveraddr.sin_addr.s_addr, hp->h_length); 
	serveraddr.sin_port = htons(port);
	
	int sent= sendto(listenudp,&init_request , sizeof(init_request), 0, &serveraddr,size);
	int bytesread= recvfrom(listenudp,&init_response, sizeof(init_response),0, &serveraddr, &size);
	startinitresponse=time(NULL);
	ntoh_pkt_INIT_RESPONSE (&init_response);    
	InitRoutingTbl(&init_response, atoi(argv[1]));

	PrintRoutes(file,id);

    no_nbr=init_response.no_nbr;
    int i;
    for(i=0;i<no_nbr;i++){
        arr[i].nbrID=init_response.nbrcost[i].nbr;
        arr[i].costToNbr=init_response.nbrcost[i].cost;
        arr[i].timeAt=time(NULL);
	arr[i].alive = 1;
    }
	convergenceTime=time(NULL);
	//THREADING
	pthread_mutex_init(&lock, NULL);
	pthread_t udpThread;
	pthread_t timerThread;	
	pthread_create(&udpThread, NULL, udpFunction, NULL);
	pthread_create(&timerThread,NULL,timerFunction,NULL);

	pthread_join(udpThread,NULL);
	pthread_join(timerThread,NULL);
	fclose(file);

}
