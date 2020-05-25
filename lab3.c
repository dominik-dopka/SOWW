#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define DATA 0
#define RESULT 1
#define FINISH 2

#define ROZMIAR_DANYCH 1000000

int main(int argc, char** argv)
{
	MPI_Status status;
	MPI_Request* requests;
	int requestcompleted;

	int myrank, proccount;

	int a = 0, b = ROZMIAR_DANYCH;
	int** paczki;
	int* paczka; int* paczka2; int* paczka_tmp;
	int rozmiar_paczki;

	int result = 0;
	int* resulttemp;

	int sentcount = 0;
	int recvcount = 0;

	int i, j, w;

	struct timeval czas_start, czas_stop;

	srand(time(0));

	//Initialize MPI
	MPI_Init(&argc, &argv);

	//find out my rank
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	//find out the number of processes in MPI_COMM_WORLD
	MPI_Comm_size(MPI_COMM_WORLD, &proccount);

	if(proccount < 2)
	{
		printf("Run with at least 2 processes");
		MPI_Finalize();
		return -1;
	}

	if(myrank == 0)			//MASTER
	{
		requests = (MPI_Request*)malloc(3*(proccount-1)*sizeof(MPI_Request));
		resulttemp = (int*)malloc((proccount-1)*sizeof(int));

		int problem[ROZMIAR_DANYCH];
		for(i = 0; i < ROZMIAR_DANYCH; i++)
			problem[i] = rand();

		printf("------------------------------------------------\n");
		printf("OVERLAPPING: ON\n\nLiczba intow do zsumowania\n%d\nRozmiarPaczki: \n", ROZMIAR_DANYCH);
		scanf("%d", &rozmiar_paczki);

		paczki = (int**)malloc((proccount-1)*sizeof(int*));
		for(i = 0; i < (proccount-1); i++)
			paczki[i] = (int*)malloc(rozmiar_paczki*sizeof(int));

		for(i = 1; i < proccount; i++) //powiadom o rozmiarze paczek
			MPI_Send(&rozmiar_paczki, 1, MPI_INT, i, DATA, MPI_COMM_WORLD);

		gettimeofday(&czas_start, 0);

		//first distribute some ranges to all slaves
		for(i = 1; i < proccount; i++)
		{
			for(j = 0; j < rozmiar_paczki; j++)
				paczki[i-1][j] = problem[a+j];
			a += rozmiar_paczki;

			MPI_Send(&(paczki[i-1]), rozmiar_paczki, MPI_INT, i, DATA, MPI_COMM_WORLD);
			sentcount++;
		}

		//the first proccount requests will be for receiving, the latter ones for sending
		for(i = 0; i < 2*(proccount-1); i++)
			requests[i]=MPI_REQUEST_NULL;

		//start receiving results from the slaves
		for(i = 1; i < proccount; i++)
			MPI_Irecv(&(resulttemp[i-1]), 1, MPI_INT, i, RESULT, MPI_COMM_WORLD, &(requests[i-1]));

		//start sending new data parts to the slaves
		for(i = 1; i < proccount; i++)
		{
			for(j = 0; j < rozmiar_paczki; j++)
				paczki[i-1][j] = problem[a+j];
			a += rozmiar_paczki;

			//send it to process i
			MPI_Isend(&(paczki[i-1]), rozmiar_paczki, MPI_INT, i, DATA, MPI_COMM_WORLD, &(requests[proccount-2+i]));
			sentcount++;
		}

		while(a < b)
		{
			//wait for completion of any of the requests
			MPI_Waitany(2*proccount-2, requests, &requestcompleted, MPI_STATUS_IGNORE);

			//if it is a result then send new data to process
			//and add the result
			if(requestcompleted < (proccount-1))
			{
				result += resulttemp[requestcompleted];
				recvcount++;

				//first check if the send has terminated
				MPI_Wait(&(requests[proccount-1+requestcompleted]), MPI_STATUS_IGNORE);

				//now send some new data portion to this process
				for(j = 0; j < rozmiar_paczki; j++)
					paczki[requestcompleted][j] = problem[a+j];
				a += rozmiar_paczki;

				MPI_Isend(&(paczki[requestcompleted]), rozmiar_paczki, MPI_INT, requestcompleted+1, DATA, MPI_COMM_WORLD, &(requests[proccount-1+requestcompleted]));
				sentcount++;

				MPI_Irecv(&(resulttemp[requestcompleted]), 1, MPI_INT, requestcompleted+1, RESULT, MPI_COMM_WORLD, &(requests[requestcompleted]));
			}
		}

		//now send the finishing paczka to the slaves
		//shut down the slaves
		paczka = (int*)malloc(rozmiar_paczki*sizeof(int));
		paczka[0]=0;

		for(i = 1; i < proccount; i++)
		{
			MPI_Isend(paczka, rozmiar_paczki, MPI_INT, i, DATA, MPI_COMM_WORLD, &(requests[2*proccount-3+i]));
		}

		//now receive results from the processes - that is finalize the pending requests
		MPI_Waitall(3*proccount-3, requests, MPI_STATUSES_IGNORE);

		//now simply add the results
		for(i = 0; i < (proccount - 1); i++)
		{
			result += resulttemp[i];
			recvcount++;
		}

		//now receive results for the initial sends
		for(i = 0; i < (proccount - 1); i++)
		{
			MPI_Recv(&(resulttemp[i]), 1, MPI_INT, i+1, RESULT, MPI_COMM_WORLD, &status);
			result += resulttemp[i];
			recvcount++;
		}

		//now display the result
		gettimeofday(&czas_stop, 0);
		long czas = (czas_stop.tv_sec - czas_start.tv_sec) * 1000000 + (czas_stop.tv_usec - czas_start.tv_usec);

		printf("\nTu Master, calosc zajela %ld mikrosek., wynik to %d\n", czas, result);
		printf("------------------------------------------------\n");
	}
	else				//SLAVE
	{
		//pobranie info o rozmiarze paczek
		MPI_Recv(&rozmiar_paczki, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD, &status);

		paczka = (int*)malloc(rozmiar_paczki * sizeof(int));
		paczka2= (int*)malloc(rozmiar_paczki * sizeof(int));

		resulttemp = (int*)malloc(2*sizeof(int));
		requests = (MPI_Request*)malloc(2*sizeof(MPI_Request));
		requests[0] = MPI_REQUEST_NULL;
		requests[1] = MPI_REQUEST_NULL;

		//first receive the initial data
		MPI_Recv(paczka, rozmiar_paczki, MPI_INT, 0, DATA, MPI_COMM_WORLD, &status);

		while(paczka[0] != 0)//if there is some data to process
		{
			//before computing the next part start receiving a new data part
			MPI_Irecv(paczka2, rozmiar_paczki, MPI_INT, 0, DATA, MPI_COMM_WORLD, &(requests[0]));

			//compute my part
			resulttemp[1] = 0;
			for(i = 0; i < rozmiar_paczki; i++)
			{
				resulttemp[1] += paczka[i];
				for(w = 0; w < 10; w++) //sztuczne obliczenia
				{
					int whatever = pow(paczka[i], 2);
				}
				if(myrank == 1) //dodatkowe obliczenia dla 1. Slave'a
				{
					for(w = 0; w < 100; w++)
					{
						int whatever = pow(paczka[i], 2);
					}
				}
			}

			//now finish receiving the new part
			//and finish sending the previous results back to master
			MPI_Waitall(2, requests, MPI_STATUSES_IGNORE);

			paczka_tmp = paczka;
			paczka = paczka2;
			paczka2 = paczka_tmp;

			resulttemp[0] = resulttemp[1];

			//and start sending the results back
			MPI_Isend(&(resulttemp[0]), 1, MPI_INT, 0, RESULT, MPI_COMM_WORLD, &(requests[1]));
		}
		MPI_Wait(&(requests[1]), MPI_STATUS_IGNORE);
	}

	//Shut down MPI
	MPI_Finalize();

	return 0;
}
