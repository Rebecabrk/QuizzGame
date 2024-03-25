#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <tuple>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <csignal>
#include "rapidxml.hpp"
#include "src/pugixml.hpp"
#include "Game.h"

using namespace std;
using namespace rapidxml;

#define PORT 2728

int sd;
vector<Game> active_games;
int number_of_games = 0;
vector<int> id_queue;
vector<string> logged_users;
int number_logged = 0;
volatile sig_atomic_t g_ctrlc_pressed = 0;

int createRoom(int fd, char username[]) /* functie care se ocupa de crearea camerelor de joc */
{
    /* facem un joc nou*/
    Game g;
    string user = username;
    g.Add_client(fd, user);
    /* adaugam jocul in lista jocurilor active */
    active_games.push_back(g);
    number_of_games++;
    /* pastram id-ul doar daca este unic*/
    if (number_of_games == 1)
    {
        id_queue.push_back(g.id_uniq);
    }
    else
    {
        bool id_gasit = false;
        bool id_is_good = true;
        while (!id_gasit)
        {
            for (int i = 0; i < number_of_games; i++)
            {
                if (g.id_uniq == id_queue[i])
                {
                    id_is_good = false;
                    break;
                }
            }
            if (id_is_good == true)
            {
                id_queue.push_back(g.id_uniq);
                id_gasit = true;
            }
            else
            {
                int newId = g.idGenerator();
            }
        }
    }
    /* verificare client adaugat */
    for (int i = 0; i < number_of_games; i++)
    {
        active_games[i].showClients();
    }
    /* returnam id-ul camerei ca sa poate fi partajat clientilor */
    return g.id_uniq;
}

bool joinRoom(int fd, int id, string username) /* functie care se ocupa de adaugarea intr-o camera a clientilor */
{
    /* cautam jocul cu acel id */
    for (int i = 0; i < number_of_games; i++)
    {
        if (active_games[i].id_uniq == id)
        {
            /* adaugarea clientului */
            active_games[i].Add_client(fd, username);
            printf("[Server Thread] Room found!\n");
            return true;
        }
    }
    /* camera inexistenta */
    printf("[Server Thread] Room not found!\n");
    return false;
}

void startGame(int id) /* functie care se ocupa de inceperea efectiva a jocului */
{
    /* cautam jocul dupa id */
    bool game_found = false;
    for (int i = 0; i < number_of_games; i++)
    {
        if (active_games[i].id_uniq == id)
        {
            /* incepem acest joc */
            game_found = true;
            active_games[i].StartGame();
        }
    }
    /* camera inexistenta */
    if (!game_found)
    {
        printf("[Server Thread] Game not found!\n");
    }
}

char *conv_addr(struct sockaddr_in address) /* functie care converteste adresa IP a clientului in sir de caractere */
{
    static char str[25];
    char port[7];
    strcpy(str, inet_ntoa(address.sin_addr));
    bzero(port, 7);
    sprintf(port, ":%d", ntohs(address.sin_port));
    strcat(str, port);
    return (str);
}

int welcomeMessage(int fd) /* functie care trimite mesajul cu primii pasi: login/signup */
{
    char welcome[300];

    bzero(welcome, 300);
    strcat(welcome, "\033[1;34mBine ati venit la QUIZZGAME!\033[1;0m\n");
    strcat(welcome, "Logarea se face prin: \033[1;34m login : <username>\033[1;0m\n");
    strcat(welcome, "Sau, daca sunteti nou: \033[1;34m signup : <username>\033[1;0m\n");
    welcome[strlen(welcome)] = '\0';
    if (write(fd, welcome, strlen(welcome)) < 0)
    {
        perror("[Server] Error at write() to client.\n");
        return 0;
    }
    return 1;
}

void *handleClient(void *arg) /* functia fiecarui thread */
{
    int client = *((int *)arg); // fd client
    struct sockaddr_in from;
    socklen_t len = sizeof(from);
    char command[265];
    char command_response[255];
    char username[255];
    bool user_found = false;
    bool login_ok = false;
    bool already_logged = false;

    if (getpeername(client, (struct sockaddr *)&from, &len) == -1)
    {
        perror("[Server Thread] Error at getpeername().\n");
        close(client);
        pthread_exit(NULL);
    }

    printf("[Server Thread] Waiting for the first command...\n");
    fflush(stdout);

    /* pregatire citire users.xml */
    xml_document<> doc;
    xml_node<> *root_node;

    ifstream theFile("users.xml");
    vector<char> buffer((istreambuf_iterator<char>(theFile)), istreambuf_iterator<char>());
    buffer.push_back('\0');

    doc.parse<0>(&buffer[0]);

    root_node = doc.first_node("UsersList");

    /* cazuri de comenzi (login/signup) */
    do
    {
        user_found = false;
        already_logged = false;
        bzero(command_response, 255);
        if (read(client, command, sizeof(command)) <= 0)
        {
            perror("[Thread] Error at read() from client.\n");
            pthread_exit(NULL);
        }

        if (strcmp(command, "disconnect") == 0)
        {
            cout<<"[Server Thread] Client with fd "<< client << " disconnected from the QUIZZGAME!"<<endl;
            pthread_exit(NULL);
        }

        command[strcspn(command, "\n")] = '\0';
        printf("[Server Thread] The command was read: %s\n", command);

        /* login */
        if (strstr(command, "login") != NULL)
        {
            /* care este username-ul ?*/
            int i = 8;
            int n2 = strlen(command);
            while (i < n2)
            {
                username[i - 8] = command[i];
                i++;
            }
            username[i - 8] = '\0';
            printf("[Server Thread] User:%s\n", username);
            /* este acest username deja logat? */
            if (number_logged != 0)
            {
                for (int i = 0; i < number_logged; i++)
                {
                    if (logged_users[i] == username)
                    {
                        /* username-ul este deja logat, nu e ok */
                        already_logged = true;
                        printf("[Server Thread] User:%s is already logged in.\n", username);
                        break;
                    }
                }
            }
            /* nu este deja logat, e ok */
            if (already_logged == false)
            {
                /* cautam username */
                for (xml_node<> *user = root_node->first_node("user"); user && !user_found; user = user->next_sibling())
                {
                    if (strcmp(user->value(), username) == 0)
                    {
                        /* username-ul exista, totul e ok */
                        printf("[Server Thread] User found!\n");
                        strcat(command_response, "Logare cu succes!\n");
                        user_found = true;
                        login_ok = true;
                        logged_users.push_back(username);
                        number_logged++;
                    }
                }
                /* username-ul nu exista, nu e ok */
                if (user_found == false)
                {
                    printf("[Server Thread] User not found!\n");
                    strcat(command_response, "Logare fara succes! User-ul nu exista.\n");
                    user_found = false;
                    login_ok = false;
                }
            }
            else
            {
                printf("[Server Thread] Try again!\n");
                strcat(command_response, "User-ul este deja logat.\n");
            }
        }
        /* signup */
        else if (strstr(command, "signup") != NULL)
        {
            /* care este username-ul? */
            int i = 9;
            int n2 = strlen(command);
            while (i < n2)
            {
                username[i - 9] = command[i];
                i++;
            }
            username[i - 9] = '\0';
            printf("[Server Thread] User:%s\n", username);
            /* cautam daca userul este deja logat */
            if (number_logged != 0)
            {
                for (int i = 0; i < number_logged; i++)
                {
                    if (logged_users[i] == username)
                    {
                        /* este deja logat, nu e ok */
                        already_logged = true;
                        printf("[Server Thread] User:%s is already logged in.\n", username);
                        break;
                    }
                }
            }
            /* nu este deja logat, e ok */
            if (already_logged == false)
            {
                for (xml_node<> *user = root_node->first_node("user"); user && !user_found; user = user->next_sibling())
                {
                    if (strcmp(user->value(), username) == 0)
                    {
                        /* acest username e luat */
                        printf("[Server Thread] User found, but cannot duplicate name!\n");
                        strcat(command_response, "Inregistrare fara succes! Username-ul este luat deja.\n");
                        user_found = true;
                        login_ok = false;
                    }
                }
                /* username-ul nu exista, e ok */
                if (user_found == false)
                {
                    /* adaugam username in users.xml */
                    printf("[Server Thread] User not found, adding the user to the DOM tree...\n");
                    strcat(command_response, "Inregistrare cu succes!\n");
                    /* pregatim fisierul users.xml */
                    pugi::xml_document doc;
                    if (!doc.load_file("users.xml"))
                    {
                        cout << "[Server Thread] Could not load XML file." << endl;
                    }
                    pugi::xml_node usersList = doc.child("UsersList");
                    pugi::xml_node newUser = usersList.append_child("user");
                    newUser.append_child(pugi::node_pcdata).set_value(username);
                    if (!doc.save_file("users.xml"))
                    {
                        cout << "[Server Thread]Could not save XML file." << endl;
                    }
                    user_found = false;
                    login_ok = true;
                    logged_users.push_back(username);
                    number_logged++;
                }
            }
            /* user-ul este deja logat, nu e ok */
            else
            {
                printf("[Server Thread] Try again!\n");
                strcat(command_response, "User-ul este deja logat.\n");
            }
        }
        /* daca totul a mers ok, trecem la urmatorul pas: rooms */
        if (strcmp(command_response, "Inregistrare cu succes!\n") == 0 || strcmp(command_response, "Logare cu succes!\n") == 0)
        {
            strcat(command_response, "\nDoriti sa creati o noua camera de joc? [Y/N]");
        }
        /* trimitem raspuns login/signup*/
        if (write(client, command_response, strlen(command_response)) < 0)
        {
            perror("[Server Thread] Error at write() to client.\n");
        }
    } while (login_ok == false);
    theFile.close();
    printf("[Server Thread] Waiting for an answer regarding rooms from client: %d!\n", client);

    /* y/n? */
    bzero(command_response, 255);
    if (read(client, command_response, sizeof(command_response)) < 0)
    {
        perror("[Thread] Error at read() from client.\n");
    }
    if (strcmp(command_response, "disconnect") == 0)
    {
        cout<<"[Server Thread] Client with fd "<< client << " disconnected from the QUIZZGAME!"<<endl;
        pthread_exit(NULL);
    }
    printf("[Server Thread] The answer is: %s", command_response);

    /* cazuri de raspuns */
    if (strstr(command_response, "y") || strstr(command_response, "Y"))
    {
        /* camera noua */
        int check = createRoom(client, username);
        if (check == -1)
        {
            /* camera nu s-a creat */
            perror("[Server Thread] Error at creating a game for client. Room is not created.\n");
            bzero(command_response, 255);
            strcat(command_response, "Camera nu s-a putut crea! Ne pare rau, reveniti mai tarziu.");
            command_response[strlen(command_response)] = '\0';
            if (write(client, command_response, strlen(command_response)) < 0)
            {
                perror("[Server Thread] Error at write() to client.\n");
            }
        }
        else
        {
            /* camera s-a creat */
            printf("[Server Thread] Room with id: %d was created!\n", check);
            bzero(command_response, 255);
            snprintf(command_response, sizeof(command_response), "Camera s-a creat! Id-ul este: \033[1;34m %d\033[1;0m.\nTastati <start> cand toata lumea e gata!", check);

            /* trimitere room created */
            if (write(client, command_response, strlen(command_response)) < 0)
            {
                perror("[Server Thread] Error at write() to client.\n");
            }

            /* asteptam semnal de start */
            char start[11];
            if (read(client, start, sizeof(start)) < 0)
            {
                perror("[Server Thread] Error at read() from client.\n");
            }
            if (strcmp(start, "disconnect") == 0)
            {
                for (int i = 0; i < number_of_games; i++)
                {
                    if (active_games[i].id_uniq == check)
                    {
                        /* stergerea clientului */
                        active_games[i].Remove_client(client);
                        cout<<"[Server Thread] Client with fd "<< client << " disconnected from the QUIZZGAME!"<<endl;
                    }
                }
                pthread_exit(NULL);
            }
            else if (strstr(start, "start"))
            {
                printf("[Server Thread] Starting the game...\n");
                startGame(check);
            }
            else if (strstr(start, "stop"))
            {
                printf("[Server Thread] Did not start the game");
                pthread_exit(NULL);
            }
        }
        printf("[Server Thread] We are back here\n");
        /* stergerea jocului din vector, intrucat s-a terminat */
                for (int i = 0; i < number_of_games; i++)
                {
                    if (active_games[i].id_uniq == check)
                    {
                        active_games.erase(active_games.begin() + i);
                        number_of_games--;
                        printf("[Server Thread] The game has ended... Erasing it from the vector of active games\n");
                    }
                }
        printf("[Server Thread] We are back here\n");
    }
    else if (strstr(command_response, "n") || strstr(command_response, "N"))
    {
        /* asteptam id-ul camerei */
        bzero(command_response, 255);
        strcat(command_response, "Introduceti un id pentru o camera.");
        if (write(client, command_response, strlen(command_response)) < 0)
        {
            perror("[Server Thread] Error at write() to client.\n");
        }
        /* citim id-ul camerei */
        bzero(command_response, 255);
        if (read(client, command_response, sizeof(command_response)) < 0)
        {
            perror("[Server Thread] Error at read() from client.\n");
        }
        if (strcmp(command_response, "disconnect") == 0)
        {
            cout<<"[Server Thread] Client with fd "<< client << " disconnected from the QUIZZGAME!"<<endl;
            pthread_exit(NULL);
        }
        /* cautam camera dupa id-ul dat */
        int id = atoi(command_response);
        printf("[Server Thread] Id received: %d\n", id);
        string user = username;
        bool check = joinRoom(client, id, user);
        if (check == false)
        {
            /* nu exista joc cu id-ul asta */
            printf("[Server Thread] No game with this id: %d", id);
        }
        else
        {
            /* am gasit camera, totul e ok */
            printf("[Server Thread] The client: %d joined the room: %d!\n", client, id);
            bzero(command_response, 255);
            strcat(command_response, "Sunteti in camera! Asteptati ca masterul camerei sa inceapa QUIZZGAME!\n");
            if (write(client, command_response, strlen(command_response)) < 0)
            {
                perror("[Server Thread] Error at write() to client.\n");
            }
        }
        printf("[Server Thread] We are back here\n");
        /* stergerea jocului din vector, intrucat s-a terminat */
                for (int i = 0; i < number_of_games; i++)
                {
                    if (active_games[i].id_uniq == check)
                    {
                        active_games.erase(active_games.begin() + i);
                        number_of_games--;
                        printf("[Server Thread] The game has ended... Erasing it from the vector of active games\n");
                    }
                }
        printf("[Server Thread] We are back here\n");
    }
    /* terminare thread */
    pthread_exit(NULL);
}

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    fd_set readfds, actfds;
    struct timeval tv;
    int client, optval = 1, fd, nfds;
    socklen_t len;

    /* creare socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[Server] Error at socket().\n");
        return 0;
    }
    /* setare REUSEADDR */
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    /* completare structuri */
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    /* atasam socket-ul */
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[Server] Error at bind().\n");
        return errno;
    }
    /* punem serverul sa asculte */
    if (listen(sd, 5) == -1)
    {
        perror("[Server] Error at listen().\n");
        return errno;
    }

    /* pregatire sets pentru select */
    FD_ZERO(&actfds);
    FD_SET(sd, &actfds);

    /* pregatire timeval */
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    nfds = sd;

    printf("[Server] Waiting at port %d...\n", PORT);
    fflush(stdout);

    /* acceptare si tratare clienti */
    while (1)
    {
        bcopy((char *)&actfds, (char *)&readfds, sizeof(readfds));
        if (select(nfds + 1, &readfds, NULL, NULL, &tv) < 0)
        {
            perror("[Server] Error at select().\n");
            return errno;
        }

        if (FD_ISSET(sd, &readfds))
        {
            len = sizeof(from);
            bzero(&from, sizeof(from));
            /* acceptam un client */
            client = accept(sd, (struct sockaddr *)&from, &len);
            printf("[Server] The client with fd %d, from adress %s connected to the QUIZZGAME\n", client, conv_addr(from));

            /* trimitem mesaj de bunvenit */
            welcomeMessage(client);

            if (client < 0)
            {
                perror("[Server] Error at accept().\n");
                return 0;
            }

            if (nfds < client)
                nfds = client;

            FD_SET(client, &actfds);

            /* creare thread nou */
            pthread_t tid;
            if (pthread_create(&tid, NULL, handleClient, (void *)&client) != 0)
            {
                perror("[server] Eroare la crearea thread-ului.\n");
                close(client);
            }
            pthread_detach(tid);
        }
    } /* while */
    close(sd);
    return 0;
}
