
#include "heat2d.h"

void Manage_Memory(int phase, int tid, float **h_u, float **d_u, float **d_un){
  if (phase==0) {
    // Allocate whole domain in host (master thread)
    *h_u = (float*)malloc(NY*NX*sizeof(float));
  }
  if (phase==1) {
    // Allocate whole domain in device (GPU thread)
    cudaError_t Error = cudaSetDevice(tid);
    if (DEBUG) printf("CUDA error (cudaSetDevice) = %s\n",cudaGetErrorString(Error));
    Error = cudaMalloc((void**)d_u ,NY*NX*sizeof(float));
    if (DEBUG) printf("CUDA error (cudaMalloc) = %s\n",cudaGetErrorString(Error));
    Error = cudaMalloc((void**)d_un,NY*NX*sizeof(float));
    if (DEBUG) printf("CUDA error (cudaMalloc) = %s\n",cudaGetErrorString(Error));
  }
  if (phase==2) {
    // Free the whole domain variables (master thread)
    free(*h_u);
    cudaError_t Error;
    Error = cudaFree(*d_u);
    if (DEBUG) printf("CUDA error (cudaFree) = %s\n",cudaGetErrorString(Error));
    Error = cudaFree(*d_un);
    if (DEBUG) printf("CUDA error (cudaFree) = %s\n",cudaGetErrorString(Error));
  }
}

__global__ void SetIC_onDevice(float *u){
  // threads id 
  int i = threadIdx.x + blockIdx.x*blockDim.x;
  int j = threadIdx.y + blockIdx.y*blockDim.y;
  int o = i + NX*j;

  u[o] = 0.0;
  // but ...
  if (i==0)    u[o] = 0.0;
  if (j==0)    u[o] = 0.0;
  if (i==NX-1) u[o] = 1.0;
  if (j==NY-1) u[o] = 1.0;
}

void Call_GPU_Init(float **u0){
  // Load the initial condition
  dim3 threads(16,16);
  dim3 blocks(NX/16,NY/16); 
  SetIC_onDevice<<<blocks, threads>>>(*u0);
}

__global__ void Laplace2d(float *u,float *un){
  // Threads id
  int i = threadIdx.x + blockIdx.x*blockDim.x;
  int j = threadIdx.y + blockIdx.y*blockDim.y;

  int o =  i + NX*j ; // node( j,i )     n
  int n = i+NX*(j+1); // node(j+1,i)     |
  int s = i+NX*(j-1); // node(j-1,i)  w--o--e
  int e = (i+1)+NX*j; // node(j,i+1)     |
  int w = (i-1)+NX*j; // node(j,i-1)     s

  // only update "interior" nodes
  if(i>0 && i<NX-1 && j>0 && j<NY-1) {
    un[o] = u[o] + KX*(u[e]-2*u[o]+u[w]) + KY*(u[n]-2*u[o]+u[s]);
  } else {
    un[o] = u[o];
  }
}

void Call_Laplace(float **d_u, float **d_un) {
  // Produce one iteration of the laplace operator
  dim3 threads(32,32);
  dim3 blocks(NX/32,NX/32); 
  Laplace2d<<<blocks,threads>>>(*d_u,*d_un);
  if (DEBUG) printf("CUDA error (Jacobi_Method) %s\n",cudaGetErrorString(cudaPeekAtLastError()));
  cudaError_t Error = cudaDeviceSynchronize();
  if (DEBUG) printf("CUDA error (Jacobi_Method Synchronize) %s\n",cudaGetErrorString(Error));
}

void Manage_Comms(int phase, int tid, float **h_u, float **d_u) {
  // Manage CPU-GPU communicastions
  if (DEBUG) printf(":::::::: Performing Comms (phase %d) ::::::::\n",phase);
  
  if (phase == 0) {
    // move h_u (from HOST) to d_u (to GPU)
    cudaError_t Error = cudaMemcpy(*d_u,*h_u,NY*NX*sizeof(float),cudaMemcpyHostToDevice);
    if (DEBUG) printf("CUDA error (memcpy h -> d ) = %s\n",cudaGetErrorString(Error));
  }
  if (phase == 1) {
    // move d_u (from GPU) to h_u (to HOST)
    cudaError_t Error = cudaMemcpy(*h_u,*d_u,NY*NX*sizeof(float),cudaMemcpyDeviceToHost);
    if (DEBUG) printf("CUDA error (memcpy d -> h ) = %s\n",cudaGetErrorString(Error));
  }
}

__global__ void Update_Domain(float *u,float *un){
  int i = threadIdx.x + blockIdx.x*blockDim.x;
  int j = threadIdx.y + blockIdx.y*blockDim.y;
  u[i+NX*j] = un[i+NX*j];
}

void Call_Update(float **u, float **un){
  // produce explicitly: u=un;
  dim3 threads(32,32);
  dim3 blocks(NX/32,NX/32); 
  Update_Domain<<<blocks,threads>>>(*u,*un);
  if (DEBUG) printf("CUDA error (Jacobi_Method) %s\n",cudaGetErrorString(cudaPeekAtLastError()));
  cudaError_t Error = cudaDeviceSynchronize();
  if (DEBUG) printf("CUDA error (Jacobi_Method Synchronize) %s\n",cudaGetErrorString(Error));
}

void Save_Results(float *u){
  // print result to txt file
  FILE *pFile = fopen("result.txt", "w");
  if (pFile != NULL) {
    for (int j = 0; j < NY; j++) {
      for (int i = 0; i < NX; i++) {      
	fprintf(pFile, "%d\t %d\t %g\n",j,i,u[i+NX*j]);
      }
    }
    fclose(pFile);
  } else {
    printf("Unable to save to file\n");
  }
}
