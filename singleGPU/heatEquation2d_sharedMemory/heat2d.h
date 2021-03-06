
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define DEBUG 0 // Display all error messages
#define NX 1024 // number of cells in the x-direction
#define NY 1024 // number of cells in the y-direction
#define L 10.0 // domain length
#define W 10.0 // domain width
#define C 1.0 // c, material conductivity. Uniform assumption.
#define TEND 1.0 // tEnd, output time
#define DX (L/NX) // dx, cell size
#define DY (W/NY) // dy, cell size
#define DT (1/(2*C*(1/DX/DX+1/DY/DY))) // dt, fix time step size
#define KX (C*DT/(DX*DX)) // numerical conductivity
#define KY (C*DT/(DY*DY)) // numerical conductivity
#define NO_STEPS (TEND/DT) // No. of time steps
#define PI 3.1415926535897932f
#define NI 128 // block size in the i-direction
#define NJ 4 // block size in the j-direction
#define RADIUS 1 // cells width of the hallo region
#define I2D(n,i,j) ((i)+(n)*(j)) // transfrom a 2D array index pair into linear index memory
#define DIVIDE_INTO(x,y) (((x)+(y)-1)/(y)) // define No. of blocks/warps

// set USE_CPU to 1 to run only on CPU
// set USE_GPU to 1 to use GPU kernel - without shared mem
// set USE_GPU to 2 to use GPU kernel - with shared mem_v2 (without halo regions)
// set USE_GPU to 3 to use GPU kernel - with shared mem_v3 (with halo regions)
// set USE_GPU to 4 to use GPU kernel - with shared mem_v4 (with halo regions v2)
// set USE_GPU to 5 to use GPU kernel - with shared mem_v5 (128x4 tile for coalescing optimization)
// set USE_GPU to 6 to use GPU kernel - with texture memory (doesn't work :[ )
#define USE_CPU 0  // set 1 to select the CPU kernel 
#define USE_GPU 6  // select the No. of GPU kernel to use

/* Declare functions */
void Manage_Memory(int phase, float **h_u, float **h_un, float **d_u, float **d_un);
void Manage_Comms(int phase, float **h_u, float **d_u);

void Call_Init(float **h_u);
void Call_CPU_Laplace(float **h_u,float **h_un);
void Call_GPU_Laplace(float **d_u,float **d_un);

void Save_Results(float *h_u);
void Save_Time(float *t);
