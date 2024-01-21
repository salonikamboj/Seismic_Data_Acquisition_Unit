

#include "DataAcquisition.h"

int main(int argc, char const *argv[])
{
    DataAcquisition dataAcquisition;
    dataAcquisition.setSignalHandler(); //set up signal handler
    dataAcquisition.setSharedMemory(); // setup shared memory
    dataAcquisition.socketSetup();  // set up the socket
    dataAcquisition.createThreads(); // create read and write threads
    dataAcquisition.readMemory();   // read the memory and then shutdown
    std::cout << "[DEBUG] Data Acquisition Unit exiting... " << std::endl;
    return 0;
}

