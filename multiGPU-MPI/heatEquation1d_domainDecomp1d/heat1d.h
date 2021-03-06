
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

#define DEBUG 0 // Display all error messages
#define NX 1048576 // 2^20 number of cells in the x-direction 
#define L 10.0 // domain length
#define C 1.0 // c, material conductivity. Uniform assumption.
#define TEND 0.1 // tEnd, output time
#define DX (L/NX) // dx, cell size
#define DT (1/(2*C*(1/DX/DX))) // dt, fix time step size
#define KX (C*DT/(DX*DX)) // numerical conductivity
#define NO_STEPS 10000//(TEND/DT) // No. of time steps
#define R 1 // radius of halo region
#define ROOT 0 // define root processor
#define PI 3.1415926535897932f

// Testing :
// A grid of n subgrids
  /* 
  |    0    |    1    |     |    n    |  mpi_rank
  |---(0)---|---(1)---| ... |---(n)---|  (gpu)
  */

/* MPI Grid size */
#define SX 2 // size in x 

/* use floats of dobles */
#define USE_FLOAT true // set false to use real
#if USE_FLOAT
	#define real	float
	#define MPI_CUSTOM_REAL MPI_FLOAT
#else
	#define real	double
	#define MPI_CUSTOM_REAL MPI_DOUBLE
#endif

/* enviroment variable */
#define USE_OMPI true // set false for MVAPICH2
#if USE_FLOAT
	#define ENV_LOCAL_RANK "OMPI_COMM_WORLD_LOCAL_RANK"
#else
	#define ENV_LOCAL_RANK "MV2_COMM_WORLD_LOCAL_RANK"
#endif

// /* Define CUDA and MPI error checks */
// #define GPU_CHECK(call) {
//     cudaError err = call; 
//     if( cudaSuccess != err) {
//         fprintf(stderr, "Cuda error in file '%s' in line %i : %s.\n",
//                 __FILE__, __LINE__, cudaGetErrorString( err) );
//         exit(-1); } }

// #define MY_CHECK(errorMessage) {
//     cudaError_t err = cudaGetLastError();
//     if( cudaSuccess != err) {
//         fprintf(stderr, "Cuda error: %s in file '%s' in line %i : %s.\n"
//                 errorMessage, __FILE__, __LINE__, cudaGetErrorString( err) );
//         exit(-1); } }

// #define MPI_CHECK(call) {
//     if( call != MPI_SUCCESS) { 
//         fprintf(stderr, "MPI error calling '%s'.\n",call);
//         exit(-1); } }

/* Declare structures */
typedef struct {
	int gpu; // domain associated gpu
	int rank; // global rank
	int npcs; // total number of procs
	int size; // domain size (local)
	int nx; // number of cells in the x-direction (local)
	int rx; // x-rank coordinate
} dmn;

/* Declare C++ functions */
void Manage_Devices();
 dmn Manage_Domain(int rank, int size, int gpu);
void Manage_Memory(int phase, dmn domain, real **h_u, real **t_u, real **d_u, real **d_un);
void Manage_Comms(int phase, dmn domain, real **t_u, real **d_u );
extern "C" void Call_Laplace(dmn domain, real **d_u, real **d_un); 
void Call_IC(int IC, real *h_u);
void Save_Results(real *h_u);
