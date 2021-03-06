#include "heat3d.h"

#define checkCuda(error) __checkCuda(error, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
// A method for checking error in CUDA calls
////////////////////////////////////////////////////////////////////////////////
inline void __checkCuda(cudaError_t error, const char *file, const int line)
{
  #if defined(DEBUG) || defined(_DEBUG)
  if (error != cudaSuccess)
  {
    printf("checkCuda error at %s:%i: %s\n", file, line, cudaGetErrorString(cudaGetLastError()));
    exit(-1);
  }
  #endif

  return;
}

///////////////////////
// Main program entry
///////////////////////
int main(int argc, char** argv)
{
  REAL C;
  unsigned int L, W, H, Nx, Ny, Nz, max_iters, blockX, blockY, blockZ;
  int rank, numberOfProcesses;

  if (argc == 12)
  {
    C = atof(argv[1]); // conductivity, here it is assumed: Cx = Cy = Cz = C.
    L = atoi(argv[2]); // domain lenght
    W = atoi(argv[3]); // domain width 
    H = atoi(argv[4]); // domain height
    Nx = atoi(argv[5]); // number cells in x-direction
    Ny = atoi(argv[6]); // number cells in x-direction
    Nz = atoi(argv[7]); // number cells in x-direction
    max_iters = atoi(argv[8]); // number of iterations / time steps
    blockX = atoi(argv[9]); // block size in the i-direction
    blockY = atoi(argv[10]); // block size in the j-direction
    blockZ = atoi(argv[11]); // block size in the k-direction
  }
  else
  {
    printf("Usage: %s diffCoef L W H nx ny nz i block_x block_y block_z\n", argv[0]);
    exit(1);
  }

  InitializeMPI(&argc, &argv, &rank, &numberOfProcesses);
  AssignDevices(rank);
  ECCCheck(rank);

  unsigned int R; REAL dx, dy, dz, dt, kx, ky, kz, tFinal;

  dx = (REAL)L/(Nx+1); // dx, cell size
  dy = (REAL)W/(Ny+1); // dy, cell size
  dz = (REAL)H/(Nz+1); // dz, cell size
  dt = 1/(2*C*(1/dx/dx+1/dy/dy+1/dz/dz)); // dt, fix time step size
  kx = C*dt/(dx*dx); // numerical conductivity
  ky = C*dt/(dy*dy); // numerical conductivity
  kz = C*dt/(dz*dz); // numerical conductivity
  tFinal = dt*max_iters; //printf("Final time: %g\n",tFinal);
  R = 1; // halo regions width (in cells size)

  // Copy constants to Constant Memory on the GPUs
  CopyToConstantMemory(kx, ky, kz);

  // Decompose along the z-axis
  const int _Nz = Nz/numberOfProcesses;
  const int dt_size = sizeof(REAL);

  // Host memory allocations
  REAL *u_new, *u_old;
  REAL *h_Uold;

  u_new = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(Nz+2));
  u_old = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(Nz+2));

  if (rank == 0) h_Uold = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(Nz+2)); 

  init(u_old, u_new, h, Nx, Ny, Nz);

  // Allocate and generate host subdomains
  REAL *h_s_Uolds, *h_s_Unews, *h_s_rbuf[numberOfProcesses];
  REAL *left_send_buffer, *left_receive_buffer;
  REAL *right_send_buffer, *right_receive_buffer;

  h_s_Unews = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(_Nz+2));
  h_s_Uolds = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(_Nz+2));

#if defined(DEBUG) || defined(_DEBUG)
  if (rank == 0)
  {
    for (int i = 0; i < numberOfProcesses; i++)
    {
        h_s_rbuf[i] = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(_Nz+2));
        checkCuda(cudaHostAlloc((void**)&h_s_rbuf[i], dt_size*(Nx+2)*(Ny+2)*(_Nz+2), cudaHostAllocPortable));
    }
  }
#endif

    right_send_buffer = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(RADIUS));
    left_send_buffer = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(RADIUS));
    right_receive_buffer = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(RADIUS));
    left_receive_buffer = (REAL*)malloc(sizeof(REAL)*(Nx+2)*(Ny+2)*(RADIUS));

    checkCuda(cudaHostAlloc((void**)&h_s_Unews, dt_size*(Nx+2)*(Ny+2)*(_Nz+2), cudaHostAllocPortable));
    checkCuda(cudaHostAlloc((void**)&h_s_Uolds, dt_size*(Nx+2)*(Ny+2)*(_Nz+2), cudaHostAllocPortable));

    checkCuda(cudaHostAlloc((void**)&right_send_buffer, dt_size*(Nx+2)*(Ny+2)*(RADIUS), cudaHostAllocPortable));
    checkCuda(cudaHostAlloc((void**)&left_send_buffer, dt_size*(Nx+2)*(Ny+2)*(RADIUS), cudaHostAllocPortable));
    checkCuda(cudaHostAlloc((void**)&right_receive_buffer, dt_size*(Nx+2)*(Ny+2)*(RADIUS), cudaHostAllocPortable));
    checkCuda(cudaHostAlloc((void**)&left_receive_buffer, dt_size*(Nx+2)*(Ny+2)*(RADIUS), cudaHostAllocPortable));

    init_subdomain(h_s_Uolds, u_old, Nx, Ny, _Nz, rank);

    // GPU Memory Operations
    size_t pitch_bytes, pitch_gc_bytes;

    REAL *d_s_Unews, *d_s_Uolds;
    REAL *d_right_send_buffer, *d_left_send_buffer;
    REAL *d_right_receive_buffer, *d_left_receive_buffer;

    checkCuda(cudaMallocPitch((void**)&d_s_Uolds, &pitch_bytes, dt_size*(Nx+2), (Ny+2)*(_Nz+2)));
    checkCuda(cudaMallocPitch((void**)&d_s_Unews, &pitch_bytes, dt_size*(Nx+2), (Ny+2)*(_Nz+2)));

    checkCuda(cudaMallocPitch((void**)&d_left_send_buffer, &pitch_gc_bytes, dt_size*(Nx+2), (Ny+2)*(RADIUS)));
    checkCuda(cudaMallocPitch((void**)&d_left_receive_buffer, &pitch_gc_bytes, dt_size*(Nx+2), (Ny+2)*(RADIUS)));
    checkCuda(cudaMallocPitch((void**)&d_right_send_buffer, &pitch_gc_bytes, dt_size*(Nx+2), (Ny+2)*(RADIUS)));
    checkCuda(cudaMallocPitch((void**)&d_right_receive_buffer, &pitch_gc_bytes, dt_size*(Nx+2), (Ny+2)*(RADIUS)));

  // Copy subdomains from host to device and get walltime
  double HtD_timer = 0.;

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
  HtD_timer -= MPI_Wtime();
  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  checkCuda(cudaMemcpy2D(d_s_Uolds, pitch_bytes, h_s_Uolds, dt_size*(Nx+2), dt_size*(Nx+2), ((Ny+2)*(_Nz+2)), cudaMemcpyDefault));
  checkCuda(cudaMemcpy2D(d_s_Unews, pitch_bytes, h_s_Unews, dt_size*(Nx+2), dt_size*(Nx+2), ((Ny+2)*(_Nz+2)), cudaMemcpyDefault));

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
  HtD_timer += MPI_Wtime();
  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  int pitch = pitch_bytes/dt_size;
  int gc_pitch = pitch_gc_bytes/dt_size;

  // GPU kernel launch parameters
  dim3 threads_per_block(blockX, blockY, blockZ);
  unsigned int blocksInX = getBlock(Nx, blockX);
  unsigned int blocksInY = getBlock(Ny, blockY);
  unsigned int blocksInZ = getBlock(_Nz-2, k_loop);

  dim3 thread_blocks(blocksInX, blocksInY, blocksInZ);
  dim3 thread_blocks_halo(blocksInX, blocksInY);

  // MPI Status
  MPI_Status status[numberOfProcesses];
  MPI_Request gather_send_request[numberOfProcesses];
  MPI_Request right_send_request[numberOfProcesses], left_send_request[numberOfProcesses];
  MPI_Request right_receive_request[numberOfProcesses], left_receive_request[numberOfProcesses];

  double compute_timer = 0.;

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
  compute_timer -= MPI_Wtime();
  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  for(unsigned int iterations = 0; iterations < max_iters; iterations++) {
  // Compute inner nodes
  ComputeInnerPoints(thread_blocks, threads_per_block, d_s_Unews, d_s_Uolds, pitch, Nx, Ny, _Nz);

  // Send data to 1-3 from 0-2
  if (rank < numberOfProcesses-1) {
    CopyBoundaryRegionToGhostCell(thread_blocks_halo, threads_per_block, d_s_Unews, d_right_send_buffer, Nx, Ny, _Nz, pitch, gc_pitch, 0);
    checkCuda(cudaMemcpy2D(right_send_buffer, dt_size*(Nx+2), d_right_send_buffer, pitch_gc_bytes, dt_size*(Nx+2), (Ny+2)*(RADIUS), cudaMemcpyDefault));

    MPI_Isend(right_send_buffer, (Nx+2)*(Ny+2)*(RADIUS), MPI_CUSTOM_REAL, rank+1, 0, MPI_COMM_WORLD, &right_send_request[rank]);
  }

  // Send data to 0-2 from 1-3
  if (rank > 0) {
    CopyBoundaryRegionToGhostCell(thread_blocks_halo, threads_per_block, d_s_Unews, d_left_send_buffer, Nx, Ny, _Nz, pitch, gc_pitch, 1);
    checkCuda(cudaMemcpy2D(left_send_buffer, dt_size*(Nx+2), d_left_send_buffer, pitch_gc_bytes, dt_size*(Nx+2), (Ny+2)*(RADIUS), cudaMemcpyDefault));

    MPI_Isend(left_send_buffer, (Nx+2)*(Ny+2)*(RADIUS), MPI_CUSTOM_REAL, rank-1, 1, MPI_COMM_WORLD, &left_send_request[rank]);
  }

  // Receive data from 0-2
  if (rank > 0) {
    MPI_Irecv(left_receive_buffer, (Nx+2)*(Ny+2)*(RADIUS), MPI_CUSTOM_REAL, rank-1, 0, MPI_COMM_WORLD, &left_receive_request[rank]);
  }

  // Receive data from 0-2
  if (rank < numberOfProcesses-1) {
    MPI_Irecv(right_receive_buffer, (Nx+2)*(Ny+2)*(RADIUS), MPI_CUSTOM_REAL, rank+1, 1, MPI_COMM_WORLD, &right_receive_request[rank]);
  }

  // Receive data from 0-2
  if (rank > 0) {
    MPI_Wait(&left_receive_request[rank], MPI_STATUS_IGNORE);
    checkCuda(cudaMemcpy2D(d_left_receive_buffer, pitch_gc_bytes, left_receive_buffer, dt_size*(Nx+2), dt_size*(Nx+2), ((Ny+2)*(RADIUS)), cudaMemcpyDefault));
    CopyGhostCellToBoundaryRegion(thread_blocks_halo, threads_per_block, d_s_Unews, d_left_receive_buffer, Nx, Ny, _Nz, pitch, gc_pitch, 1);
  }

  if (rank < numberOfProcesses-1) {
    MPI_Wait(&right_receive_request[rank], MPI_STATUS_IGNORE);

    checkCuda(cudaMemcpy2D(d_right_receive_buffer, pitch_gc_bytes, right_receive_buffer, dt_size*(Nx+2), dt_size*(Nx+2), ((Ny+2)*(RADIUS)), cudaMemcpyDefault));
    CopyGhostCellToBoundaryRegion(thread_blocks_halo, threads_per_block, d_s_Unews, d_right_receive_buffer, Nx, Ny, _Nz, pitch, gc_pitch, 0);
  }

  if (rank < numberOfProcesses-1) { 
    MPI_Wait(&right_send_request[rank], MPI_STATUS_IGNORE);
  }

  if (rank > 0) {
    MPI_Wait(&left_send_request[rank], MPI_STATUS_IGNORE);
  }

  // Swap pointers on the host
  checkCuda(cudaDeviceSynchronize());
  swap(REAL*, d_s_Unews, d_s_Uolds);
  }

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
  compute_timer += MPI_Wtime();
  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  // Copy data from device to host
  double DtH_timer = 0;

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
  DtH_timer -= MPI_Wtime();
  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  checkCuda(cudaMemcpy2D(h_s_Uolds, dt_size*(Nx+2), d_s_Uolds, pitch_bytes, dt_size*(Nx+2), (Ny+2)*(_Nz+2), cudaMemcpyDefault));

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
  DtH_timer += MPI_Wtime();
  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  // Gather results from subdomains
  MPI_CHECK(MPI_Isend(h_s_Uolds, (Nx+2)*(Ny+2)*(_Nz+2), MPI_CUSTOM_REAL, 0, 0, MPI_COMM_WORLD, &gather_send_request[rank]));

  if (rank == 0)
  {
    for (int i = 0; i < numberOfProcesses; i++)
    {
      MPI_CHECK(MPI_Recv(h_s_rbuf[i], (Nx+2)*(Ny+2)*(_Nz+2), MPI_CUSTOM_REAL, i, 0, MPI_COMM_WORLD, &status[rank]));
      merge_domains(h_s_rbuf[i], h_Uold, Nx, Ny, _Nz, i);
    }
  }

  // Calculate on host
#if defined(DEBUG) || defined(_DEBUG)
  if (rank == 0)
  {
    cpu_heat3D(u_new, u_old, c0, c1, max_iters, Nx, Ny, Nz);
  }
#endif

  if (rank == 0)
  {
    float gflops = CalcGflops(compute_timer, max_iters, Nx, Ny, Nz);
    PrintSummary("3D Heat (7-pt)", "Plane sweeping", compute_timer, HtD_timer, DtH_timer, gflops, max_iters, Nx);

    REAL t = max_iters * dt;
    CalcError(h_Uold, u_old, t, h, Nx, Ny, Nz);
  }

  Finalize();

  // Free device memory
  checkCuda(cudaFree(d_s_Unews));
  checkCuda(cudaFree(d_s_Uolds));
  checkCuda(cudaFree(d_right_send_buffer));
  checkCuda(cudaFree(d_left_send_buffer));
  checkCuda(cudaFree(d_right_receive_buffer));
  checkCuda(cudaFree(d_left_receive_buffer));

  // Free host memory
  checkCuda(cudaFreeHost(h_s_Unews));
  checkCuda(cudaFreeHost(h_s_Uolds));

#if defined(DEBUG) || defined(_DEBUG)
  if (rank == 0)
  {
    for (int i = 0; i < numberOfProcesses; i++)
    {
      checkCuda(cudaFreeHost(h_s_rbuf[i]));
    }

    free(h_Uold);
  }
#endif

  checkCuda(cudaFreeHost(left_send_buffer));
  checkCuda(cudaFreeHost(left_receive_buffer));
  checkCuda(cudaFreeHost(right_send_buffer));
  checkCuda(cudaFreeHost(right_receive_buffer));

  checkCuda(cudaDeviceReset());

  free(u_old);
  free(u_new);

  return 0;
}
