/*
 *  vho_3g.c
 *  Receives 3G data from pre-handoff, calculates QEV for parameters
 *  and gives the QDV of a network
 * 
 *  (c) SeaMo, version 0.1, 2011, ECE Department, IISc, Bangalore &
 *  Department of IT, MCIT, Government of India
 *
 *  Copyright (c) 2009 - 2011
 *  MIP Project group, ECE Department, Indian
 *  Institute of Science, Bangalore and Department of IT, Ministry of
 *  Communications and IT, Government of India. All rights reserved.
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *    
 *  Authors: Seema K   <seema at eis.iisc.ernet.in>
 *           Anand SVR <anand at eis.iisc.ernet.in>
 *          
 *  See the file "license.terms" for information on usage and redistribution
 *  of this file.
 */     

/***************************** INCLUDES *****************************/
#include "vho_3g.h"
#include "vho.h"

/**************************** VARIABLES ******************************/
struct Ginfo data;
struct node_3g *networks_3g;	/* Linked list to hold the networks */
int MAX_DEVICES = 5;
void average3gdata(struct node_3g *net3g);



/**************************** 3G THREAD ***************************/

/* Thread function which receives the data from the pre-handoff phase
 * and stores in the queue. */

void *threeg_data()
{
	int sockfd, n, len_send, addr_len, ret;
	char msg[20] = "Hello";
	struct sockaddr_in server_addr, client_addr;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Socket");
		exit(1);
	}

        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");;
        client_addr.sin_port = htons(55557);

        ret = bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr));
        if(ret < 0) {
        perror ("ThreeG bind failed\n");
        }

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(55556);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	bzero(&(server_addr.sin_zero), 8);

	len_send = strlen(msg);
	addr_len = sizeof(server_addr);

	/* Send a message to unblock the sender (pre-handoff), so that the sender
	 * can send the data continuously to the VHO phase */

        printf ("Sending Hello!\n");

	sendto(sockfd, msg, len_send, 0, (struct sockaddr *)&server_addr,
	       addr_len);

	/* Receive the data continuously */

	n = recvfrom(sockfd, &data, sizeof(data), 0, NULL, NULL);
	while (n) {
		received_3g = 1;
//                    printf("\n(vho_3g) : %s, %f, %d\n", data.apn,data.level, data.bandwidth);
		/* Add the received data to the queue */
		add_to_3gqueue(data);
#ifdef DEBUG_L1
		syslog(LOG_INFO,
		       "\nCLIENT: apn = %s, level = %f, bandwidth = %d\n",
		       data.apn, data.level, data.bandwidth);
#endif

		n = recvfrom(sockfd, &data, sizeof(data), 0, NULL, NULL);
	}
	return 0;
}

/* Add the data obtained from the pre-handoff to a circular queue.
 * @param1: Ginfo structure data obtained from pre-handoff
 */

void add_to_3gqueue(struct Ginfo data)
{
	static int rear = -1;

	if (q3count == MAX_QUEUE) {
		return;
	} else {
		rear = (rear + 1) % MAX_QUEUE;
		strcpy(queue[rear].apn, data.apn);
		queue[rear].bandwidth = data.bandwidth;
		queue[rear].level = data.level;
		queue[rear].cost = data.cost;
		queue[rear].up = data.up;
		q3count = q3count + 1;
	}
}

/************************* END OF THREAD ********************************/

/**************************** SUBROUTINES ******************************/

void observe_3g(const char *apn)
{
        struct node_3g *temp_3g;
       temp_3g = networks_3g;

        /* Traverse the linked list to get the index of current network
 *          * and observe the current network */

        while (temp_3g != NULL) {
                if ((!strcmp(temp_3g->apn,apn )))
                        observe_network_3g(temp_3g);
                temp_3g = temp_3g->link;
        }
}


void observe_network_3g(struct node_3g *current_net)
{
        struct node_3g *temp_3g;
        int i, count = 0;
        temp_3g = current_net;

        for (i = 0; i < MAX_DEVICES ; i++) {
                if (temp_3g->avg_data[i].level > temp_3g->avg_data[i + 1].level)
                        count++;
                if (temp_3g->avg_data[i].bandwidth >
                    temp_3g->avg_data[i + 1].bandwidth)
                        count++;
        }

        if (count >= 1)
                vho_trigger();
}




int threeg_param()
{

	/* If the thread has not received any data from the
	 * 3G modem return -1 */

	if (!received_3g) {
		return -1;
        }

	/* Read a data set from the queue */
	read_3g_queue();

#ifdef DEBUG_L2
	struct node_3g *temp_3g;
	int i;

	temp_3g = networks_3g;
	while (temp_3g != NULL) {
		syslog(LOG_INFO, "Network apn = %s\n", temp_3g->apn);
//		printf("Network apn = %s\n", temp_3g->apn);
		for (i = 0; i < 4; i++)
			syslog(LOG_INFO, " level = %f, bandwidth = %d\n",
			       temp_3g->data[i].level,
			       temp_3g->data[i].bandwidth);
/*			printf(" level = %f, bandwidth = %d\n",
			       temp_3g->data[i].level,
			       temp_3g->data[i].bandwidth);
*/		temp_3g = temp_3g->link;
	}
	temp_3g = networks_3g;
	while (temp_3g != NULL) {
		syslog(LOG_INFO, "AVG Network\napn = %s", temp_3g->apn);
//		printf("AVG Network apn = %s", temp_3g->apn);
		syslog(LOG_INFO, " level = %f,bandwidth = %d\n",
		       temp_3g->avg_data[2].level,
		       temp_3g->avg_data[2].bandwidth);
		temp_3g = temp_3g->link;
	}
#endif

	return 1;
}

reset_apn_data(char *apn)
{
	struct node_3g *temp_3g;
	int i;

	temp_3g = networks_3g;

        while (temp_3g != NULL) {
                if (!strcmp(temp_3g->apn, apn)) {
                
                for (i = 0; i < 4; i++) {
                               temp_3g->data[i].level = -115;
                               temp_3g->data[i].bandwidth = 0;

                   }
                   break;
                 }
                temp_3g = temp_3g->link;
        }
}

void read_3g_queue()
{
	struct Ginfo new_data;

	while (q3count == 0) {
		sleep(1);
	}

        while (q3count) {

        if (queue[front].level <= -115.0) {
             printf ("%s Ignored, dongle absent ?\n", queue[front].apn);
             reset_apn_data(queue[front].apn);
        }

	strcpy(new_data.apn, queue[front].apn);
	new_data.level = queue[front].level;
	new_data.bandwidth = queue[front].bandwidth;
	new_data.cost = queue[front].cost;
	new_data.up = queue[front].up;

	front = (front + 1) % MAX_QUEUE;
	q3count = q3count - 1;


	/* If the linked list is empty, then add the new network into the list */

	if (networks_3g == NULL)
		add_3g_network(new_data);
	else {
		/* If there is already networks stored in the list, collect the data
		 * and store in the list */

		collect3gdata(new_data);
	}
     }
}

/* Stores the data to the linked list in the appropriate index */

void collect3gdata(struct Ginfo new_data)
{
	int found = 0, i;
	struct node_3g *temp_3g;
	temp_3g = networks_3g;
	while (temp_3g != NULL) {

		/* Traverse through the list to find if the network for the
		 * obtained data is already added.*/

		if (!strcmp(temp_3g->apn, new_data.apn)) {
			found = 1;

			/* We are storing 4 sets of values inorder to average them.
			 * The Obtained latest value is stored at the end of the array
			 * and the oldest value is ignored */

			for (i = 0; i < 3; i++) {
				temp_3g->data[i].level =
				    temp_3g->data[i + 1].level;
				temp_3g->data[i].bandwidth =
				    temp_3g->data[i + 1].bandwidth;
			}
			temp_3g->data[i].level = new_data.level;
			temp_3g->data[i].bandwidth = new_data.bandwidth;
			average3gdata(temp_3g);
		}
		temp_3g = temp_3g->link;
	}
	if (found == 0)
		add_3g_network(new_data);
}

void average3gdata(struct node_3g *net3g)
{
	int i, j, count_3g = 0;

	/* We maintain 3 successive sets of averaged data in order to trigger the
	 * algorithm. The oldest averaged value is ignored. */

	for (i = 0; i < 2; i++) {
		net3g->avg_data[i].level = net3g->avg_data[i + 1].level;
		net3g->avg_data[i].bandwidth = net3g->avg_data[i + 1].bandwidth;
	}
	net3g->avg_data[2].level = net3g->avg_data[2].bandwidth = 0;

	/* Calculate the no of raw data stored to average accordingly */

	for (i = 0; i < 4; i++)
		if (net3g->data[i].level != 0.0)
			count_3g++;

	/* Averaging the 4 sets of data stored */

	if (count_3g) {
		for (j = 0; j < 4; j++) {
			net3g->avg_data[2].level += net3g->data[j].level;
			net3g->avg_data[2].bandwidth +=
			    net3g->data[j].bandwidth;
		}
		net3g->avg_data[2].level = net3g->avg_data[2].level / count_3g;
		net3g->avg_data[2].bandwidth =
		    net3g->avg_data[2].bandwidth / count_3g;
	}
}

/* Add a new network into the linked list.*/

void add_3g_network(struct Ginfo newdata)
{
	struct node_3g *temp_3g;
	int i;

	temp_3g = (struct node_3g *)malloc(sizeof(struct node_3g));

	strcpy(temp_3g->apn, newdata.apn);
	temp_3g->cost = newdata.cost;
	temp_3g->up = newdata.up;

	/* Initialize the data structure */

	temp_3g->data[0].level = temp_3g->data[1].level =
	    temp_3g->data[2].level = 0;
	temp_3g->data[0].bandwidth = temp_3g->data[1].bandwidth =
	    temp_3g->data[2].bandwidth = 0;

	temp_3g->data[3].level = newdata.level;
	temp_3g->data[3].bandwidth = newdata.bandwidth;
	for (i = 0; i < 3; i++)
		temp_3g->avg_data[i].level = temp_3g->avg_data[i].bandwidth = 0;

	temp_3g->link = NULL;

	/* Add a node to the end of the linked list */

	if (networks_3g == NULL)
		networks_3g = temp_3g;
	else {
		struct node_3g *cur_3g;
		cur_3g = networks_3g;
		while (cur_3g->link != NULL) {
			cur_3g = cur_3g->link;
		}
		cur_3g->link = temp_3g;
	}
	average3gdata(temp_3g);
}

/* Get the qdv value for the 3G network. In case of multiple
 * wifi networks, we return the network with highest QDV.
 * @param1: Buffer to store the qdv
 * @param2: buffer to store the APN name
 */

char* get_3g_qdv(float *qdv_3g, char *apn)
{
	struct node_3g *temp_3g;
	temp_3g = networks_3g;
	float qev[5], qev_bandwidth[5], qev_cost[5], qdv[5];
	int i = 0, max = 0;
        int j=1;
	qev_level_3g(qev);
	qev_bw_3g(qev_bandwidth);
	qev_cost_3g(qev_cost);
	qdv_3g_networks(qev, qev_bandwidth, qev_cost, qdv);  
  
	while (temp_3g != NULL) {
		if (qdv[i] >= qdv[max]) {
			max = i;
			strcpy(apn, temp_3g->apn);
                        printf("QDV of the 3g provider %s = %f\n ",apn,qdv[i]);
		}
		i++;
		temp_3g = temp_3g->link;
	}
	*qdv_3g = qdv[max];
printf("*****QDV of the BEST 3G provider %s = %f\n ",apn,qdv[max]);
return apn;
}

/************************************************** QEV Calculations ******************************************/
void qev_level_3g(float qev[])
{
	int i = 0;
	struct node_3g *temp_3g;
	temp_3g = networks_3g;
	while (temp_3g != NULL) {
		int k;
		float a[1][5] = { {0, 0, 0, 0, 0} };
		float NUErssi[5][1] = { {0}, {0.25}, {0.5}, {0.75}, {1} };

		qev[i] = 0;

		if (temp_3g->avg_data[2].level < RSSI_3G_TH) {
			a[0][0] = 1;
		} else if (RSSI_3G_TH <= temp_3g->avg_data[2].level
			   && temp_3g->avg_data[2].level < RSSI_3G_LOW) {
			a[0][0] =
			    -(temp_3g->avg_data[2].level -
			      RSSI_3G_LOW) / (float)(RSSI_3G_LOW - RSSI_3G_TH);
			a[0][1] =
			    (temp_3g->avg_data[2].level -
			     RSSI_3G_TH) / (float)(RSSI_3G_LOW - RSSI_3G_TH);
		} else if (RSSI_3G_LOW <= temp_3g->avg_data[2].level
			   && temp_3g->avg_data[2].level < RSSI_3G_MED) {
			a[0][1] =
			    -(temp_3g->avg_data[2].level -
			      RSSI_3G_MED) / (float)(RSSI_3G_MED - RSSI_3G_LOW);
			a[0][2] =
			    (temp_3g->avg_data[2].level -
			     RSSI_3G_LOW) / (float)(RSSI_3G_MED - RSSI_3G_LOW);
		} else if (RSSI_3G_MED <= temp_3g->avg_data[2].level
			   && temp_3g->avg_data[2].level < RSSI_3G_HIGH) {
			a[0][2] =
			    -(temp_3g->avg_data[2].level -
			      RSSI_3G_HIGH) / (float)(RSSI_3G_HIGH -
						      RSSI_3G_MED);
			a[0][3] =
			    (temp_3g->avg_data[2].level -
			     RSSI_3G_MED) / (float)(RSSI_3G_HIGH - RSSI_3G_MED);
		} else if (RSSI_3G_HIGH <= temp_3g->avg_data[2].level
			   && temp_3g->avg_data[2].level < RSSI_3G_VHIGH) {
			a[0][3] =
			    -(temp_3g->avg_data[2].level -
			      RSSI_3G_VHIGH) / (float)(RSSI_3G_VHIGH -
						       RSSI_3G_HIGH);
			a[0][4] =
			    (temp_3g->avg_data[2].level -
			     RSSI_3G_HIGH) / (float)(RSSI_3G_VHIGH -
						     RSSI_3G_HIGH);
		} else if (temp_3g->avg_data[2].level >= RSSI_3G_VHIGH) {
			a[0][4] = 1;
		}
		for (k = 0; k < 5; k++) {
			qev[i] = qev[i] + a[0][k] * NUErssi[k][0];
		}
		temp_3g = temp_3g->link;
		i++;
	}
}

void qev_bw_3g(float qev_bandwidth[])
{
	int i = 0;
	struct node_3g *temp_3g;
	temp_3g = networks_3g;
	while (temp_3g != NULL) {
		int k;
		float a[1][5] = { {0, 0, 0, 0, 0} };
		float NUEbandwidth[5][1] = { {0}, {0.25}, {0.5}, {0.75}, {1} };

		qev_bandwidth[i] = 0;
		if (temp_3g->avg_data[2].bandwidth <= BW_3G_TH) {
			a[0][0] = 1;
		} else if (BW_3G_TH <= temp_3g->avg_data[2].bandwidth
			   && temp_3g->avg_data[2].bandwidth < BW_3G_LOW) {
			a[0][0] =
			    -(temp_3g->avg_data[2].bandwidth -
			      BW_3G_LOW) / (float)(BW_3G_LOW - BW_3G_TH);
			a[0][1] =
			    (temp_3g->avg_data[2].bandwidth -
			     BW_3G_TH) / (float)(BW_3G_LOW - BW_3G_TH);
		} else if (BW_3G_LOW <= temp_3g->avg_data[2].bandwidth
			   && temp_3g->avg_data[2].bandwidth < BW_3G_MED) {
			a[0][1] =
			    -(temp_3g->avg_data[2].bandwidth -
			      BW_3G_MED) / (float)(BW_3G_MED - BW_3G_LOW);
			a[0][2] =
			    (temp_3g->avg_data[2].bandwidth -
			     BW_3G_LOW) / (float)(BW_3G_MED - BW_3G_LOW);
		} else if (BW_3G_MED <= temp_3g->avg_data[2].bandwidth
			   && temp_3g->avg_data[2].bandwidth < BW_3G_HIGH) {
			a[0][2] =
			    -(temp_3g->avg_data[2].bandwidth -
			      BW_3G_HIGH) / (float)(BW_3G_HIGH - BW_3G_MED);
			a[0][3] =
			    (temp_3g->avg_data[2].bandwidth -
			     BW_3G_MED) / (float)(BW_3G_HIGH - BW_3G_MED);
		} else if (BW_3G_HIGH <= temp_3g->avg_data[2].bandwidth
			   && temp_3g->avg_data[2].bandwidth < BW_3G_VHIGH) {
			a[0][3] =
			    -(temp_3g->avg_data[2].bandwidth -
			      BW_3G_VHIGH) / (float)(BW_3G_VHIGH - BW_3G_HIGH);
			a[0][4] =
			    (temp_3g->avg_data[2].bandwidth -
			     BW_3G_HIGH) / (float)(BW_3G_VHIGH - BW_3G_HIGH);
		} else if (temp_3g->avg_data[2].bandwidth >= BW_3G_VHIGH) {
			a[0][4] = 1;
		}
		for (k = 0; k < 5; k++) {
			qev_bandwidth[i] =
			    qev_bandwidth[i] + a[0][k] * NUEbandwidth[k][0];
		}
		i++;
		temp_3g = temp_3g->link;
	}
}

void qev_cost_3g(float qev_cost[])
{
	int i = 0;
	struct node_3g *temp_3g;
	temp_3g = networks_3g;
	while (temp_3g != NULL) {
		int k;
		float a[1][5] = { {0, 0, 0, 0, 0} };
		float NUEcost[5][1] = { {1}, {0.75}, {0.5}, {0.25}, {0} };

		qev_cost[i] = 0;
		if (temp_3g->cost <= COST_TH) {
			a[0][0] = 1;
		} else if (COST_TH <= temp_3g->cost && temp_3g->cost < COST_LOW) {
			a[0][0] =
			    -(temp_3g->cost - COST_LOW) / (float)(COST_LOW -
								  COST_TH);
			a[0][1] =
			    (temp_3g->cost - COST_TH) / (float)(COST_LOW -
								COST_TH);
		} else if (COST_LOW <= temp_3g->cost
			   && temp_3g->cost < COST_MED) {
			a[0][1] =
			    -(temp_3g->cost - COST_MED) / (float)(COST_MED -
								  COST_LOW);
			a[0][2] =
			    (temp_3g->cost - COST_LOW) / (float)(COST_MED -
								 COST_LOW);;
		} else if (COST_MED <= temp_3g->cost
			   && temp_3g->cost < COST_HIGH) {
			a[0][2] =
			    -(temp_3g->cost - COST_HIGH) / (float)(COST_HIGH -
								   COST_MED);
			a[0][3] =
			    (temp_3g->cost - COST_MED) / (float)(COST_HIGH -
								 COST_MED);
		} else if (COST_HIGH <= temp_3g->cost
			   && temp_3g->cost < COST_VHIGH) {
			a[0][3] =
			    -(temp_3g->cost - COST_VHIGH) / (float)(COST_VHIGH -
								    COST_HIGH);
			a[0][4] =
			    (temp_3g->cost - COST_HIGH) / (float)(COST_VHIGH -
								  COST_HIGH);
		} else if (temp_3g->cost >= COST_VHIGH) {
			a[0][4] = 1;
		}
		for (k = 0; k < 5; k++) {
			qev_cost[i] = qev_cost[i] + a[0][k] * NUEcost[k][0];
		}
		i++;
		temp_3g = temp_3g->link;
	}
}

/********************************************** END OF QEV Calculations ****************************************/

void qdv_3g_networks(float qev[], float qev_bandwidth[], float qev_cost[],
		     float qdv[])
{
        char* apn_name; 
	struct node_3g *temp_3g;
	float uweight;
	temp_3g = networks_3g;
	int i = 0;

	/* Add extra weights for the networks based on the user preference */
/*	switch (temp_3g->up) {
/	case 1:
		uweight = 0.2;
		break;
	case 2:
		uweight = 0.1; 
		break;
	default:
		uweight = 0.0;
	}
*/
/*suhas  	while (temp_3g != NULL) {
    
            apn_name = temp_3g->apn;
		qdv[i] =
		    RSSI_WT_3G * qev[i] + BW_WT_3G * qev_bandwidth[i] +
		    COST_WT * qev_cost[i];
                printf( "QDV of provider%d,%s = %f\n",i,apn_name,qdv[i]);
         	i++;
		temp_3g = temp_3g->link;
	}
*/

   while (temp_3g != NULL) {
    
            apn_name = temp_3g->apn;
                qdv[i] =
                    RSSI_WT_3G * qev[i] + BW_WT_3G * qev_bandwidth[i] +
                    0 * qev_cost[i];

             printf("-----------------------vho_3g_start---------------------------\n");

             printf( "QEV OF 3G apn=%s\t : rssi %f\t, bandwidth %f\t, cost %f\n-------QDV = %f",apn_name
                       ,qev[i], qev_bandwidth[i], qev_cost[i],qdv[i]);
      
             printf("-----------------------vho_3g_end---------------------------\n");


                i++;
                temp_3g = temp_3g->link;
        }

/*
#ifdef DEBUG_L2
	i = 0;
	temp_3g = networks_3g;
	while (temp_3g != NULL) {
		syslog(LOG_INFO, "QEV OF 3G: rssi %f, bandwidth %f, cost %f\n",
		       qev[i], qev_bandwidth[i], qev_cost[i]);
		syslog(LOG_INFO, "QDV %f\n", qdv[i]);
                printf( "QDV of provider%d = %f\n",i+1, qdv[i]);
		i++;
		temp_3g = temp_3g->link;
	}
#endif  */
}
