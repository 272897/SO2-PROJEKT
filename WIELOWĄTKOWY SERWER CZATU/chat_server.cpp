#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")

#define MAX_CLIENTS 50
#define BUFFER_SIZE 2048
#define SERVER_PORT 8888

std::vector<std::string> chat_history;
const int MAX_HISTORY = 100; 

// Global variables
bool server_running = true;
std::mutex server_running_mutex;
int client_count = 0;
std::mutex client_count_mutex;

// Custom semaphore implementation using condition variable
class Semaphore {
private:
    std::mutex mutex;
    std::condition_variable condition;
    unsigned int count;

public:
    Semaphore(unsigned int initial_count) : count(initial_count) {}

    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        while (count == 0) {
            condition.wait(lock);
        }
        count--;
    }

    void post() {
        std::unique_lock<std::mutex> lock(mutex);
        count++;
        condition.notify_one();
    }
};

// Class to represent a client
class Client {
public:
    SOCKET socket_fd;
    sockaddr_in address;
    std::string username;
    int id;

    Client(SOCKET sock, sockaddr_in addr, int client_id)
        : socket_fd(sock), address(addr), username("Anonymous"), id(client_id) {}
    
    ~Client() {
        closesocket(socket_fd);
    }
};

// Global variables
std::vector<std::shared_ptr<Client>> clients(MAX_CLIENTS);
std::mutex clients_mutex;
Semaphore connection_semaphore(MAX_CLIENTS);  // Using our own semaphore implementation
SOCKET server_socket;

// Function prototypes
void handle_client(std::shared_ptr<Client> client);
void send_message_to_all(const std::string& message, int sender_id, bool include_sender);
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type);

int main() {
    WSADATA wsaData;
    int result;
    sockaddr_in server_addr;

    // Set up console control handler
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return EXIT_FAILURE;
    }

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Set socket options to reuse address
    BOOL opt = TRUE;
    result = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    if (result == SOCKET_ERROR) {
        std::cerr << "Error setting socket options: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the socket to the address
    result = bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    if (result == SOCKET_ERROR) {
        std::cerr << "Error binding socket: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Listen for connections
    result = listen(server_socket, 10);
    if (result == SOCKET_ERROR) {
        std::cerr << "Error listening: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "===== CHAT SERVER STARTED =====" << std::endl;
    std::cout << "Listening on port " << SERVER_PORT << std::endl;

    // Accept and handle client connections
    while (true) {
        // Check if server is still running
        {
            std::lock_guard<std::mutex> lock(server_running_mutex);
            if (!server_running) {
                break;
            }
        }

        // Wait for semaphore before accepting new connections
        connection_semaphore.wait();

        // Check again if server is still running
        {
            std::lock_guard<std::mutex> lock(server_running_mutex);
            if (!server_running) {
                connection_semaphore.post();
                break;
            }
        }

        // Accept a new connection
        SOCKET client_socket;
        sockaddr_in client_addr;
        int client_size = sizeof(client_addr);
        client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_size);
        
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Error accepting connection: " << WSAGetLastError() << std::endl;
            connection_semaphore.post();
            continue;
        }

        // Find an available slot for the client
        int client_id = -1;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i]) {
                    // Create a new client
                    clients[i] = std::make_shared<Client>(client_socket, client_addr, i);
                    client_id = i;
                    
                    // Increment client count with mutex protection
                    {
                        std::lock_guard<std::mutex> count_lock(client_count_mutex);
                        client_count++;
                    }
                    
                    break;
                }
            }
        }

        if (client_id == -1) {
            std::cerr << "Server full, client rejected" << std::endl;
            closesocket(client_socket);
            connection_semaphore.post();
            continue;
        }

        // Create a thread for the client
        std::thread client_thread(handle_client, clients[client_id]);
        client_thread.detach();  // Detach the thread to run independently

        // Print connection info
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "New connection: " << client_ip << ":" 
                << ntohs(client_addr.sin_port) << " (ID: " << client_id << ")" << std::endl;
    }

    // Clean up and exit
    closesocket(server_socket);
    WSACleanup();
    
    std::cout << "Server shut down" << std::endl;
    return EXIT_SUCCESS;
}

/**
 * Thread function to handle a client connection
 */
void handle_client(std::shared_ptr<Client> client) {
    char buffer[BUFFER_SIZE];
    char username_buffer[32];
    bool username_set = false;
    int bytes_received;
    
    // First message from client should be their username
    bytes_received = recv(client->socket_fd, username_buffer, sizeof(username_buffer), 0);
    if (bytes_received > 0) {
        // Remove trailing newline
        username_buffer[bytes_received] = '\0';
        if (username_buffer[bytes_received - 1] == '\n')
            username_buffer[bytes_received - 1] = '\0';
        
        // Copy username to client struct
        {
           std::lock_guard<std::mutex> lock(clients_mutex);
           for (const auto& msg : chat_history) {
               send(client->socket_fd, msg.c_str(), (int)msg.length(), 0);
           }        
           client->username = username_buffer;
        }
        
        username_set = true;
        
        // Prepare and send welcome message
        std::string welcome_msg = "Server: Welcome " + client->username + " to the chat!\n";
        send(client->socket_fd, welcome_msg.c_str(), (int)welcome_msg.length(), 0);
        
        // Notify other clients about new user
        std::string join_msg = "Server: " + client->username + " has joined the chat\n";
        send_message_to_all(join_msg, client->id, false);
        
        std::cout << "User " << client->username << " (ID: " << client->id << ") set their username" << std::endl;
    }
    
    // If username wasn't set, assign a default one
    if (!username_set) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client->username = "Anonymous" + std::to_string(client->id);
    }
    
    // Handle client messages
    while ((bytes_received = recv(client->socket_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        // Null-terminate the message
        buffer[bytes_received] = '\0';
        
        // Remove trailing newline if present
        if (buffer[bytes_received - 1] == '\n')
            buffer[bytes_received - 1] = '\0';
        
        // Handle client commands
        if (buffer[0] == '/') {
            // Handle /quit command
            if (strncmp(buffer, "/quit", 5) == 0) {
                break;
            }
            // Add more commands here if needed
            continue;
        }
        
        // Format message and broadcast to all clients
        std::string message = client->username + ": " + buffer + "\n";
        send_message_to_all(message, client->id, true); // teraz wysyła też do siebie

        std::cout << "Message from " << client->username << " (ID: " << client->id << "): " << buffer << std::endl;
    }
    
    // Client disconnected
    std::cout << "User " << client->username << " (ID: " << client->id << ") disconnected" << std::endl;
    
    // Send disconnect message to all
    std::string leave_msg = "Server: " + client->username + " has left the chat\n";
    send_message_to_all(leave_msg, client->id, true);
    
    // Remove client from the list
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients[client->id] = nullptr;
        
        // Decrement client count with mutex protection
        {
            std::lock_guard<std::mutex> count_lock(client_count_mutex);
            client_count--;
        }
    }
    
    // Release a spot for a new connection
    connection_semaphore.post();
}

/**
 * Sends a message to all connected clients except the sender
 */
void send_message_to_all(const std::string& message, int sender_id, bool include_sender = false) {
   std::lock_guard<std::mutex> lock(clients_mutex);

   
   if (include_sender || sender_id >= 0) {
       chat_history.push_back(message);
       if (chat_history.size() > MAX_HISTORY) {
           chat_history.erase(chat_history.begin()); 
       }
   }

   
   for (int i = 0; i < MAX_CLIENTS; i++) {
       if (clients[i] && (include_sender || clients[i]->id != sender_id)) {
           if (send(clients[i]->socket_fd, message.c_str(), (int)message.length(), 0) == SOCKET_ERROR) {
               std::cerr << "Error sending message: " << WSAGetLastError() << std::endl;
           }
       }
   }
}

/**
 * Console control handler for graceful shutdown
 */
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        std::cout << "\nShutting down server..." << std::endl;
        
        // Set the running flag to false with mutex protection
        {
            std::lock_guard<std::mutex> lock(server_running_mutex);
            server_running = false;
        }
        
        std::string message = "Server: Server is shutting down...\n";
        
        // Send shutdown message to all clients
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto& client : clients) {
                if (client) {
                    send(client->socket_fd, message.c_str(), (int)message.length(), 0);
                    // Clients will be automatically closed when their shared_ptr is reset
                    client = nullptr;
                }
            }
            
            // Reset client count with mutex protection
            {
                std::lock_guard<std::mutex> count_lock(client_count_mutex);
                client_count = 0;
            }
        }
        
        // Wake up the main thread if it's waiting on accept()
        closesocket(server_socket);
        
        return TRUE;
    }
    return FALSE;
}
