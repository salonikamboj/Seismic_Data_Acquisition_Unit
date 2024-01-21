# Seismic_Data_Acquisition_Unit

## Project Description
### Overview
The Seismic Data Acquisition Unit project focuses on developing software to efficiently collect and transmit seismic data from a transducer to multiple data centers. The transducer converts seismic waves into binary data streams, which are then transmitted over the network to data centers for analysis. The system is designed to ensure quick data retrieval, password-authenticated subscriptions, and protection against rogue data centers attempting unauthorized access or denial of service attacks.

### Business Case
The release of energy from earthquakes generates seismic waves, offering valuable insights into geological activities. The data acquisition unit serves as a crucial component, swiftly retrieving and transmitting seismic data for analysis, contributing to a better understanding of geologic processes.

### Design
Transducer
  * Manufactured to write seismic data into shared memory.
  * Data streams are of fixed length with included metadata.
  * Controlled shutdown on ctrl-C.
Seismic Data Acquisition Unit
  * Retrieves data from the transducer through shared memory.
  * Uses INET datagram sockets for subscriptions and data transmission.
  * Implements mutexes for synchronization.
  * Detects and handles rogue data centers attempting unauthorized access or denial of service attacks.
Data Center
  * Subscribes to the data acquisition unit using INET datagram sockets with password authentication.
  * Reads seismic data for 30 seconds and cancels subscription.
  * Implements client numbers for IP port determination.
  * Source code includes valid data centers and rogue data centers for testing.

### Implementation
Transducer
  * Writes one packet of seismic data into shared memory each second.
  * Uses synchronization through status bytes and semaphores.
  * Controlled shutdown on ctrl-C.
Seismic Data Acquisition Unit
  * Retrieves data from shared memory, synchronizing with the transducer.
  * Manages a queue of DataPacket structures for efficient data handling.
  * Communicates with data centers via INET datagram sockets.
  * Implements read and write threads for authentication and data transmission.
  * Detects and handles denial of service and brute force attacks by rogue data centers.
Data Center
  * Subscribes to the data acquisition unit, reads data, and cancels subscription.
  * Implements client numbers for IP port determination.
  * Source code includes valid and rogue data centers for testing.

### Testing and Rework
* Test the system by running the provided start batch files for transducer, data acquisition unit, and data centers.
* Observe the data centers receiving data and the system responding to rogue data centers.
* Controlled shutdown of all components after 30 seconds of data transmission.
  
## Data Acquisition Sample Run
![acq](https://github.com/salonikamboj/Seismic_Data_Acquisition_Unit/assets/100891813/2bf812a6-0331-407c-a528-803259dd448e)


## Data Center Sample Run
![center](https://github.com/salonikamboj/Seismic_Data_Acquisition_Unit/assets/100891813/e743332d-c4e7-4085-91ba-17d4a41f477b)


