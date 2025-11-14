#include <dftracer/service/service.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

// Atomic flag to control server running state
std::atomic<bool> running(true);

// Signal handler for SIGINT to gracefully shut down the server
static void server_signal_handler(int signal) {
  if (signal == SIGINT) {
    running = false;
    std::cout << "\nSIGINT received. Shutting down server..." << std::endl;
  }
}

// Path to the PID file for the server process
const char* PID_FILE = "/tmp/dftracer_server.pid";

// Daemonize the process: detach from terminal and run in background
void daemonize() {
  pid_t pid = fork();
  if (pid < 0) exit(EXIT_FAILURE);  // Fork failed
  if (pid > 0) exit(EXIT_SUCCESS);  // Parent exits

  // Child continues as session leader
  if (setsid() < 0) exit(EXIT_FAILURE);

  pid = fork();
  if (pid < 0) exit(EXIT_FAILURE);  // Second fork failed
  if (pid > 0) exit(EXIT_SUCCESS);  // First child exits

  // Close standard file descriptors
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}

int main(int argc, char* argv[]) {
  // Check for correct usage
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " <start|stop> [log_dir]" << std::endl;
    return 1;
  }

  std::string cmd = argv[1];
  std::string log_dir = (argc == 3) ? argv[2] : "/tmp";

  // Ensure log_dir ends without trailing slash
  if (!log_dir.empty() && log_dir.back() == '/') log_dir.pop_back();

  std::string pid_file_path = log_dir + "/dftracer_server.pid";
  std::string out_log_path = log_dir + "/dftracer_server.out";
  std::string err_log_path = log_dir + "/dftracer_server.err";

  if (cmd == "start") {
    // Start the server as a daemon
    daemonize();

    // Redirect stdout and stderr to log files (truncate on running)
    freopen(out_log_path.c_str(), "w", stdout);
    freopen(err_log_path.c_str(), "w", stderr);

    // Write the server's PID to a file for later reference
    std::ofstream pid_file(pid_file_path);
    pid_file << getpid();
    pid_file.close();

    // Register signal handler for graceful shutdown
    std::signal(SIGINT, server_signal_handler);

    // Create and start the DFTracerService server
    auto server = dftracer::DFTracerService();
    server.start();

    // Main loop: keep running until SIGINT is received
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Stop the server and clean up
    server.stop();

    // Remove the PID file
    std::remove(pid_file_path.c_str());

    return 0;
  } else if (cmd == "stop") {
    // Stop the running server by sending SIGINT to its PID
    std::ifstream pid_file(pid_file_path);
    pid_t pid;
    if (!(pid_file >> pid)) {
      std::cerr << "No running server found." << std::endl;
      return 1;
    }
    pid_file.close();

    // Send SIGINT to the server process
    if (kill(pid, SIGINT) == 0) {
      std::cout << "Sent SIGINT to server (PID " << pid << ")." << std::endl;
      std::remove(pid_file_path.c_str());
    } else {
      std::cerr << "Failed to send SIGINT to server." << std::endl;
      return 1;
    }
    return 0;
  } else {
    // Unknown command
    std::cerr << "Unknown command: " << cmd << std::endl;
    return 1;
  }
}