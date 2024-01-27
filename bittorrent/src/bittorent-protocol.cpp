#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

typedef struct f_data
{
    int file_num;                  // file number
    int rank;                      // client rank
    std::vector<std::string> segs; // segments
} data_cl;

std::vector<data_cl> files_to_down;
std::vector<data_cl> files_own;
int curr_download_file = 0; // current downloading file number

void request_to_tracker(int client_rank, int file, int &size, int &segs_maxim, std::vector<data_cl> &clients)
{
    // Request asking for the list of clients who have the file that needs to be downloaded
    int message[2];
    message[0] = 666;
    message[1] = client_rank;

    MPI_Send(message, 2, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);

    MPI_Send(&file, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);

    // seeds
    MPI_Recv(&size, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    for (int i = 0; i < size; i++)
    {
        data_cl client;
        client.file_num = file;
        int cl_rank = 0;
        MPI_Recv(&cl_rank, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        client.rank = cl_rank;
        int nr_segs = 0;
        MPI_Recv(&nr_segs, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int j = 0; j < nr_segs; j++)
        {
            char new_data_cl[33] = {0};
            MPI_Recv(new_data_cl, 32, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            client.segs.push_back(std::string(new_data_cl));
        }
        clients.push_back(client);
    }

    // peers
    int peers_size = 0;
    MPI_Recv(&peers_size, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    for (int i = 0; i < peers_size; i++)
    {
        data_cl client{};
        MPI_Recv(&client.rank, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        int nr_segs = 0;
        MPI_Recv(&nr_segs, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (int j = 0; j < nr_segs; j++)
        {
            char new_data_cl[33] = {0};
            MPI_Recv(new_data_cl, 32, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            client.segs.push_back(std::string(new_data_cl));
        }

        clients.push_back(client);
    }

    size += peers_size;

    MPI_Recv(&segs_maxim, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

bool find_a_client(int client_rank, int segs_maxim, int &curr_nr, int &br,
                   int size, int file, int &rr, std::vector<data_cl> clients)
{
    for (int z = 0; z < size; z++)
        if (clients[(z + rr) % size].segs.size() > curr_nr)
        {
            // Update the rr
            rr = (z + rr) % size;

            // Request to client rr
            int new_message[2];
            new_message[0] = 888;
            new_message[1] = client_rank;

            MPI_Send(new_message, 2, MPI_INT, clients[rr].rank, clients[rr].rank, MPI_COMM_WORLD);

            MPI_Send(&file, 1, MPI_INT, clients[rr].rank, clients[rr].rank, MPI_COMM_WORLD);

            std::string hash_to_send = clients[rr].segs[curr_nr];

            MPI_Send(hash_to_send.c_str(), 32, MPI_CHAR, clients[rr].rank, clients[rr].rank, MPI_COMM_WORLD);

            char ACK = 0;
            MPI_Recv(&ACK, 1, MPI_CHAR, clients[rr].rank, clients[rr].rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (ACK == 6)
            {
                std::string seg = clients[rr].segs[curr_nr];
                files_to_down[curr_download_file].segs.push_back(seg);
            }

            curr_nr++;

            if (curr_nr % 10 == 0)
            {
                // 10 new segs
                int new_message[2];
                new_message[0] = 777;
                new_message[1] = client_rank;

                MPI_Send(new_message, 2, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                MPI_Send(&file, 1, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                int nr_segs = 10;
                MPI_Send(&nr_segs, 1, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                for (int j = curr_nr - 10; j < curr_nr; j++)
                    MPI_Send(files_to_down[curr_download_file].segs[j].c_str(), 32, MPI_CHAR, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                // Start again
                br = 0;
            }

            // Finished the download
            if (curr_nr == segs_maxim)
                return 1;

            break;
        }
    return 0;
}

void *download_thread_func(void *arg)
{
    int rank = *(int *)arg;

    if (files_to_down.size() == 0)
    {
        // NO FILES TO DOWNLOAD
        int message[2];
        message[0] = 999;
        message[1] = rank;

        MPI_Send(message, 2, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

        return NULL;
    }

    std::vector<std::vector<std::string>> files_str;

    int nr = 0;                                            // current seg that the client needs
    int file = files_to_down[curr_download_file].file_num; // current file
    int stop = 0;

    while (!stop)
    {

        int size = 0;
        int segs_maxim = 0;
        std::vector<data_cl> clients;
        request_to_tracker(rank, file, size, segs_maxim, clients);

        int rr = 0;     // Round Robin
        int br = 1;

        while (br)
        {
            if (find_a_client(rank, segs_maxim, nr, br, size, file, rr, clients))
            {
                // Write the output file
                std::string outfile = "client" + std::to_string(rank) + "_file" + std::to_string(file);

                std::ofstream outputFile(outfile);

                for (int out = 0; out < files_to_down[curr_download_file].segs.size(); out++)
                {
                    outputFile << files_to_down[curr_download_file].segs[out];
                    if (out != files_to_down[curr_download_file].segs.size() - 1)
                        outputFile << "\n";
                }

                // Left segs to send
                if (nr % 10 != 0)
                {
                    int new_message[2];
                    new_message[0] = 777;
                    new_message[1] = rank;
                    MPI_Send(new_message, 2, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                    MPI_Send(&file, 1, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                    int nr_segs = nr - nr / 10 * 10;

                    MPI_Send(&nr_segs, 1, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                    for (int j = nr / 10 * 10; j < nr; j++)
                        MPI_Send(files_to_down[curr_download_file].segs[j].c_str(), 32, MPI_CHAR, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);
                }

                // FINISHED THE FILE message
                int new_message[2];
                new_message[0] = 888;
                new_message[1] = rank;

                MPI_Send(new_message, 2, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                MPI_Send(&file, 1, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);
                nr = 0;
                files_own[files_to_down[curr_download_file].file_num] = files_to_down[curr_download_file];
                curr_download_file++;

                if (curr_download_file < files_to_down.size())
                    // More to download
                    file = files_to_down[curr_download_file].file_num;

                if (curr_download_file == files_to_down.size())
                {
                    // FINISHED ALL FILES
                    int finish_all_file_message[2];
                    finish_all_file_message[0] = 999;
                    finish_all_file_message[1] = rank;

                    MPI_Send(finish_all_file_message, 2, MPI_INT, TRACKER_RANK, TRACKER_RANK, MPI_COMM_WORLD);

                    stop = 1;
                }
                break;
            }
            else
            {
                rr++;
                if (rr % size == 0)
                    rr = 0;
            }
        }
    }
    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int *)arg;
    int message[2] = {0};
    int stop = 0;
    while (!stop)
    {
        // Waiting for a message
        MPI_Recv(message, 2, MPI_INT, MPI_ANY_SOURCE, rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        switch (message[0])
        {
        // Request
        case 888:
        {
            int file_num;
            MPI_Recv(&file_num, 1, MPI_INT, message[1], rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // de verificat
            char new_data_cl[33] = {0};
            MPI_Recv(new_data_cl, 32, MPI_CHAR, message[1], rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (std::find(files_own[file_num].segs.begin(), files_own[file_num].segs.end(), std::string(new_data_cl)) != files_own[file_num].segs.end())
            {
                char ACK = 6;
                MPI_Send(&ACK, 1, MPI_CHAR, message[1], rank, MPI_COMM_WORLD);
            }
            else
            {
                if (std::find(files_to_down[curr_download_file].segs.begin(), files_to_down[curr_download_file].segs.end(), std::string(new_data_cl)) != files_to_down[curr_download_file].segs.end())
                {
                    char ACK = 6;
                    MPI_Send(&ACK, 1, MPI_CHAR, message[1], rank, MPI_COMM_WORLD);
                }
                else
                {
                    char ACK = 2;
                    MPI_Send(&ACK, 1, MPI_CHAR, message[1], rank, MPI_COMM_WORLD);
                }
            }
            break;
        }
        // Tracker announcement
        case 999:
        {
            stop = 1;
            break;
        }
        default:
            break;
        }
    }
    return NULL;
}

void data_receive_tracker(int total_cl, std::vector<std::vector<data_cl>> &files)
{
    // Receive the files from clients
    for (int i = 1; i < total_cl; i++)
    {
        //Notify to start
        std::string message = "start";
        MPI_Send(message.c_str(), 5, MPI_CHAR, i, i, MPI_COMM_WORLD);
        
        int files_nr = 0;
        MPI_Recv(&files_nr, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int j = 0; j < files_nr; j++)
        {
            data_cl client_data;

            int file_nr = 0;
            MPI_Recv(&file_nr, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int nr_segs = 0;
            MPI_Recv(&nr_segs, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            client_data.file_num = file_nr;
            client_data.rank = i;

            for (int k = 0; k < nr_segs; k++)
            {
                char new_data[33] = {0};
                MPI_Recv(new_data, 32, MPI_CHAR, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                client_data.segs.push_back(std::string(new_data));
            }
            

            files[file_nr].push_back(client_data);
        }
    }
}

void req_tracker(int total_cl, int client_rank, std::vector<int> &round_robin,
                 std::vector<std::vector<data_cl>> &files, std::vector<data_cl> &curr_downloading)
{

    int file_nr;
    MPI_Recv(&file_nr, 1, MPI_INT, client_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int k = round_robin[file_nr]; // Round-Robin
    round_robin[file_nr]++;

    std::vector<data_cl> clients_data;
    clients_data = files[file_nr];

    int size = clients_data.size();

    // SEEDS
    MPI_Send(&size, 1, MPI_INT, client_rank, 0, MPI_COMM_WORLD);
    for (int i = 0; i < size; i++)
    {
        int aux = (i + k) % size;
        int rank = clients_data[aux].rank;

        MPI_Send(&rank, 1, MPI_INT, client_rank, 0, MPI_COMM_WORLD);
        int nr = clients_data[aux].segs.size();
        MPI_Send(&nr, 1, MPI_INT, client_rank, 0, MPI_COMM_WORLD);
        for (int j = 0; j < nr; j++)
        {
            std::string seg = clients_data[aux].segs[j];
            MPI_Send(seg.c_str(), 32, MPI_CHAR, client_rank, 100, MPI_COMM_WORLD);
        }
    }

    size = 0;

    // PEERS
    std::vector<data_cl> clients_down;
    for (int i = 1; i < total_cl; i++)
        if (curr_downloading[i].file_num == file_nr && i != client_rank)
        {
            clients_down.push_back(curr_downloading[i]);
            size++;
        }
    MPI_Send(&size, 1, MPI_INT, client_rank, 0, MPI_COMM_WORLD);
    for (int i = 0; i < size; i++)
    {
        MPI_Send(&(clients_down[i].rank), 1, MPI_INT, client_rank, 100, MPI_COMM_WORLD);

        int nr = clients_down[i].segs.size();
        MPI_Send(&nr, 1, MPI_INT, client_rank, 100, MPI_COMM_WORLD);
        for (int j = 0; j < nr; j++)
        {

            std::string seg = clients_down[i].segs[j];
            MPI_Send(seg.c_str(), 32, MPI_CHAR, client_rank, 100, MPI_COMM_WORLD);
        }
    }

    int maxim = clients_data[0].segs.size();
    MPI_Send(&maxim, 1, MPI_INT, client_rank, 0, MPI_COMM_WORLD);
}

void seg_update_tracker(int client_rank, std::vector<std::vector<data_cl>> &files,
                        std::vector<data_cl> &curr_downloading)
{
    // Update nr of segments
    int file_nr;
    MPI_Recv(&file_nr, 1, MPI_INT, client_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int nr_segs = 0;
    MPI_Recv(&nr_segs, 1, MPI_INT, client_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    std::vector<data_cl> data;
    data = files[file_nr];

    curr_downloading[client_rank].file_num = file_nr;
    for (int i = 0; i < nr_segs; i++)
    {
        char new_data[33] = {0};
        MPI_Recv(new_data, 32, MPI_CHAR, client_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        curr_downloading[client_rank].segs.push_back(std::string(new_data));
    }
}

bool all_files_tracker(int total_cl, int client_rank, std::vector<int> &finished)
{
    //All files downloaded
    finished[client_rank] = 1;
    //Check if all clients finished
    if (std::find(finished.begin() + 1, finished.begin() + total_cl, 0) == finished.begin() + total_cl)
    {
        int new_message[2];
        new_message[0] = 999;
        new_message[1] = 0;

        for (int i = 1; i < total_cl; i++)
            MPI_Send(new_message, 2, MPI_INT, i, i, MPI_COMM_WORLD);

        return 1;
    }
    return 0;
}

void tracker(int numtasks, int rank)
{
    std::vector<std::vector<data_cl>> files;
    files.resize(10);
    files.reserve(10);
    std::vector<int> finished; // clients who finished
    finished.resize(10);
    finished.reserve(10);
    std::vector<int> round_robin; // Round Robin
    round_robin.resize(10);
    round_robin.reserve(10);

    std::vector<data_cl> curr_downloading; // clients who are currently downloading
    curr_downloading.resize(numtasks);
    curr_downloading.reserve(numtasks);

    for (int i = 1; i < numtasks; i++)
        curr_downloading[i].rank = i;

    data_receive_tracker(numtasks, files);

    // Notify the clients
    char ACK = 6;
    for (int i = 1; i < numtasks; i++)
        MPI_Send(&ACK, 1, MPI_CHAR, i, i, MPI_COMM_WORLD);

    int message[2] = {0};

    int nr = 3;
    int stop = 0;

    while (!stop)
    {
        // Waiting for a message
        MPI_Recv(message, 2, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        switch (message[0])
        {
            // Request
        case 666:
        {
            req_tracker(numtasks, message[1], round_robin, files, curr_downloading);
            break;
        }
            // Client segments update
        case 777:
        {
            seg_update_tracker(message[1], files, curr_downloading);
            break;
        }
            // Finished the file download
        case 888:
        {
            int file_nr;
            MPI_Recv(&file_nr, 1, MPI_INT, message[1], 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            files[file_nr].push_back(curr_downloading[message[1]]);
            curr_downloading[message[1]].segs.clear();
            break;
        }
            // Finished all files download
        case 999:
        {
            if (all_files_tracker(numtasks, message[1], finished) == 1)
                stop = 1;
            break;
        }
        default:
            break;
        }
    }
}

void data_read_peer(int client_rank, std::string input_file)
{
    std::ifstream MyReadFile(input_file);
    std::string line;

    char wait[6] = {0};
    MPI_Recv(wait, 5, MPI_CHAR, 0, client_rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    getline(MyReadFile, line);

    // Send nr of files
    int nr_files = std::atoi(line.c_str());
    MPI_Send(&nr_files, 1, MPI_INT, TRACKER_RANK, client_rank, MPI_COMM_WORLD);

    // Read the data and send it to tracker
    for (int i = 0; i < nr_files; i++)
    {
        getline(MyReadFile, line);
        data_cl file;

        int file_nr = std::atoi(line.substr(4, line.find(" ")).c_str());
        int nr_segs = std::atoi(line.substr(line.find(" "), line.size()).c_str());
        MPI_Send(&file_nr, 1, MPI_INT, TRACKER_RANK, client_rank, MPI_COMM_WORLD);
        MPI_Send(&nr_segs, 1, MPI_INT, TRACKER_RANK, client_rank, MPI_COMM_WORLD);

        file.file_num = file_nr;
        file.rank = client_rank;

        std::vector<std::string> segs;
        for (int j = 0; j < nr_segs; j++)
        {
            getline(MyReadFile, line);
            MPI_Send(line.c_str(), 32, MPI_CHAR, TRACKER_RANK, client_rank, MPI_COMM_WORLD);
            segs.push_back(line);
        }

        file.segs = segs;

        files_own[file_nr] = file;
    }

    // Read the files to download
    getline(MyReadFile, line);
    int nr = std::atoi(line.c_str());

    for (int i = 0; i < nr; i++)
    {
        getline(MyReadFile, line);
        int file_nr = std::atoi(line.substr(4, line.size()).c_str());
        data_cl file{};
        file.file_num = file_nr;
        file.rank = client_rank;
        files_to_down.push_back(file);
    }
    MyReadFile.close();
}

void peer(int numtasks, int rank)
{
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    files_own.resize(10);
    files_own.reserve(10);

    std::string input_file = "in" + std::to_string(rank) + ".txt";

    data_read_peer(rank, input_file);

    // Waiting for a signal from tracker
    char ACK = 0;
    MPI_Recv(&ACK, 1, MPI_CHAR, 0, rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    r = pthread_create(&download_thread, NULL, download_thread_func, &rank);
    if (r)
    {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, &rank);
    if (r)
    {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r)
    {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r)
    {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    int numtasks, rank;

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE)
    {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK)
    {
        tracker(numtasks, rank);
    }
    else
    {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
