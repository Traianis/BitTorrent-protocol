# BitTorrent-protocol
This is a simulation of BitTorrent protocol using threads and MPI
BitTorrent is a communication protocol for sharing files, operating on a peer-to-peer (P2P) basis, enabling users to distribute data and electronic files over the Internet in a decentralized manner. To send or receive files, users utilize a BitTorrent client on a computer connected to the Internet and interact with a tracker. BitTorrent trackers provide a list of files available for transfer and enable the client to find users from whom they can download the files. Downloading through BitTorrent is considered faster than HTTP and FTP due to the absence of a central server that could limit bandwidth.

Instead of downloading a file from a single source server, the BitTorrent protocol allows users to join a swarm of nodes to simultaneously upload and download from each other. The distributed file is divided into segments, and as each node receives a new segment of the file, it becomes a source (of that segment) for other clients, freeing the original source from sending that segment to every user desiring a copy. With BitTorrent, the task of distributing the file is shared among those who want it. Thus, the source may only send a single copy of the file, but ultimately distribute it to an unlimited number of clients. Each segment is protected by a cryptographic hash, ensuring that any modification to the segment can be easily detected, preventing accidental or malicious changes.

Segments have a consistent size throughout a single download. Due to the nature of this approach, the download of any file can be interrupted at any time and resumed at a later date without losing previously downloaded information, making BitTorrent useful for transferring large files. This also allows the client to search for available segments at a specific point in time and download them immediately, rather than stopping the download and waiting for the next segment (which might not be available).
