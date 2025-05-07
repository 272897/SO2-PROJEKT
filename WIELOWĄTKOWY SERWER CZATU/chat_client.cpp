#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define BUFFER_SIZE 2048
#define SERVER_PORT 8888

// Global variables
SOCKET client_socket;
bool running = true;
std::mutex running_mutex;
std::mutex cout_mutex; // For thread-safe console output

// Function prototypes
void receive_messages();
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type);

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    int result;
    sockaddr_in server_addr;
    
    // Set up console control handler
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
    
    // Check command line arguments
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        return EXIT_FAILURE;
    }
    
    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return EXIT_FAILURE;
    }
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    // Convert IP address from string to binary form
    result = inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
    if (result <= 0) {
        std::cerr << "Invalid address: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    
    // Connect to server
    result = connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    if (result == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    
    std::cout << "Connected to chat server at " << argv[1] << ":" << SERVER_PORT << std::endl;
    
    // Get username
    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);
    username += '\n'; // Add newline for protocol
    
    // Send username to server
    send(client_socket, username.c_str(), (int)username.length(), 0);
    
    // Create a thread to receive messages
    std::thread recv_thread(receive_messages);
    
    // Main loop for sending messages
    std::string message;
    while (true) {
        // Get message from user
        std::getline(std::cin, message);
        
        // Check if client is still running
        bool should_continue;
        {
            std::lock_guard<std::mutex> lock(running_mutex);
            should_continue = running;
        }
        
        if (!should_continue) {
            break;
        }
        
        // Check if user wants to quit
        if (message == "/quit") {
            std::lock_guard<std::mutex> lock(running_mutex);
            running = false;
            break;
        }
        
        // Send message to server
        message += '\n'; // Add newline for protocol
        send(client_socket, message.c_str(), (int)message.length(), 0);
    }
    
    // Wait for receive thread to finish
    if (recv_thread.joinable()) {
        recv_thread.join();
    }
    
    // Clean up and exit
    closesocket(client_socket);
    WSACleanup();
    std::cout << "Disconnected from server" << std::endl;
    
    return EXIT_SUCCESS;
}

/**
 * Thread function to receive messages from the server
 */
void receive_messages() {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    while (true) {
        // Check if client is still running
        bool should_continue;
        {
            std::lock_guard<std::mutex> lock(running_mutex);
            should_continue = running;
        }
        
        if (!should_continue) {
            break;
        }
        
        // Receive message from server
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        // Check if error or disconnection
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Server closed connection" << std::endl;
            } else {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error receiving message: " << WSAGetLastError() << std::endl;
            }
            
            // Set running to false with mutex protection
            {
                std::lock_guard<std::mutex> lock(running_mutex);
                running = false;
            }
            
            break;
        }
        
        // Null-terminate message and print
        buffer[bytes_received] = '\0';
        
        // Use mutex to prevent output from being interleaved with input
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << buffer << std::flush;
        }
    }
}

/**
 * Console control handler for graceful termination
 */
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        // Set running to false with mutex protection
        {
            std::lock_guard<std::mutex> lock(running_mutex);
            running = false;
        }
        
        closesocket(client_socket);
        std::cout << "\nDisconnected from server" << std::endl;
        WSACleanup();
        return TRUE;
    }
    return FALSE;
}