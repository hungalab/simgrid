/* Copyright (c) 2015-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <stdio.h>
#include <mpi.h>
#include <simgrid/modelchecker.h>

int x;

int main(int argc, char **argv) {
  int recv_buff;
  int size;
  int rank;
  MPI_Status status;

  MPI_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &size);   /* Get nr of tasks */
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);   /* Get id of this process */

  MC_ignore(&status.count, sizeof status.count);

  if (rank == 0) {
    while (1) {
      MPI_Recv(&recv_buff, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    }
  } else {
    while (1) {
      x = 2;
      MPI_Send(&rank, 1, MPI_INT, 0, 42, MPI_COMM_WORLD);
    }
  }

  MPI_Finalize();

  return 0;
}
