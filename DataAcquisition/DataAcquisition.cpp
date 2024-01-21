#include "DataAcquisition.h"

using namespace std;

DataAcquisition* DataAcquisition::data = nullptr;// Static pointer to the current instance

// Signal handler for SIGINT (Ctrl+C)
static void signalHandler(int signum) {
	switch (signum) {
	case SIGINT:
		cout << "DataAcquisition shutting down..." << endl;
		break;
	}
}

// Setting up signal handler for SIGINT
void DataAcquisition::setSignalHandler() {

	struct sigaction action;
	action.sa_handler = signalHandler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);
}

// Constructor for DataAcquisition
DataAcquisition::DataAcquisition() {
	is_running = false;
	ShmPTR = nullptr;
	DataAcquisition::data = this;// Set the static pointer to this instance
}

// Set up shared memory
void DataAcquisition::setSharedMemory() {
	ShmKey = ftok(MEMNAME, 65);  // Get key for shared memory
	ShmID = shmget(ShmKey, sizeof(struct SeismicMemory), IPC_CREAT | 0666); // Create shared memory segment
	if (ShmID < 0) {
		cout << "Seismic[ShmID] Error: " << strerror(errno) << endl;
		exit(-1);
	}

	ShmPTR = (struct SeismicMemory*)shmat(ShmID, NULL, 0); // Attach shared memory segment
	if (ShmPTR == (void*)-1) {
		cout << "Seismic[ShmPTR] Error: " << strerror(errno) << endl;
		exit(-1);
	}
	sem_id1 = sem_open(SEMNAME, O_CREAT, SEM_PERMS, 0);// Get semaphore for shared memory synchronization

	if (sem_id1 == SEM_FAILED) {
		cout << "Seismic[ShmPTR] Error: " << strerror(errno) << endl;
		exit(-1);
	}

	is_running = true;

}

// Set up socket for data transfer
void DataAcquisition::socketSetup() {
	const char LOCALHOST[] = "127.0.0.1";
	const int PORT = 1153;
	struct sockaddr_in sv_addr;
	int ret;
	sv_sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);// Create UDP socket
	if (sv_sock == -1) {
		cout << "Scoket creation failed. Error: " << strerror(errno) << endl;
		exit(-1);
	}

	memset(&sv_addr, 0, sizeof(sv_addr));// Zero out server address struct
	sv_addr.sin_family = AF_INET;// IPv4
	sv_addr.sin_port = htons(PORT);// Port to listen on
	ret = inet_pton(AF_INET, LOCALHOST, &sv_addr.sin_addr);// Convert address to binary format
	if (ret < 0) {
		cerr << strerror(errno) << endl;
		exit(ret);

	}

	ret = bind(sv_sock, (struct sockaddr*)&sv_addr, sizeof(sv_addr));// Bind socket to address
	if (ret < 0) {
		cerr << strerror(errno) << endl;
		exit(ret);

	}


}

// Create threads for data transfer
void DataAcquisition::createThreads() {
	int ret;
	// Initialize a mutex lock to protect the critical section
	pthread_mutex_init(&lock_x, NULL);

	// Create a thread to receive data
	ret = pthread_create(&read_tid, NULL, &DataAcquisition::recv_func, this);
	if (ret < 0) {
		cerr << strerror(errno) << endl;
		exit(ret);

	}

	// Create a thread to send data
	ret = pthread_create(&write_tid, NULL, &DataAcquisition::send_func, this);
	if (ret < 0) {
		cerr << strerror(errno) << endl;
		exit(ret);

	}

}

// This function reads data from the shared memory
void DataAcquisition::readMemory() {

	int dataIdx = 0;
	cout << "Starting to read shared memory... " << endl;
	// Loop while is_running flag is true
	while (is_running) {
		// Check if the data is written to the shared memory
		if (ShmPTR->seismicData[dataIdx].status == WRITTEN) {
			// Create a data packet
			struct DataPacket Packet;
			// Wait for a semaphore signal to access shared resources
			sem_wait(sem_id1);
			// Copy data from shared memory to the data packet
			Packet.data = string(ShmPTR->seismicData[dataIdx].data);
			Packet.packetLen = ShmPTR->seismicData[dataIdx].packetLen;
			Packet.packetNo = uint8_t(ShmPTR->packetNo);

			// Mark the data as read
			ShmPTR->seismicData[dataIdx].status = READ;
			sem_post(sem_id1);

			// Push the data packet to a queue
			pthread_mutex_lock(&lock_x);
			packet.push(Packet);
			pthread_mutex_unlock(&lock_x);

			// Push the data packet to a queue
			++dataIdx;
			if (dataIdx > NUM_DATA) dataIdx = 0;
		}

		sleep(1);
	}

	// Set the is_running flag to false
	is_running = false;
	// Close the server socket
	close(sv_sock);
	// Close the semaphore
	sem_close(sem_id1);
	// Remove the semaphore from the system
	sem_unlink(SEMNAME);

}

// This is a static function that is used as a thread for receiving data
void* DataAcquisition::recv_func(void* arg) {

	cout << "Receive thread started... " << endl;
	// Cast the argument to the appropriate data type
	DataAcquisition* data = (DataAcquisition*)arg;
	int ret = 0;
	struct sockaddr_in cl_addr;
	socklen_t cl_addr_len = sizeof(cl_addr);
	char IP_addr[INET_ADDRSTRLEN];
	int port;
	char buf[BUF_LEN];
	memset(buf, 0, BUF_LEN);

	// Loop while is_running flag is true
	while (data->is_running) {
		// Receive data from the server socket
		memset(&cl_addr, 0, sizeof(cl_addr));
		ret = recvfrom(data->sv_sock, buf, BUF_LEN, 0, (struct sockaddr*)&cl_addr, &cl_addr_len);

		if (ret > 0) {
			// Extract the IP address and port number of the client from the received address structure
			memset(IP_addr, 0, INET_ADDRSTRLEN);
			inet_ntop(AF_INET, &(cl_addr.sin_addr), IP_addr, INET_ADDRSTRLEN);
			port = ntohs(cl_addr.sin_port);
			string key = string(IP_addr) + ":" + to_string(port);

			// Authenticate the client if it is not in the list of authenticated clients
			if (data->list2.find(key) == data->list2.end()) {
				data->authenticate(buf, &cl_addr, data->sv_sock);
			}
			memset(&buf, 0, BUF_LEN);

		}
	}
	cout << "Receive thread exiting... " << endl;
	//exit the thread
	pthread_exit(NULL);


}

// This function runs as a separate thread and sends data to clients
void* DataAcquisition::send_func(void* arg) {
	int ret;
	DataAcquisition* data = (DataAcquisition*)arg;
	cout << "Send thread started... " << endl;
	struct sockaddr_in cl_addr;
	DataPacket* packet;
	std::map<std::string, Subscriber> subscribers;
	while (data->is_running) {

		// If there is data in the packet queue, process it
		if (!data->packet.empty()) {
			packet = &(data->packet.front());

			// Copy the current list of subscribers into a local variable to prevent the list from changing during iteration
			pthread_mutex_lock(&(data->lock_x));
			subscribers = data->subscribers;
			pthread_mutex_unlock(&(data->lock_x));

			// Iterate through the list of subscribers and send the data packet to each one
			for (auto it = data->subscribers.begin(); it != data->subscribers.end(); it++) {
				cout << "dataPacket.size(): " << data->packet.size() << " client.size(): " << data->subscribers.size() << endl;
				// Set up the address structure to send the packet to the subscriber
				memset(&cl_addr, 0, sizeof(cl_addr));
				cl_addr.sin_family = AF_INET;
				cl_addr.sin_port = htons(it->second.port);
				ret = inet_pton(AF_INET, it->second.IP_addr, &cl_addr.sin_addr);
				if (ret < 0) {
					cerr << strerror(errno) << endl;
					exit(ret);

				}
				char IP_addr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(cl_addr.sin_addr), IP_addr, INET_ADDRSTRLEN);

				cout << "send_func():" << IP_addr << ":" << ntohs(cl_addr.sin_port) << endl;

				// Create a buffer to hold the packet data and copy the data into it
				char buf[BUF_LEN + 3];
				buf[0] = packet->packetNo & 0xFF;
				buf[1] = (packet->packetLen >> 8) & 0XFF;
				buf[2] = packet->packetLen & 0xFF;
				memcpy(buf + 3, packet->data.c_str(), packet->packetLen);

				// Send the packet to the subscriber
				sendto(data->sv_sock, (char*)buf, BUF_LEN + 3, 0, (struct sockaddr*)&cl_addr, sizeof(cl_addr));

			}

			// Remove the packet from the queue since it has been sent to all subscribers
			pthread_mutex_lock(&data->lock_x);
			data->packet.pop();
			pthread_mutex_unlock(&data->lock_x);
		}
		// Sleep for a second before checking for more packets to send
		sleep(1);
	}

	cout << " Send thread exiting... " << endl;
	// Exit the thread
	pthread_exit(NULL);
}

// This is a member function of the DataAcquisition class that is responsible for authenticating subscriber requests.
void DataAcquisition::authenticate(char cl_msg[BUF_LEN], struct sockaddr_in* cl_addr, int sv_sock) {

	// Constant strings for the password, action subscribe, action cancel and reply message
	const char password[] = "Leaf";
	const char subscribe[] = "Subscribe";
	const char cancel[] = "Cancel";
	const char subscribed[] = "Subscribed";

	// Constant integers for the indices of different fields in the message
	const int cmdIndex = 0;
	const int usernameIndex = 1;
	const int passwordIndex = 2;

	// Char arrays to hold IP address
	char IP_addr[INET_ADDRSTRLEN];
	// Convert IP address to string and get port number
	inet_ntop(AF_INET, &(cl_addr->sin_addr), IP_addr, INET_ADDRSTRLEN);
	int port = ntohs(cl_addr->sin_port);
	// If port is invalid or IP address is 0.0.0.0, return without doing anything
	if (port == 0 || IP_addr == "0.0.0.0") return;

	// Create a key using IP address and port 
	string key = string(IP_addr) + ":" + to_string(port);

	const int MAX_MSG = 3;
	char extractedMsgs[MAX_MSG][BUF2_LEN];
	// Extract different fields from subscriber's message
	int idx = 0;
	char* token = strtok(cl_msg, ",");
	while (idx < MAX_MSG) {
		// Initialize the extracted message array
		memset(&extractedMsgs[idx], 0, BUF2_LEN);
		// If token is NULL, copy an empty string to extracted message array, else copy the token
		if (token == NULL) memcpy(&extractedMsgs[idx], "", BUF2_LEN);
		else memcpy(&extractedMsgs[idx], token, BUF2_LEN);

		// Get the next token
		token = strtok(NULL, ",");
		idx++;
	}

	// Create a subscriber object from extracted fields
	Subscriber sub;
	memcpy(&sub.username, &extractedMsgs[usernameIndex], BUF2_LEN);
	memcpy(&sub.IP_addr, &IP_addr, INET_ADDRSTRLEN);
	sub.port = port;

	// If the command is cancel, remove the subscriber from the subscribers map and print a message
	if (strcmp(extractedMsgs[cmdIndex], cancel) == 0) {
		pthread_mutex_lock(&lock_x);
		subscribers.erase(key);
		pthread_mutex_unlock(&lock_x);
		cout << "username:" << sub.username << " has Cancelled. " << endl;
	}

	// If the command is subscribe and the password matches, add the subscriber to the subscribers map and print a message
	if ((strcmp(extractedMsgs[cmdIndex], subscribe) == 0) && (strcmp(extractedMsgs[passwordIndex], password) == 0)) {

		// If the subscriber is not already in the map, add it and send a reply message
		if (list1.find(key) == list1.end()) {

			pthread_mutex_lock(&lock_x);
			subscribers[key] = sub;
			pthread_mutex_unlock(&lock_x);

			cout << extractedMsgs[usernameIndex] << "has subscribed." << endl;

			sendto(sv_sock, subscribed, sizeof(subscribed), 0, (struct sockaddr*)cl_addr, sizeof(*cl_addr));

		}
		// If the subscriber is already in the map, print a message
		else cout << extractedMsgs[usernameIndex] << " has already subscribed." << endl;

	}
	// If the client message is not a "Subscribe" or "Cancel" command, print an error message and update list1 and list2 maps
	if ((strcmp(extractedMsgs[cmdIndex], subscribe) != 0) && strcmp(extractedMsgs[cmdIndex], cancel) != 0) {
		cout << "DataAcquisition: unknown command " << extractedMsgs[cmdIndex] << endl;
		// If the subscriber has already sent 3 invalid requests, move them to list2 and remove them from list1
		list1[key] = (list1.find(key) != list1.end()) ? list1[key] + 1 : 1;

		if (list1[key] >= 3) {
			list2[key] = sub;
			pthread_mutex_lock(&lock_x);
			subscribers.erase(key);
			pthread_mutex_unlock(&lock_x);
		}

	}
	// Check if the received message is a cancellation request but with an incorrect password
	else if (((strcmp(extractedMsgs[passwordIndex], password) != 0) && (strcmp(extractedMsgs[cmdIndex], cancel) != 0)))
	{
		// If the subscriber has already sent 3 invalid requests, move them to list2 and remove them from list1
		list1[key] = (list1.find(key) != list1.end()) ? list1[key] + 1 : 1;

		if (list1[key] >= 3) {
			list2[key] = sub;
			pthread_mutex_lock(&lock_x);
			subscribers.erase(key);
			pthread_mutex_unlock(&lock_x);
		}
	}

}


