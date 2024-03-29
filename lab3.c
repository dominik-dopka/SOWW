#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define DATA 0
#define RESULT 1
#define FINISH 2

#define TABLE_SIZE 100000


int main(int argc, char** argv)
{
	int myrank, proccount;
	int a = 0, b = TABLE_SIZE;
	int result = 0;
	int* resulttemp;
	int package_size;

	MPI_Status status;
	MPI_Request* requests;
	int requestcompleted;

	//Initialize MPI
	MPI_Init(&argc, &argv);

	//find out my rank
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	//find out the number of processes in MPI_COMM_WORLD
	MPI_Comm_size(MPI_COMM_WORLD, &proccount);

	if (proccount < 2)
	{
		printf("Run with at least 2 processes");
		MPI_Finalize();
		return -1;
	}

	// now the master will distribute the data and slave processes will perform computations
	if (myrank == 0)
	{
		requests = (MPI_Request*)malloc(3 * (proccount - 1) *
			sizeof(MPI_Request));

		long temp_package_size = strtol(argv[1], NULL, 10);
		package_size = temp_package_size;

		int table[TABLE_SIZE];

		struct timeval start, end;

		srand(time(0));

		for (int i = 0; i < TABLE_SIZE; i++)
			table[i] = abs(rand());

		resulttemp = (int*)malloc((proccount - 1) * sizeof(int));

		int** packages = (int**)malloc((proccount - 1) * sizeof(int*));

		for (int i = 0; i < (proccount - 1); i++)
		{
			packages[i] = (int*)malloc(package_size * sizeof(int));
		}
			
		gettimeofday(&start, 0);

		//first distribute some ranges to all slaves
		for (int i = 1; i < proccount; i++)
		{
			//send package size to slaves for local array creation
			MPI_Send(&package_size, 1, MPI_INT, i, DATA, MPI_COMM_WORLD);

			for (int j = 0; j < package_size; j++)
			{
				packages[i - 1][j] = table[a + j];
			}
			a += package_size;

			MPI_Send(&(packages[i - 1]), package_size, MPI_INT, i, DATA, MPI_COMM_WORLD);
		}

		//the first proccount requests will be for receiving, the latter ones for sending
		for (int i = 0; i < 2 * (proccount - 1); i++)
			requests[i] = MPI_REQUEST_NULL; // none active at this point

		//start receiving results from the slaves
		for (int i = 1; i < proccount; i++)
			MPI_Irecv(&(resulttemp[i - 1]), 1, MPI_INT, i, RESULT, 
				MPI_COMM_WORLD, &(requests[i - 1]));

		//start sending new data parts to the slaves
		for (int i = 1; i < proccount; i++)
		{
			for (int j = 0; j < package_size; j++)
			{
				packages[i - 1][j] = table[a + j];
			}
			a += package_size;

			//send it to process i
			MPI_Isend(&(packages[i - 1]), package_size, MPI_INT, i, DATA, 
				MPI_COMM_WORLD, &(requests[proccount - 2 + i]));
		}

		while (a < b)
		{
			//wait for completion of any of the requests
			MPI_Waitany(2 * proccount - 2, requests, &requestcompleted, 
				MPI_STATUS_IGNORE);

			//if it is a result then send new data to process
			//and add the result
			if (requestcompleted < (proccount - 1))
			{
				result += resulttemp[requestcompleted];

				//first check if the send has terminated
				MPI_Wait(&(requests[proccount - 1 + requestcompleted]), 
					MPI_STATUS_IGNORE);

				//now send some new data portion to this process
				for (int j = 0; j < package_size; j++)
				{
					packages[requestcompleted][j] = table[a + j];
				}				
				a += package_size;

				MPI_Isend(&(packages[requestcompleted]), package_size, MPI_INT, 
					requestcompleted + 1, DATA, MPI_COMM_WORLD, 
					&(requests[proccount - 1 + requestcompleted]));

				// now issue a corresponding recv
				MPI_Irecv(&(resulttemp[requestcompleted]), 1, 
					MPI_INT, requestcompleted + 1, RESULT, 
					MPI_COMM_WORLD, 
					&(requests[requestcompleted]));
			}
		}

		// now send the FINISHING ranges to the slaves
		// shut down the slaves
		int* package = (int*)malloc(package_size * sizeof(int));
		package[0] = -1;

		for (int i = 1; i < proccount; i++)
		{
			MPI_Isend(package, package_size, MPI_INT, i, DATA, MPI_COMM_WORLD, 
				&(requests[2 * proccount - 3 + i]));
		}

		//now receive results from the processes - that is finalize the pending requests
		MPI_Waitall(3 * proccount - 3, requests, MPI_STATUSES_IGNORE);

		//now simply add the results
		for (int i = 0; i < (proccount - 1); i++)
		{
			result += resulttemp[i];
		}

		//now receive results for the initial sends
		for (int i = 0; i < (proccount - 1); i++)
		{
			MPI_Recv(&(resulttemp[i]), 1, MPI_INT, i + 1, RESULT, 
				MPI_COMM_WORLD, &status);
			result += resulttemp[i];
		}

		//now display the result
		gettimeofday(&end, 0);
		long time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);

		printf("\nTime: %ld ms \n\n", time);
	}
	else				//SLAVE
	{
		requests = (MPI_Request*)malloc(2 * sizeof(MPI_Request));

		requests[0] = requests[1] = MPI_REQUEST_NULL;

		//download info about package size
		MPI_Recv(&package_size, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD, &status);

		int* package = (int*)malloc(package_size * sizeof(int));
		int* next_package = (int*)malloc(package_size * sizeof(int));

		//first receive the initial data
		MPI_Recv(package, package_size, MPI_INT, 0, DATA, MPI_COMM_WORLD, &status);

		// check if first element is -1
		//abs in use so only artificial negative number can appear
		while (package[0] != -1)
		{
			//if there is some data to process
			//before computing the next part start receiving a new data part
			MPI_Irecv(next_package, package_size, MPI_INT, 0, DATA, MPI_COMM_WORLD, &(requests[0]));

			//compute my part
			int slave_resulttemp = 0;
			for (int i = 0; i < package_size; i++)
			{
				slave_resulttemp += package[i];

				// additional computation to generate time
				for (int j = 0; j < 100; j++)
				{
					sin(package[i])* sin(package[i]) / package[i];
				}
			}

			//now finish receiving the new part
			//and finish sending the previous results back to master
			MPI_Waitall(2, requests, MPI_STATUSES_IGNORE);

			package = next_package;

			//and start sending the results back
			MPI_Isend(&slave_resulttemp, 1, MPI_INT, 0, RESULT,
				MPI_COMM_WORLD, &(requests[1]));
		}

		// now finish sending the last results to the master
		MPI_Wait(&(requests[1]), MPI_STATUS_IGNORE);
	}

	//Shut down MPI
	MPI_Finalize();

	return 0;
}
