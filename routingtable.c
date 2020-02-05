#include "ne.h"
#include "router.h"
#include <stdbool.h>


/* ----- GLOBAL VARIABLES ----- */
struct route_entry routingTable[MAX_ROUTERS];
int NumRoutes;


////////////////////////////////////////////////////////////////
void InitRoutingTbl (struct pkt_INIT_RESPONSE *InitResponse, int myID){
	int i=0;
	NumRoutes=0;
	routingTable[i].dest_id=myID;
	routingTable[i].next_hop=myID;
	routingTable[i].cost=0;
	routingTable[i].path_len=1;
	routingTable[i].path[0]=myID;
	NumRoutes++;
	for(i = 1; i < InitResponse->no_nbr+1; i++){
		routingTable[i].dest_id=InitResponse->nbrcost[i-1].nbr;
		routingTable[i].next_hop=InitResponse->nbrcost[i-1].nbr;
		routingTable[i].cost=InitResponse->nbrcost[i-1].cost;
		routingTable[i].path_len=2;
		routingTable[i].path[0]=myID;
		routingTable[i].path[1]=InitResponse->nbrcost[i-1].nbr;
		NumRoutes++;		
	}	
	return;
}

////////////////////////////////////////////////////////////////
int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID){

	int changed = 0;
	int i;
	int j;
	int x;
	int changeFlag = 0;
	int finalCost;
	bool sameDest = false;
	bool forcedBool = false;
	bool splitBool = false;
	int indexMatch;
	int completeCost = 0;
	//unsigned int newCost ;
	for (i = 0; i < RecvdUpdatePacket->no_routes; i++)
	{
	  sameDest = false;
	  forcedBool = false;
	  splitBool = false;
	  for (j = 0; j < NumRoutes ; j++)
	  {
	    sameDest = (routingTable[j].dest_id == RecvdUpdatePacket->route[i].dest_id); //error is here
	    if (sameDest == true)
	      {
		      indexMatch = j;
		      break;
		      }
	    
	  	}
		int flag = 0;

	  if (sameDest == true)
	  {
		
	      forcedBool = (RecvdUpdatePacket->sender_id == routingTable[indexMatch].next_hop);
	      if (forcedBool == true)
	      {
		unsigned int newCost = costToNbr + RecvdUpdatePacket->route[i].cost;
		if (newCost > INFINITY)
	       		newCost = INFINITY;
		
		for(x=0;x<RecvdUpdatePacket->route[i].path_len; x++)
			{
		  		if (routingTable[indexMatch].path[x+1] != RecvdUpdatePacket->route[i].path[x]){
					changed = 1;
					flag = 1;
				}
			}

		if (newCost == routingTable[indexMatch].cost && flag == 0 && newCost != INFINITY){
			
		}
		else{
            if(newCost != routingTable[indexMatch].cost){
                routingTable[indexMatch].cost = newCost;//replace myID with index of found sameDest
                routingTable[indexMatch].path_len = RecvdUpdatePacket->route[i].path_len + 1;
                routingTable[indexMatch].path[0] = myID;

                for(x=0;x<RecvdUpdatePacket->route[i].path_len; x++)
                {
                  routingTable[indexMatch].path[x+1]=RecvdUpdatePacket->route[i].path[x];
                }
                changed = 1;
            }
		}
	      }
		

	     for(x=0; x<RecvdUpdatePacket->route[i].path_len; x++)
	     {
	       if(RecvdUpdatePacket->route[i].path[x] == myID)
		 splitBool = true;
	    
	     }

	     //myID not in path of router 
	      if (splitBool == false)
	      {
		//CHECK IF COST IS LOWER. IF LOWER THAN INCLUDE
		completeCost = costToNbr + RecvdUpdatePacket->route[i].cost;
		if (routingTable[indexMatch].cost > completeCost)
		{
		  	routingTable[indexMatch].cost = costToNbr + RecvdUpdatePacket->route[i].cost;//replace myID with index of found sameDest
			routingTable[indexMatch].path_len = RecvdUpdatePacket->route[i].path_len + 1;
			routingTable[indexMatch].path[0]=myID;
			routingTable[indexMatch].next_hop = RecvdUpdatePacket->sender_id;
			
			for(x=0; x<RecvdUpdatePacket->route[i].path_len; x++)
			  routingTable[indexMatch].path[x+1]=RecvdUpdatePacket->route[i].path[x]; 

			changed = 1;	
	        }
	     }
	 }

	 else //sameDest = false
	 {
	     
	     finalCost = RecvdUpdatePacket->route[i].cost + costToNbr;
	     if (finalCost > INFINITY)
	       finalCost = INFINITY;
	     
	     routingTable[NumRoutes].next_hop = RecvdUpdatePacket->sender_id;
	     routingTable[NumRoutes].cost = finalCost;
	     routingTable[NumRoutes].dest_id = RecvdUpdatePacket->route[i].dest_id;
	     
	     routingTable[NumRoutes].path[0]=myID;
	     routingTable[NumRoutes].path_len = RecvdUpdatePacket->route[i].path_len + 1;
	     for(x=0; x<RecvdUpdatePacket->route[i].path_len; x++)
	       routingTable[NumRoutes].path[x+1]=RecvdUpdatePacket->route[i].path[x];

	     
	     NumRoutes++;
	     changed = 1;
	 }
	      
	
      }
	  
      return changed;
}


////////////////////////////////////////////////////////////////
void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID){

	UpdatePacketToSend->sender_id=myID;
	UpdatePacketToSend->no_routes=NumRoutes;
	int i;
	int j;
	for(i = 0; i < NumRoutes; i++){
		UpdatePacketToSend->route[i].dest_id=routingTable[i].dest_id;
		UpdatePacketToSend->route[i].next_hop=routingTable[i].next_hop;
		UpdatePacketToSend->route[i].cost=routingTable[i].cost;
		unsigned int len=routingTable[i].path_len;
		UpdatePacketToSend->route[i].path_len=len;
		for(j=0;j<len;j++){
			UpdatePacketToSend->route[i].path[j]=routingTable[i].path[j];
		}		
	}
	return;
}


////////////////////////////////////////////////////////////////
void PrintRoutes (FILE* Logfile, int myID){

	int i;
	int j;
	for(i = 0; i < NumRoutes; i++){
		fprintf(Logfile, "<R%d -> R%d> Path: R%d", myID, routingTable[i].dest_id, myID);

		for(j = 1; j < routingTable[i].path_len; j++){
			fprintf(Logfile, " -> R%d", routingTable[i].path[j]);	
		}
		fprintf(Logfile, ", Cost: %d\n", routingTable[i].cost);
	}
	fprintf(Logfile, "\n");
	fflush(Logfile);
}


////////////////////////////////////////////////////////////////
void UninstallRoutesOnNbrDeath(int DeadNbr){
  int x;
  for(x=0; x < NumRoutes; x++)
  {
    if (routingTable[x].next_hop == DeadNbr)
      routingTable[x].cost = INFINITY;
  }
  return;
}
