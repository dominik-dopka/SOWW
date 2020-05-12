#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define PRECISION 0.000001
#define RANGESIZE 1
#define DATA 0
#define RESULT 1
#define FINISH 2

#define TABLE_SIZE 100000

main(int argc, char** argv)
{
	int myrank, proccount;
	int a = 0, b = TABLE_SIZE;
	int result = 0, resulttemp;
	int i;
	int package_size;
	MPI_Status status;

	struct timeval start, end;

	srand(time(0));

	// Initialize MPI
	MPI_Init(&argc, &argv);

	// find out my rank
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	// find out the number of processes in MPI_COMM_WORLD
	MPI_Comm_size(MPI_COMM_WORLD, &proccount);

	if (proccount < 2)
	{
		printf("Run with at least 2 processes");
		MPI_Finalize();
		return -1;
	}

	if (myrank == 0)		//MASTER
	{
		//int table_size;
		//printf("Type in desired table size: ");
		//scanf("%d", )

		int table[TABLE_SIZE];

		for (int i = 0; i < size; i++)
			table[i] = rand();

		printf("Table size: %d \n \n Please type in desired package size: ", TABLE_SIZE);
		scanf("%d", &package_size);


		for (i = 1; i < proccount; i++) //powiadom o rozmiarze paczek
			MPI_Send(&package_size, 1, MPI_INT, i, DATA, MPI_COMM_WORLD);

		int* package = malloc(package_size * sizeof(int));

		gettimeofday(&start, 0);

		// first distribute some ranges to all slaves
		for (i = 1; i < proccount; i++)
		{
			for (int j = 0; j < package_size; j++)
			{
				package[j] = table[a + j];
			}
			a += package_size;

			MPI_Send(package, package_size, MPI_INT, i, DATA, MPI_COMM_WORLD);
		}
		do
		{
			// distribute remaining subranges to the processes which have completed their parts
			MPI_Recv(&resulttemp, 1, MPI_INT, MPI_ANY_SOURCE, RESULT, MPI_COMM_WORLD, &status);
			result += resulttemp;

			for (int j = 0; j < package_size; j++)
			{
				package[j] = table[a + j];
			}
			a += package_size;

			MPI_Send(package, package_size, MPI_INT, status.MPI_SOURCE, DATA, MPI_COMM_WORLD);
		} while (a < b);

		// now receive results from the processes
		for (i = 0; i < (proccount - 1); i++)
		{
			MPI_Recv(&resulttemp, 1, MPI_INT, MPI_ANY_SOURCE, RESULT, MPI_COMM_WORLD, &status);
			result += resulttemp;
		}
		// shut down the slaves
		for (i = 1; i < proccount; i++)
		{
			MPI_Send(NULL, 0, MPI_DOUBLE, i, FINISH, MPI_COMM_WORLD);
		}
		// now display the result
		gettimeofday(&end, 0);
		long time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
		printf("\nResult %d, Time: %ld ms, wynik to %d\n", result, time);
	}
	else			//SLAVE
	{
		//pobranie info o rozmiarze paczek
		MPI_Recv(&package_size, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD, &status);
		int* package = malloc(package_size * sizeof(int));

		// this is easy - just receive data and do the work
		do
		{
			MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			if (status.MPI_TAG == DATA)
			{
				MPI_Recv(package, package_size, MPI_INT, 0, DATA, MPI_COMM_WORLD, &status);
				// compute my part

				resulttemp = 0;
				for (int i = 0; i < package_size; i++)
				{
					resulttemp += package[i];

					// additional computation to generate time
					for (int j = 0; j < 10; j++)
					{
						sin(package[i]) * sin(package[i]) / package[i];
					}
				}

				// send the result back
				MPI_Send(&resulttemp, 1, MPI_INT, 0, RESULT, MPI_COMM_WORLD);
			}
		} while (status.MPI_TAG != FINISH);
	}

	// Shut down MPI
	MPI_Finalize();

	return 0;
}



