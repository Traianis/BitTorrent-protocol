# BitTorrent Protocol Simulation

## Overview
This project is a simulation of the BitTorrent protocol using threads and MPI (Message Passing Interface). The BitTorrent protocol enables decentralized file sharing, where peers collaboratively distribute file segments without relying on a central server.

The implementation leverages MPI for inter-process communication and multithreading to handle multiple peer interactions efficiently. The repository includes a test suite to validate the implementation.

## Features
- Peer-to-peer (P2P) file sharing simulation using the BitTorrent protocol.
- Multithreaded implementation for handling concurrent requests.
- MPI-based communication for distributed processing.
- A testing framework with predefined test cases.
- Docker support for easy deployment and testing.

## Project Structure
```
traianis-bittorrent-protocol/
├── bittorrent/
│   ├── README.md          # Additional documentation
│   ├── Dockerfile         # Docker setup for the project
│   ├── local.sh           # Utility script for building, testing, and running the project
│   ├── checker/           # Testing suite
│   │   ├── checker.sh     # Script to validate implementation against test cases
│   │   ├── tests/         # Test cases (input/output files)
│   ├── src/               # Source code
│   │   ├── Makefile       # Compilation rules
│   │   ├── bittorrent-protocol.cpp  # Main implementation
```

## Installation
### Prerequisites
- **C++ Compiler**: A C++ compiler supporting C++11 or newer.
- **MPI Library**: OpenMPI or an equivalent implementation.

### Build Instructions
1. Navigate to the `src/` directory:
   ```sh
   cd bittorrent/src
   ```
2. Compile the project using the provided `Makefile`:
   ```sh
   make
   ```
3. The compiled executable `tema3` will be generated in the same directory.

## Usage
### Running the Simulation
To run the BitTorrent simulation using `mpirun` with 4 processes:
```sh
mpirun -np 4 ./tema3
```

## Testing
The repository includes a testing suite located in `bittorrent/checker/tests/`. The `checker.sh` script automates testing by comparing the program's output against expected results.

To run tests manually:
```sh
cd bittorrent/checker
./checker.sh
```
Each test case contains input files (`in*.txt`) and expected output files (`out*.txt`).


## License
This project is released under the MIT License.

